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

	if (notify && !connection_obj->close_notified && !Z_ISUNDEF(intern->on_close)) {
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

	if (connection_obj->close_notified || Z_ISUNDEF(intern->on_close)) {
		return true;
	}

	connection_obj->close_notified = true;
	ZVAL_COPY_VALUE(&params[0], connection);
	ok = websocket_server_call_handler(&intern->on_close, 1, params);

	return ok;
}

static void websocket_server_release_connection(websocket_server_object *intern, zval *connection)
{
	if (!EG(exception) && Z_ISUNDEF(intern->reusable_connection) && GC_REFCOUNT(Z_OBJ_P(connection)) == 1) {
		ZVAL_COPY_VALUE(&intern->reusable_connection, connection);
		return;
	}

	zval_ptr_dtor(connection);
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

static bool websocket_server_accept_connection(websocket_server_object *intern)
{
	const int client_fd = accept(intern->listener_fd, NULL, NULL);
	zval connection;
	websocket_connection_object *connection_obj;
	bool close_requested;
	bool ok;
	const bool needs_connection = Z_ISUNDEF(intern->on_open) || intern->on_open_param_count > 0;

	if (client_fd < 0) {
		return false;
	}
	errno = 0;

	if (!needs_connection) {
		ok = websocket_server_call_open_handler(intern, NULL, &close_requested);
		if (!ok || close_requested) {
			websocket_server_close_fd(client_fd);
			return ok;
		}
	}

	websocket_server_create_connection_zval(intern, &connection);
	connection_obj = Z_WEBSOCKET_CONNECTION_P(&connection);
	websocket_connection_open(connection_obj, WEBSOCKET_G(next_connection_id)++, NULL, 0, client_fd);

	close_requested = false;
	if (needs_connection) {
		connection_obj->defer_close = true;
		ok = websocket_server_call_open_handler(intern, &connection, &close_requested);
		connection_obj->defer_close = false;
	} else {
		ok = true;
	}

	if (!ok) {
		websocket_connection_close_socket(connection_obj);
		zval_ptr_dtor(&connection);
		return false;
	}

	if (close_requested) {
		connection_obj->open = false;
	}

	if (!connection_obj->open) {
		ok = websocket_server_notify_connection_closed(intern, &connection, connection_obj);
		websocket_connection_close_socket(connection_obj);
		websocket_server_release_connection(intern, &connection);
		return ok;
	}

	if (websocket_server_set_nonblocking(client_fd) == FAILURE) {
		websocket_connection_close_socket(connection_obj);
		zval_ptr_dtor(&connection);
		zend_throw_error(NULL, "Cannot make accepted WebSocket connection non-blocking: %s", strerror(errno));
		return false;
	}

	if (!websocket_server_ensure_connection_capacity(intern)) {
		websocket_connection_close_socket(connection_obj);
		zval_ptr_dtor(&connection);
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
		int ready;

		ready = WEBSOCKET_G(driver)->wait(WEBSOCKET_LOOP_TIMEOUT_USEC);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}

			zend_throw_error(NULL, "WebSocket server loop failed: %s", strerror(errno));
			break;
		}

		if (ready > 0) {
			if (!websocket_server_accept_pending(intern)) {
				break;
			}
		}

		if (!websocket_server_purge_closed_connections(intern)) {
			break;
		}
	}

	websocket_server_runtime_close(intern, !EG(exception));

	return !EG(exception);
}
