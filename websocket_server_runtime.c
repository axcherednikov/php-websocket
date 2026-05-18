#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_websocket.h"
#include "php_websocket_compat.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define WEBSOCKET_LISTEN_BACKLOG 1024
#define WEBSOCKET_ACCEPT_BATCH_LIMIT 1024
#define WEBSOCKET_LOOP_TIMEOUT_USEC 100000
#define WEBSOCKET_READ_CHUNK_SIZE 4096

static const char websocket_bad_request_response[] =
	"HTTP/1.1 400 Bad Request\r\n"
	"Connection: close\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

static void websocket_server_close_fd(const int fd)
{
	if (fd >= 0) {
		while (close(fd) < 0 && errno == EINTR) {
		}
	}
}

static int websocket_server_set_nonblocking(const int fd)
{
	const int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) {
		return FAILURE;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return FAILURE;
	}

	return SUCCESS;
}

static bool websocket_server_call_handler(zval *handler, uint32_t param_count, zval *params)
{
	zval retval;

	if (Z_ISUNDEF_P(handler)) {
		return true;
	}

	ZVAL_UNDEF(&retval);
	if (call_user_function(EG(function_table), NULL, handler, &retval, param_count, params) == FAILURE) {
		zend_throw_error(NULL, "Failed to call WebSocket server handler");
		return false;
	}

	if (!Z_ISUNDEF(retval)) {
		zval_ptr_dtor(&retval);
	}

	return !EG(exception);
}

static bool websocket_server_call_open_handler(websocket_server_object *intern, zval *connection, bool *close_requested)
{
	zval retval;
	zval params[1];
	zval *call_params = NULL;
	uint32_t param_count = 0;

	*close_requested = false;

	if (Z_ISUNDEF(intern->on_open)) {
		return true;
	}

	if (connection) {
		ZVAL_COPY_VALUE(&params[0], connection);
		call_params = params;
		param_count = 1;
	}

	ZVAL_UNDEF(&retval);
	if (intern->on_open_cache_initialized) {
		websocket_call_known_fcc(&intern->on_open_cache, &retval, param_count, call_params);
	} else if (call_user_function(EG(function_table), NULL, &intern->on_open, &retval, param_count, call_params) == FAILURE) {
		zend_throw_error(NULL, "Failed to call WebSocket server handler");
		return false;
	}

	if (!Z_ISUNDEF(retval)) {
		*close_requested = Z_TYPE(retval) == IS_FALSE;
		zval_ptr_dtor(&retval);
	}

	return !EG(exception);
}

static int websocket_server_create_listener(websocket_server_object *intern)
{
	struct addrinfo hints;
	struct addrinfo *addresses = NULL;
	struct addrinfo *address;
	char port[16];
	int listener_fd = -1;
	int last_errno = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	snprintf(port, sizeof(port), "%ld", (long) intern->port);

	const int error = getaddrinfo(ZSTR_VAL(intern->host), port, &hints, &addresses);
	if (error != 0) {
		zend_throw_error(NULL, "Cannot resolve WebSocket listen address %s:%ld: %s", ZSTR_VAL(intern->host), (long) intern->port, gai_strerror(error));
		return -1;
	}

	for (address = addresses; address != NULL; address = address->ai_next) {
		const int reuse = 1;

		listener_fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (listener_fd < 0) {
			last_errno = errno;
			continue;
		}

		(void) setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse, sizeof(reuse));

		if (websocket_server_set_nonblocking(listener_fd) == FAILURE) {
			last_errno = errno;
			websocket_server_close_fd(listener_fd);
			listener_fd = -1;
			continue;
		}

		if (bind(listener_fd, address->ai_addr, address->ai_addrlen) < 0) {
			last_errno = errno;
			websocket_server_close_fd(listener_fd);
			listener_fd = -1;
			continue;
		}

		if (listen(listener_fd, WEBSOCKET_LISTEN_BACKLOG) < 0) {
			last_errno = errno;
			websocket_server_close_fd(listener_fd);
			listener_fd = -1;
			continue;
		}

		break;
	}

	freeaddrinfo(addresses);

	if (listener_fd < 0) {
		zend_throw_error(NULL, "Cannot listen on %s:%ld: %s", ZSTR_VAL(intern->host), (long) intern->port, strerror(last_errno ? last_errno : errno));
	}

	return listener_fd;
}

static bool websocket_server_ensure_connection_capacity(websocket_server_object *intern)
{
	if (intern->connection_count < intern->connection_capacity) {
		return true;
	}

	if (intern->connection_capacity == 0) {
		intern->connection_capacity = 8;
	} else {
		intern->connection_capacity *= 2;
	}

	intern->connections = erealloc(intern->connections, sizeof(zval) * intern->connection_capacity);

	return true;
}

static bool websocket_server_close_connection_at(websocket_server_object *intern, size_t index, bool notify)
{
	zval connection;
	websocket_connection_object *connection_obj;
	bool ok = true;

	ZVAL_COPY_VALUE(&connection, &intern->connections[index]);

	if (index + 1 < intern->connection_count) {
		memmove(&intern->connections[index], &intern->connections[index + 1], sizeof(zval) * (intern->connection_count - index - 1));
	}
	intern->connection_count--;

	connection_obj = Z_WEBSOCKET_CONNECTION_P(&connection);
	connection_obj->open = false;
	if (WEBSOCKET_G(driver) && connection_obj->fd >= 0) {
		WEBSOCKET_G(driver)->unwatch(connection_obj->fd);
	}

	if (notify && connection_obj->upgraded && !connection_obj->close_notified && !Z_ISUNDEF(intern->on_close)) {
		zval params[1];

		connection_obj->close_notified = true;
		ZVAL_COPY_VALUE(&params[0], &connection);
		ok = websocket_server_call_handler(&intern->on_close, 1, params);
	}

	websocket_connection_close_socket(connection_obj);
	zval_ptr_dtor(&connection);

	return ok;
}

static bool websocket_server_purge_closed_connections(websocket_server_object *intern)
{
	size_t i = 0;

	while (i < intern->connection_count) {
		websocket_connection_object *connection_obj = Z_WEBSOCKET_CONNECTION_P(&intern->connections[i]);

		if (!connection_obj->open) {
			if (!websocket_server_close_connection_at(intern, i, true)) {
				return false;
			}
			continue;
		}

		i++;
	}

	return true;
}

static bool websocket_server_close_all_connections(websocket_server_object *intern, bool notify)
{
	while (intern->connection_count > 0) {
		if (!websocket_server_close_connection_at(intern, intern->connection_count - 1, notify)) {
			return false;
		}
	}

	return true;
}

static bool websocket_server_notify_connection_closed(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj)
{
	zval params[1];
	bool ok;

	if (!connection_obj->upgraded || connection_obj->close_notified || Z_ISUNDEF(intern->on_close)) {
		return true;
	}

	connection_obj->close_notified = true;
	ZVAL_COPY_VALUE(&params[0], connection);
	ok = websocket_server_call_handler(&intern->on_close, 1, params);

	return ok;
}

static void websocket_server_create_connection_zval(websocket_server_object *intern, zval *connection)
{
	if (!Z_ISUNDEF(intern->reusable_connection)) {
		ZVAL_COPY_VALUE(connection, &intern->reusable_connection);
		ZVAL_UNDEF(&intern->reusable_connection);
		return;
	}

	ZVAL_OBJ(connection, websocket_connection_ce->create_object(websocket_connection_ce));
}

static bool websocket_connection_ensure_read_capacity(websocket_connection_object *connection_obj, const size_t append_len)
{
	size_t needed;
	size_t capacity;

	if (append_len > WEBSOCKET_HTTP_MAX_REQUEST_SIZE || connection_obj->read_buffer_len > WEBSOCKET_HTTP_MAX_REQUEST_SIZE - append_len) {
		return false;
	}

	needed = connection_obj->read_buffer_len + append_len;
	if (needed <= connection_obj->read_buffer_capacity) {
		return true;
	}

	capacity = connection_obj->read_buffer_capacity > 0 ? connection_obj->read_buffer_capacity : 1024;
	while (capacity < needed) {
		capacity *= 2;
	}

	if (capacity > WEBSOCKET_HTTP_MAX_REQUEST_SIZE) {
		capacity = WEBSOCKET_HTTP_MAX_REQUEST_SIZE;
	}

	if (connection_obj->read_buffer) {
		connection_obj->read_buffer = erealloc(connection_obj->read_buffer, capacity);
	} else {
		connection_obj->read_buffer = emalloc(capacity);
	}
	connection_obj->read_buffer_capacity = capacity;

	return true;
}

static bool websocket_server_send_bytes(const int fd, const char *buffer, const size_t len)
{
	size_t sent = 0;

	while (sent < len) {
		ssize_t written;

#ifdef MSG_NOSIGNAL
		written = send(fd, buffer + sent, len - sent, MSG_NOSIGNAL);
#else
		written = send(fd, buffer + sent, len - sent, 0);
#endif
		if (written > 0) {
			sent += (size_t) written;
			continue;
		}

		if (written < 0 && errno == EINTR) {
			continue;
		}

		return false;
	}

	return true;
}

static void websocket_connection_discard_read_bytes(websocket_connection_object *connection_obj, const size_t bytes)
{
	if (bytes >= connection_obj->read_buffer_len) {
		connection_obj->read_buffer_len = 0;
		return;
	}

	memmove(connection_obj->read_buffer, connection_obj->read_buffer + bytes, connection_obj->read_buffer_len - bytes);
	connection_obj->read_buffer_len -= bytes;
}

static bool websocket_server_finish_upgrade(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj, zend_string *accept_key, const size_t bytes_consumed)
{
	zend_string *response;
	bool close_requested = false;
	bool ok;
	const bool needs_connection = Z_ISUNDEF(intern->on_open) || intern->on_open_param_count > 0;

	response = websocket_http_upgrade_response(accept_key);
	ok = websocket_server_send_bytes(connection_obj->fd, ZSTR_VAL(response), ZSTR_LEN(response));
	zend_string_release(response);

	if (!ok) {
		connection_obj->open = false;
		return true;
	}

	connection_obj->upgraded = true;
	websocket_connection_discard_read_bytes(connection_obj, bytes_consumed);
	if (WEBSOCKET_G(driver) && connection_obj->fd >= 0) {
		WEBSOCKET_G(driver)->unwatch(connection_obj->fd);
	}

	if (needs_connection) {
		connection_obj->defer_close = true;
		ok = websocket_server_call_open_handler(intern, connection, &close_requested);
		connection_obj->defer_close = false;
	} else {
		ok = websocket_server_call_open_handler(intern, NULL, &close_requested);
	}

	if (!ok) {
		connection_obj->open = false;
		return false;
	}

	if (close_requested) {
		connection_obj->open = false;
	}

	if (!connection_obj->open) {
		return websocket_server_notify_connection_closed(intern, connection, connection_obj);
	}

	return true;
}

static bool websocket_server_process_handshake(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj)
{
	char chunk[WEBSOCKET_READ_CHUNK_SIZE];

	while (connection_obj->open && !connection_obj->upgraded) {
		const ssize_t bytes_read = recv(connection_obj->fd, chunk, sizeof(chunk), 0);

		if (bytes_read > 0) {
			zend_string *accept_key = NULL;
			size_t bytes_consumed = 0;
			websocket_http_upgrade_result result;

			if (!websocket_connection_ensure_read_capacity(connection_obj, (size_t) bytes_read)) {
				(void) websocket_server_send_bytes(connection_obj->fd, websocket_bad_request_response, sizeof(websocket_bad_request_response) - 1);
				connection_obj->open = false;
				return true;
			}

			memcpy(connection_obj->read_buffer + connection_obj->read_buffer_len, chunk, (size_t) bytes_read);
			connection_obj->read_buffer_len += (size_t) bytes_read;

			result = websocket_http_parse_upgrade(connection_obj->read_buffer, connection_obj->read_buffer_len, &accept_key, &bytes_consumed);
			if (result == WEBSOCKET_HTTP_UPGRADE_INCOMPLETE) {
				continue;
			}

			if (result == WEBSOCKET_HTTP_UPGRADE_INVALID) {
				(void) websocket_server_send_bytes(connection_obj->fd, websocket_bad_request_response, sizeof(websocket_bad_request_response) - 1);
				connection_obj->open = false;
				return true;
			}

			if (!websocket_server_finish_upgrade(intern, connection, connection_obj, accept_key, bytes_consumed)) {
				zend_string_release(accept_key);
				return false;
			}

			zend_string_release(accept_key);
			return true;
		}

		if (bytes_read == 0) {
			connection_obj->open = false;
			return true;
		}

		if (errno == EINTR) {
			continue;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return true;
		}

		connection_obj->open = false;
		return true;
	}

	return true;
}

static bool websocket_server_accept_connection(websocket_server_object *intern)
{
	struct sockaddr_storage remote_addr;
	socklen_t remote_addr_len = sizeof(remote_addr);
	const int client_fd = accept(intern->listener_fd, (struct sockaddr *) &remote_addr, &remote_addr_len);
	zval connection;
	websocket_connection_object *connection_obj;

	if (client_fd < 0) {
		return false;
	}
	errno = 0;

	if (websocket_server_set_nonblocking(client_fd) == FAILURE) {
		websocket_server_close_fd(client_fd);
		zend_throw_error(NULL, "Cannot make accepted WebSocket connection non-blocking: %s", strerror(errno));
		return false;
	}

	websocket_server_create_connection_zval(intern, &connection);
	connection_obj = Z_WEBSOCKET_CONNECTION_P(&connection);
	websocket_connection_open(connection_obj, WEBSOCKET_G(next_connection_id)++, (const struct sockaddr *) &remote_addr, remote_addr_len, client_fd);

	if (!websocket_server_ensure_connection_capacity(intern)) {
		websocket_connection_close_socket(connection_obj);
		zval_ptr_dtor(&connection);
		return false;
	}

	if (WEBSOCKET_G(driver)->watch_read(client_fd) == FAILURE) {
		websocket_connection_close_socket(connection_obj);
		zval_ptr_dtor(&connection);
		zend_throw_error(NULL, "Cannot watch accepted WebSocket connection with %s driver: %s", WEBSOCKET_G(driver)->name, strerror(errno));
		return false;
	}

	ZVAL_COPY_VALUE(&intern->connections[intern->connection_count], &connection);
	intern->connection_count++;

	return true;
}

static bool websocket_server_accept_pending(websocket_server_object *intern)
{
	size_t accepted = 0;

	while (intern->running && !WEBSOCKET_G(stopped) && accepted < WEBSOCKET_ACCEPT_BATCH_LIMIT) {
		if (!websocket_server_accept_connection(intern)) {
			if (EG(exception)) {
				return false;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return true;
			}
			if (errno == EINTR) {
				continue;
			}

			return !EG(exception);
		}

		accepted++;
	}

	return true;
}

static bool websocket_server_process_connection_fd(websocket_server_object *intern, const int fd)
{
	size_t i;

	for (i = 0; i < intern->connection_count; i++) {
		websocket_connection_object *connection_obj = Z_WEBSOCKET_CONNECTION_P(&intern->connections[i]);

		if (connection_obj->fd != fd) {
			continue;
		}

		if (!connection_obj->open || connection_obj->upgraded) {
			return true;
		}

		return websocket_server_process_handshake(intern, &intern->connections[i], connection_obj);
	}

	return true;
}

static bool websocket_server_process_connections(websocket_server_object *intern)
{
	size_t i;

	for (i = 0; i < intern->connection_count; i++) {
		websocket_connection_object *connection_obj = Z_WEBSOCKET_CONNECTION_P(&intern->connections[i]);

		if (!connection_obj->open || connection_obj->upgraded) {
			continue;
		}

		if (!websocket_server_process_handshake(intern, &intern->connections[i], connection_obj)) {
			return false;
		}
	}

	return true;
}

void websocket_server_runtime_close(websocket_server_object *intern, bool notify)
{
	if (intern->listener_fd >= 0) {
		if (WEBSOCKET_G(driver)) {
			WEBSOCKET_G(driver)->unwatch(intern->listener_fd);
		}
		websocket_server_close_fd(intern->listener_fd);
		intern->listener_fd = -1;
	}

	if (!websocket_server_close_all_connections(intern, notify)) {
		(void) websocket_server_close_all_connections(intern, false);
	}
}

void websocket_server_runtime_free(websocket_server_object *intern)
{
	websocket_server_runtime_close(intern, false);

	if (!Z_ISUNDEF(intern->reusable_connection)) {
		zval_ptr_dtor(&intern->reusable_connection);
		ZVAL_UNDEF(&intern->reusable_connection);
	}

	if (intern->connections) {
		efree(intern->connections);
		intern->connections = NULL;
	}

	intern->connection_count = 0;
	intern->connection_capacity = 0;
}

bool websocket_server_runtime_run(websocket_server_object *intern)
{
	intern->listener_fd = websocket_server_create_listener(intern);
	if (intern->listener_fd < 0) {
		return false;
	}

	if (WEBSOCKET_G(driver)->watch_read(intern->listener_fd) == FAILURE) {
		zend_throw_error(NULL, "Cannot watch WebSocket listener with %s driver: %s", WEBSOCKET_G(driver)->name, strerror(errno));
		websocket_server_runtime_close(intern, false);
		return false;
	}

	while (intern->running && !WEBSOCKET_G(stopped)) {
		int ready_fd = -1;
		const int ready = WEBSOCKET_G(driver)->wait(WEBSOCKET_LOOP_TIMEOUT_USEC, &ready_fd);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}

			zend_throw_error(NULL, "WebSocket server loop failed: %s", strerror(errno));
			break;
		}

		if (ready > 0 && ready_fd == intern->listener_fd) {
			if (!websocket_server_accept_pending(intern)) {
				break;
			}
		} else if (ready > 0 && ready_fd >= 0) {
			if (!websocket_server_process_connection_fd(intern, ready_fd)) {
				break;
			}
		}

		if (!websocket_server_process_connections(intern)) {
			break;
		}

		if (!websocket_server_purge_closed_connections(intern)) {
			break;
		}
	}

	websocket_server_runtime_close(intern, !EG(exception));

	return !EG(exception);
}
