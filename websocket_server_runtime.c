/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_websocket.h"
#include "php_websocket_compat.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_smart_str.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define WEBSOCKET_LISTEN_BACKLOG 1024
#define WEBSOCKET_ACCEPT_BATCH_LIMIT 1024
#define WEBSOCKET_LOOP_TIMEOUT_USEC 100000
#define WEBSOCKET_READ_CHUNK_SIZE 4096

typedef enum _websocket_server_frame_status {
	WEBSOCKET_SERVER_FRAME_INCOMPLETE = 0,
	WEBSOCKET_SERVER_FRAME_OK = 1,
	WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR = 2,
	WEBSOCKET_SERVER_FRAME_MESSAGE_TOO_BIG = 3,
	WEBSOCKET_SERVER_FRAME_INVALID_PAYLOAD = 4,
} websocket_server_frame_status;

typedef struct _websocket_server_frame {
	uint8_t opcode;
	bool final;
	size_t bytes_consumed;
	zend_string *payload;
} websocket_server_frame;

static bool websocket_server_close_with_code(websocket_connection_object *connection_obj, zend_long code, const char *reason);
static bool websocket_server_send_bytes(int fd, const char *buffer, size_t len);
static uint64_t websocket_server_handshake_timeout_usec(websocket_server_object *intern);
static uint64_t websocket_server_idle_timeout_usec(websocket_server_object *intern);

static const char websocket_bad_request_response[] =
	"HTTP/1.1 400 Bad Request\r\n"
	"Connection: close\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

static const char websocket_service_unavailable_response[] =
	"HTTP/1.1 503 Service Unavailable\r\n"
	"Connection: close\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

static const char websocket_forbidden_response[] =
	"HTTP/1.1 403 Forbidden\r\n"
	"Connection: close\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

static uint64_t websocket_server_now_usec(void)
{
#ifdef CLOCK_MONOTONIC
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		return ((uint64_t) ts.tv_sec * 1000000u) + ((uint64_t) ts.tv_nsec / 1000u);
	}
#endif
	{
		struct timeval tv;

		if (gettimeofday(&tv, NULL) == 0) {
			return ((uint64_t) tv.tv_sec * 1000000u) + (uint64_t) tv.tv_usec;
		}
	}

	return 0;
}

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

static const char *websocket_http_reason_phrase(const zend_long status)
{
	switch (status) {
		case 200:
			return "OK";
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 429:
			return "Too Many Requests";
		case 500:
			return "Internal Server Error";
		case 503:
			return "Service Unavailable";
		default:
			return "Rejected";
	}
}

static bool websocket_server_send_handshake_response(const int fd, zval *response)
{
	zval *status_zv;
	zval *headers_zv;
	zval *body_zv;
	zend_long status;
	zend_string *body;
	zend_string *name;
	zval *value;
	bool has_connection = false;
	bool has_content_length = false;
	smart_str buffer = {0};
	bool ok;

	status_zv = zend_read_property(websocket_handshake_response_ce, Z_OBJ_P(response), "status", strlen("status"), 0, NULL);
	headers_zv = zend_read_property(websocket_handshake_response_ce, Z_OBJ_P(response), "headers", strlen("headers"), 0, NULL);
	body_zv = zend_read_property(websocket_handshake_response_ce, Z_OBJ_P(response), "body", strlen("body"), 0, NULL);

	status = zval_get_long(status_zv);
	body = zval_get_string(body_zv);

	smart_str_append_printf(&buffer, "HTTP/1.1 %ld %s\r\n", (long) status, websocket_http_reason_phrase(status));
	if (Z_TYPE_P(headers_zv) == IS_ARRAY) {
		ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(headers_zv), name, value) {
			if (!name || Z_TYPE_P(value) != IS_STRING) {
				continue;
			}
			if (zend_string_equals_literal_ci(name, "Connection")) {
				has_connection = true;
			} else if (zend_string_equals_literal_ci(name, "Content-Length")) {
				has_content_length = true;
			}
			smart_str_append(&buffer, name);
			smart_str_appendl(&buffer, ": ", 2);
			smart_str_append(&buffer, Z_STR_P(value));
			smart_str_appendl(&buffer, "\r\n", 2);
		} ZEND_HASH_FOREACH_END();
	}
	if (!has_connection) {
		smart_str_appendl(&buffer, "Connection: close\r\n", strlen("Connection: close\r\n"));
	}
	if (!has_content_length) {
		smart_str_append_printf(&buffer, "Content-Length: %zu\r\n", ZSTR_LEN(body));
	}
	smart_str_appendl(&buffer, "\r\n", 2);
	smart_str_append(&buffer, body);
	smart_str_0(&buffer);

	ok = buffer.s && websocket_server_send_bytes(fd, ZSTR_VAL(buffer.s), ZSTR_LEN(buffer.s));
	if (buffer.s) {
		smart_str_free(&buffer);
	}
	zend_string_release(body);

	return ok;
}

static zend_string *websocket_server_lower_header_name(const char *name, const size_t name_len)
{
	zend_string *header = zend_string_init(name, name_len, false);
	zend_string *lower = zend_string_tolower(header);
	zend_string_release(header);
	return lower;
}

static void websocket_server_add_request_header(zval *headers, const char *name, const size_t name_len, const char *value, const size_t value_len)
{
	zend_string *lower_name = websocket_server_lower_header_name(name, name_len);
	zval *existing = zend_hash_find(Z_ARRVAL_P(headers), lower_name);
	zval header_value;

	if (existing && Z_TYPE_P(existing) == IS_STRING) {
		zend_string *joined = strpprintf(0, "%s, %.*s", Z_STRVAL_P(existing), (int) value_len, value);
		ZVAL_STR(&header_value, joined);
		zend_hash_update(Z_ARRVAL_P(headers), lower_name, &header_value);
	} else {
		ZVAL_STRINGL(&header_value, value, value_len);
		zend_hash_update(Z_ARRVAL_P(headers), lower_name, &header_value);
	}

	zend_string_release(lower_name);
}

static void websocket_server_http_trim(const char **value, size_t *len)
{
	while (*len > 0 && ((*value)[0] == ' ' || (*value)[0] == '\t')) {
		(*value)++;
		(*len)--;
	}

	while (*len > 0 && ((*value)[*len - 1] == ' ' || (*value)[*len - 1] == '\t')) {
		(*len)--;
	}
}

static bool websocket_server_create_request(zval *request, const char *buffer, const size_t bytes_consumed)
{
	const char *header_end = buffer + bytes_consumed - 4;
	const char *request_line_end = memchr(buffer, '\r', bytes_consumed);
	const char *method_end;
	const char *target_start;
	const char *target_end;
	const char *line;
	zval headers;

	if (!request_line_end || request_line_end + 1 >= buffer + bytes_consumed || request_line_end[1] != '\n') {
		return false;
	}

	method_end = memchr(buffer, ' ', (size_t) (request_line_end - buffer));
	if (!method_end) {
		return false;
	}
	target_start = method_end + 1;
	target_end = memchr(target_start, ' ', (size_t) (request_line_end - target_start));
	if (!target_end) {
		return false;
	}

	object_init_ex(request, websocket_request_ce);
	zend_update_property_stringl(websocket_request_ce, Z_OBJ_P(request), "method", strlen("method"), buffer, (size_t) (method_end - buffer));
	zend_update_property_stringl(websocket_request_ce, Z_OBJ_P(request), "target", strlen("target"), target_start, (size_t) (target_end - target_start));

	array_init(&headers);
	line = request_line_end + 2;
	while (line < header_end) {
		const char *line_end = memchr(line, '\r', (size_t) (header_end - line) + 1);
		const char *colon;
		const char *name;
		const char *value;
		size_t line_len;
		size_t name_len;
		size_t value_len;

		if (!line_end || line_end + 1 >= buffer + bytes_consumed || line_end[1] != '\n') {
			zval_ptr_dtor(&headers);
			zval_ptr_dtor(request);
			ZVAL_UNDEF(request);
			return false;
		}

		line_len = (size_t) (line_end - line);
		if (line_len == 0) {
			break;
		}

		colon = memchr(line, ':', line_len);
		if (!colon) {
			zval_ptr_dtor(&headers);
			zval_ptr_dtor(request);
			ZVAL_UNDEF(request);
			return false;
		}

		name = line;
		name_len = (size_t) (colon - line);
		value = colon + 1;
		value_len = line_len - name_len - 1;
		websocket_server_http_trim(&name, &name_len);
		websocket_server_http_trim(&value, &value_len);
		websocket_server_add_request_header(&headers, name, name_len, value, value_len);

		line = line_end + 2;
	}

	zend_update_property(websocket_request_ce, Z_OBJ_P(request), "headers", strlen("headers"), &headers);
	zval_ptr_dtor(&headers);

	return true;
}

static bool websocket_server_handle_handshake_exception(websocket_connection_object *connection_obj, bool *accepted)
{
	zend_object *exception = EG(exception);
	zval exception_zv;
	zval *response;

	if (!exception || !instanceof_function(exception->ce, websocket_handshake_exception_ce)) {
		return false;
	}

	GC_ADDREF(exception);
	zend_clear_exception();

	ZVAL_OBJ(&exception_zv, exception);
	response = zend_read_property(websocket_handshake_exception_ce, Z_OBJ(exception_zv), "response", strlen("response"), 0, NULL);
	if (Z_TYPE_P(response) == IS_OBJECT && instanceof_function(Z_OBJCE_P(response), websocket_handshake_response_ce)) {
		(void) websocket_server_send_handshake_response(connection_obj->fd, response);
	} else {
		(void) websocket_server_send_bytes(connection_obj->fd, websocket_forbidden_response, sizeof(websocket_forbidden_response) - 1);
	}

	connection_obj->open = false;
	*accepted = false;
	zval_ptr_dtor(&exception_zv);

	return true;
}

static bool websocket_server_call_handshake_handler(websocket_server_object *intern, zval *request, websocket_connection_object *connection_obj, bool *accepted)
{
	zval retval;
	zval params[1];

	*accepted = true;

	if (Z_ISUNDEF(intern->on_handshake)) {
		return true;
	}

	ZVAL_COPY(&params[0], request);
	ZVAL_UNDEF(&retval);
	if (call_user_function(EG(function_table), NULL, &intern->on_handshake, &retval, 1, params) == FAILURE) {
		zval_ptr_dtor(&params[0]);
		zend_throw_error(NULL, "Failed to call WebSocket server handshake handler");
		return false;
	}
	zval_ptr_dtor(&params[0]);

	if (EG(exception)) {
		if (websocket_server_handle_handshake_exception(connection_obj, accepted)) {
			if (!Z_ISUNDEF(retval)) {
				zval_ptr_dtor(&retval);
			}
			return true;
		}
		if (!Z_ISUNDEF(retval)) {
			zval_ptr_dtor(&retval);
		}
		return false;
	}

	if (!Z_ISUNDEF(retval) && Z_TYPE(retval) != IS_NULL) {
		zend_type_error("WebSocket handshake handler must return void, %s returned", websocket_zval_value_name(&retval));
		zval_ptr_dtor(&retval);
		return false;
	}

	if (!Z_ISUNDEF(retval)) {
		zval_ptr_dtor(&retval);
	}
	return true;
}

static int websocket_server_create_listener(websocket_server_object *intern)
{
	struct addrinfo hints;
	struct addrinfo *addresses = NULL;
	struct addrinfo *address;
	char port[16];
	int listener_fd = -1;
	int last_errno = 0;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	snprintf(port, sizeof(port), "%ld", (long) intern->port);

	error = getaddrinfo(ZSTR_VAL(intern->host), port, &hints, &addresses);
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

static bool websocket_server_connection_expired(websocket_server_object *intern, websocket_connection_object *connection_obj, const uint64_t now_usec)
{
	uint64_t last_activity_usec;
	uint64_t timeout_usec;

	if (now_usec == 0) {
		return false;
	}

	last_activity_usec = connection_obj->last_activity_usec ? connection_obj->last_activity_usec : connection_obj->accepted_at_usec;
	if (last_activity_usec == 0 || now_usec < last_activity_usec) {
		return false;
	}

	if (!connection_obj->upgraded) {
		timeout_usec = websocket_server_handshake_timeout_usec(intern);
	} else {
		timeout_usec = websocket_server_idle_timeout_usec(intern);
	}

	return timeout_usec > 0 && now_usec - last_activity_usec >= timeout_usec;
}

static void websocket_server_close_expired_connection(websocket_connection_object *connection_obj)
{
	if (connection_obj->upgraded) {
		(void) websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_NORMAL, "idle timeout");
		return;
	}

	websocket_connection_close_socket(connection_obj);
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

static bool websocket_connection_ensure_read_capacity(websocket_connection_object *connection_obj, const size_t append_len, const size_t limit)
{
	size_t needed;
	size_t capacity;

	if (append_len > limit || connection_obj->read_buffer_len > limit - append_len) {
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

	if (capacity > limit) {
		capacity = limit;
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

static size_t websocket_server_positive_option(websocket_server_object *intern, const char *name, const size_t name_len, const size_t fallback)
{
	zval *value;
	zval rv;
	zend_long option_value;

	if (Z_TYPE(intern->options) == IS_ARRAY) {
		value = zend_hash_str_find(Z_ARRVAL(intern->options), name, name_len);
		if (!value) {
			return fallback;
		}
	} else if (Z_TYPE(intern->options) == IS_OBJECT && instanceof_function(Z_OBJCE(intern->options), websocket_server_options_ce)) {
		value = zend_read_property(websocket_server_options_ce, Z_OBJ(intern->options), name, name_len, 0, &rv);
	} else {
		return fallback;
	}

	option_value = zval_get_long(value);
	if (option_value <= 0) {
		return fallback;
	}

	return (size_t) option_value;
}

static size_t websocket_server_max_message_size(websocket_server_object *intern)
{
	return websocket_server_positive_option(intern, "maxMessageSize", strlen("maxMessageSize"), WEBSOCKET_DEFAULT_MAX_MESSAGE_SIZE);
}

static size_t websocket_server_max_queued_bytes(websocket_server_object *intern)
{
	return websocket_server_positive_option(intern, "maxQueuedBytes", strlen("maxQueuedBytes"), WEBSOCKET_DEFAULT_MAX_QUEUED_BYTES);
}

static size_t websocket_server_max_connections(websocket_server_object *intern)
{
	return websocket_server_positive_option(intern, "maxConnections", strlen("maxConnections"), WEBSOCKET_DEFAULT_MAX_CONNECTIONS);
}

static uint64_t websocket_server_handshake_timeout_usec(websocket_server_object *intern)
{
	return (uint64_t) websocket_server_positive_option(intern, "handshakeTimeoutMs", strlen("handshakeTimeoutMs"), WEBSOCKET_DEFAULT_HANDSHAKE_TIMEOUT_MS) * 1000u;
}

static uint64_t websocket_server_idle_timeout_usec(websocket_server_object *intern)
{
	return (uint64_t) websocket_server_positive_option(intern, "idleTimeoutMs", strlen("idleTimeoutMs"), WEBSOCKET_DEFAULT_IDLE_TIMEOUT_MS) * 1000u;
}

static bool websocket_server_close_with_code(websocket_connection_object *connection_obj, const zend_long code, const char *reason)
{
	zend_string *reason_string;
	bool ok;

	reason_string = reason ? zend_string_init(reason, strlen(reason), false) : zend_string_init("", 0, false);
	ok = websocket_connection_send_close_frame(connection_obj, code, reason_string);
	zend_string_release(reason_string);

	if (ok) {
		websocket_connection_close_after_write(connection_obj);
	} else {
		websocket_connection_close_socket(connection_obj);
		ok = true;
	}

	return ok;
}

static zend_always_inline void websocket_server_mask_payload(unsigned char *dst, const unsigned char *src, const size_t len, const uint8_t mask[4])
{
	size_t i;

	for (i = 0; i < len; i++) {
		dst[i] = src[i] ^ mask[i & 3];
	}
}

static websocket_server_frame_status websocket_server_parse_frame(websocket_connection_object *connection_obj, const size_t max_message_size, websocket_server_frame *frame)
{
	const unsigned char *in = (const unsigned char *) connection_obj->read_buffer;
	const size_t len = connection_obj->read_buffer_len;
	size_t pos = 2;
	uint8_t b0;
	uint8_t b1;
	uint64_t payload_len;
	uint8_t mask[4];
	size_t i;

	memset(frame, 0, sizeof(*frame));

	if (len < 2) {
		return WEBSOCKET_SERVER_FRAME_INCOMPLETE;
	}

	b0 = in[0];
	b1 = in[1];
	frame->final = (b0 & 0x80) != 0;
	frame->opcode = b0 & 0x0f;
	payload_len = b1 & 0x7f;

	if ((b0 & 0x70) != 0 || (b1 & 0x80) == 0) {
		return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
	}

	if (!websocket_protocol_opcode_is_valid(frame->opcode)) {
		return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
	}

	if (payload_len == 126) {
		if (len < 4) {
			return WEBSOCKET_SERVER_FRAME_INCOMPLETE;
		}

		payload_len = ((uint64_t) in[2] << 8) | in[3];
		if (payload_len < 126) {
			return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
		}
		pos = 4;
	} else if (payload_len == 127) {
		if (len < 10) {
			return WEBSOCKET_SERVER_FRAME_INCOMPLETE;
		}
		if ((in[2] & 0x80) != 0) {
			return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
		}

		payload_len = 0;
		for (i = 0; i < 8; i++) {
			payload_len = (payload_len << 8) | in[2 + i];
		}
		if (payload_len <= 0xffff) {
			return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
		}
		pos = 10;
	}

	if (websocket_protocol_opcode_is_control(frame->opcode) && (!frame->final || payload_len > 125)) {
		return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
	}

	if (!websocket_protocol_opcode_is_control(frame->opcode) && payload_len > max_message_size) {
		return WEBSOCKET_SERVER_FRAME_MESSAGE_TOO_BIG;
	}

	if (payload_len > SIZE_MAX || payload_len > SIZE_MAX - pos || len < pos + 4) {
		return WEBSOCKET_SERVER_FRAME_INCOMPLETE;
	}

	memcpy(mask, in + pos, sizeof(mask));
	pos += 4;

	if (payload_len > SIZE_MAX - pos || len < pos + (size_t) payload_len) {
		return WEBSOCKET_SERVER_FRAME_INCOMPLETE;
	}

	if (frame->opcode == WEBSOCKET_OPCODE_CLOSE) {
		if (payload_len == 1) {
			return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
		}
		if (payload_len >= 2) {
			const zend_long close_code = ((zend_long) (in[pos] ^ mask[0]) << 8) | (zend_long) (in[pos + 1] ^ mask[1]);

			if (!websocket_protocol_close_code_is_valid(close_code)) {
				return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
			}
		}
	}

	frame->payload = zend_string_alloc((size_t) payload_len, false);
	websocket_server_mask_payload((unsigned char *) ZSTR_VAL(frame->payload), in + pos, (size_t) payload_len, mask);
	ZSTR_VAL(frame->payload)[payload_len] = '\0';
	frame->bytes_consumed = pos + (size_t) payload_len;

	if (frame->opcode == WEBSOCKET_OPCODE_CLOSE && payload_len >= 2 && !websocket_protocol_is_valid_utf8(ZSTR_VAL(frame->payload) + 2, (size_t) payload_len - 2)) {
		zend_string_release(frame->payload);
		frame->payload = NULL;
		return WEBSOCKET_SERVER_FRAME_INVALID_PAYLOAD;
	}

	return WEBSOCKET_SERVER_FRAME_OK;
}

static bool websocket_server_call_message_handler(websocket_server_object *intern, zval *connection, zend_string *payload, const uint8_t opcode)
{
	zval params[3];
	zval retval;
	zend_object *type_case;
	bool ok;

	if (Z_ISUNDEF(intern->on_message)) {
		return true;
	}

	type_case = websocket_protocol_message_type_from_opcode(opcode);
	if (!type_case) {
		return true;
	}

	ZVAL_COPY(&params[0], connection);
	ZVAL_STR_COPY(&params[1], payload);
	ZVAL_OBJ_COPY(&params[2], type_case);
	ZVAL_UNDEF(&retval);

	ok = call_user_function(EG(function_table), NULL, &intern->on_message, &retval, 3, params) != FAILURE;

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);

	if (!ok) {
		zend_throw_error(NULL, "Failed to call WebSocket server message handler");
		return false;
	}

	if (!Z_ISUNDEF(retval)) {
		zval_ptr_dtor(&retval);
	}

	return !EG(exception);
}

static void websocket_server_clear_fragment(websocket_connection_object *connection_obj)
{
	if (connection_obj->fragmented_payload) {
		zend_string_release(connection_obj->fragmented_payload);
		connection_obj->fragmented_payload = NULL;
	}

	connection_obj->fragmented = false;
	connection_obj->fragmented_opcode = 0;
}

static bool websocket_server_append_fragment(websocket_connection_object *connection_obj, zend_string *payload, const size_t max_message_size)
{
	const size_t current_len = connection_obj->fragmented_payload ? ZSTR_LEN(connection_obj->fragmented_payload) : 0;
	const size_t append_len = ZSTR_LEN(payload);
	zend_string *combined;

	if (append_len > max_message_size || current_len > max_message_size - append_len) {
		return false;
	}

	if (!connection_obj->fragmented_payload) {
		connection_obj->fragmented_payload = zend_string_alloc(append_len, false);
		if (append_len > 0) {
			memcpy(ZSTR_VAL(connection_obj->fragmented_payload), ZSTR_VAL(payload), append_len);
		}
		ZSTR_VAL(connection_obj->fragmented_payload)[append_len] = '\0';
		return true;
	}

	if (append_len == 0) {
		return true;
	}

	combined = zend_string_extend(connection_obj->fragmented_payload, current_len + append_len, false);
	memcpy(ZSTR_VAL(combined) + current_len, ZSTR_VAL(payload), append_len);
	ZSTR_VAL(combined)[current_len + append_len] = '\0';
	connection_obj->fragmented_payload = combined;

	return true;
}

static bool websocket_server_handle_data_frame(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj, websocket_server_frame *frame)
{
	const size_t max_message_size = websocket_server_max_message_size(intern);

	if (frame->opcode == WEBSOCKET_OPCODE_CONTINUATION) {
		if (!connection_obj->fragmented) {
			return websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_PROTOCOL_ERROR, "protocol error");
		}

		if (!websocket_server_append_fragment(connection_obj, frame->payload, max_message_size)) {
			websocket_server_clear_fragment(connection_obj);
			return websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_MESSAGE_TOO_BIG, "message too big");
		}

		if (!frame->final) {
			return true;
		}

		if (connection_obj->fragmented_opcode == WEBSOCKET_OPCODE_TEXT && !websocket_protocol_is_valid_utf8(ZSTR_VAL(connection_obj->fragmented_payload), ZSTR_LEN(connection_obj->fragmented_payload))) {
			websocket_server_clear_fragment(connection_obj);
			return websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_INVALID_PAYLOAD, "invalid utf-8");
		}

		if (!websocket_server_call_message_handler(intern, connection, connection_obj->fragmented_payload, connection_obj->fragmented_opcode)) {
			websocket_server_clear_fragment(connection_obj);
			return false;
		}

		websocket_server_clear_fragment(connection_obj);
		return true;
	}

	if (connection_obj->fragmented) {
		websocket_server_clear_fragment(connection_obj);
		return websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_PROTOCOL_ERROR, "protocol error");
	}

	if (frame->final) {
		if (frame->opcode == WEBSOCKET_OPCODE_TEXT && !websocket_protocol_is_valid_utf8(ZSTR_VAL(frame->payload), ZSTR_LEN(frame->payload))) {
			return websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_INVALID_PAYLOAD, "invalid utf-8");
		}

		return websocket_server_call_message_handler(intern, connection, frame->payload, frame->opcode);
	}

	connection_obj->fragmented = true;
	connection_obj->fragmented_opcode = frame->opcode;
	connection_obj->fragmented_payload = frame->payload;
	frame->payload = NULL;

	return true;
}

static bool websocket_server_handle_frame(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj, websocket_server_frame *frame)
{
	bool ok = true;

	switch (frame->opcode) {
		case WEBSOCKET_OPCODE_TEXT:
		case WEBSOCKET_OPCODE_BINARY:
		case WEBSOCKET_OPCODE_CONTINUATION:
			ok = websocket_server_handle_data_frame(intern, connection, connection_obj, frame);
			break;
		case WEBSOCKET_OPCODE_PING:
			ok = websocket_connection_send_frame(connection_obj, frame->payload, WEBSOCKET_OPCODE_PONG);
			break;
		case WEBSOCKET_OPCODE_PONG:
			break;
		case WEBSOCKET_OPCODE_CLOSE:
			if (connection_obj->open && connection_obj->upgraded) {
				ok = websocket_connection_send_frame(connection_obj, frame->payload, WEBSOCKET_OPCODE_CLOSE);
			}
			websocket_server_clear_fragment(connection_obj);
			if (ok) {
				websocket_connection_close_after_write(connection_obj);
			} else {
				websocket_connection_close_socket(connection_obj);
				ok = true;
			}
			break;
		default:
			ok = websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_PROTOCOL_ERROR, "protocol error");
			break;
	}

	if (frame->payload) {
		zend_string_release(frame->payload);
		frame->payload = NULL;
	}

	return ok && !EG(exception);
}

static bool websocket_server_process_buffered_frames(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj)
{
	const size_t max_message_size = websocket_server_max_message_size(intern);

	while (connection_obj->open && !connection_obj->close_after_write && connection_obj->upgraded && connection_obj->read_buffer_len > 0) {
		websocket_server_frame frame;
		const websocket_server_frame_status status = websocket_server_parse_frame(connection_obj, max_message_size, &frame);

		if (status == WEBSOCKET_SERVER_FRAME_INCOMPLETE) {
			return true;
		}

		if (status == WEBSOCKET_SERVER_FRAME_MESSAGE_TOO_BIG) {
			websocket_server_clear_fragment(connection_obj);
			(void) websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_MESSAGE_TOO_BIG, "message too big");
			return true;
		}

		if (status == WEBSOCKET_SERVER_FRAME_INVALID_PAYLOAD) {
			websocket_server_clear_fragment(connection_obj);
			(void) websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_INVALID_PAYLOAD, "invalid utf-8");
			return true;
		}

		if (status == WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR) {
			websocket_server_clear_fragment(connection_obj);
			(void) websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_PROTOCOL_ERROR, "protocol error");
			return true;
		}

		websocket_connection_discard_read_bytes(connection_obj, frame.bytes_consumed);
		if (!websocket_server_handle_frame(intern, connection, connection_obj, &frame)) {
			return false;
		}
	}

	return true;
}

static bool websocket_server_finish_upgrade(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj, zend_string *accept_key, zend_string *selected_subprotocol, const size_t bytes_consumed)
{
	zend_string *response;
	bool close_requested = false;
	bool ok;
	const bool needs_connection = Z_ISUNDEF(intern->on_open) || intern->on_open_param_count > 0;

	response = websocket_http_upgrade_response(accept_key, selected_subprotocol);
	ok = websocket_server_send_bytes(connection_obj->fd, ZSTR_VAL(response), ZSTR_LEN(response));
	zend_string_release(response);

	if (!ok) {
		connection_obj->open = false;
		return true;
	}

	if (connection_obj->selected_subprotocol) {
		zend_string_release(connection_obj->selected_subprotocol);
		connection_obj->selected_subprotocol = NULL;
	}
	if (selected_subprotocol) {
		connection_obj->selected_subprotocol = zend_string_copy(selected_subprotocol);
	}

	connection_obj->upgraded = true;
	websocket_connection_discard_read_bytes(connection_obj, bytes_consumed);

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

	return websocket_server_process_buffered_frames(intern, connection, connection_obj);
}

static bool websocket_server_process_handshake(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj)
{
	char chunk[WEBSOCKET_READ_CHUNK_SIZE];

	while (connection_obj->open && !connection_obj->upgraded) {
		const ssize_t bytes_read = recv(connection_obj->fd, chunk, sizeof(chunk), 0);

		if (bytes_read > 0) {
			zend_string *accept_key = NULL;
			zend_string *selected_subprotocol = NULL;
			size_t bytes_consumed = 0;
			websocket_http_upgrade_result result;

			connection_obj->last_activity_usec = websocket_server_now_usec();

			if (!websocket_connection_ensure_read_capacity(connection_obj, (size_t) bytes_read, WEBSOCKET_HTTP_MAX_REQUEST_SIZE)) {
				(void) websocket_server_send_bytes(connection_obj->fd, websocket_bad_request_response, sizeof(websocket_bad_request_response) - 1);
				connection_obj->open = false;
				return true;
			}

			memcpy(connection_obj->read_buffer + connection_obj->read_buffer_len, chunk, (size_t) bytes_read);
			connection_obj->read_buffer_len += (size_t) bytes_read;

			result = websocket_http_parse_upgrade(connection_obj->read_buffer, connection_obj->read_buffer_len, Z_TYPE(intern->subprotocols) == IS_ARRAY ? Z_ARRVAL(intern->subprotocols) : NULL, &accept_key, &selected_subprotocol, &bytes_consumed);
			if (result == WEBSOCKET_HTTP_UPGRADE_INCOMPLETE) {
				continue;
			}

			if (result == WEBSOCKET_HTTP_UPGRADE_INVALID) {
				(void) websocket_server_send_bytes(connection_obj->fd, websocket_bad_request_response, sizeof(websocket_bad_request_response) - 1);
				connection_obj->open = false;
				return true;
			}

			if (!Z_ISUNDEF(intern->on_handshake)) {
				zval request;
				bool accepted = true;

				ZVAL_UNDEF(&request);
				if (!websocket_server_create_request(&request, connection_obj->read_buffer, bytes_consumed)) {
					(void) websocket_server_send_bytes(connection_obj->fd, websocket_bad_request_response, sizeof(websocket_bad_request_response) - 1);
					connection_obj->open = false;
					zend_string_release(accept_key);
					if (selected_subprotocol) {
						zend_string_release(selected_subprotocol);
					}
					return true;
				}

				if (!websocket_server_call_handshake_handler(intern, &request, connection_obj, &accepted)) {
					zval_ptr_dtor(&request);
					zend_string_release(accept_key);
					if (selected_subprotocol) {
						zend_string_release(selected_subprotocol);
					}
					return false;
				}
				zval_ptr_dtor(&request);

				if (!accepted) {
					zend_string_release(accept_key);
					if (selected_subprotocol) {
						zend_string_release(selected_subprotocol);
					}
					return true;
				}
			}

			if (!websocket_server_finish_upgrade(intern, connection, connection_obj, accept_key, selected_subprotocol, bytes_consumed)) {
				zend_string_release(accept_key);
				if (selected_subprotocol) {
					zend_string_release(selected_subprotocol);
				}
				return false;
			}

			zend_string_release(accept_key);
			if (selected_subprotocol) {
				zend_string_release(selected_subprotocol);
			}
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

static bool websocket_server_process_frame_reads(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj)
{
	char chunk[WEBSOCKET_READ_CHUNK_SIZE];
	const size_t max_message_size = websocket_server_max_message_size(intern);
	const size_t read_limit = max_message_size > SIZE_MAX - WEBSOCKET_READ_CHUNK_SIZE - 14 ? SIZE_MAX : max_message_size + WEBSOCKET_READ_CHUNK_SIZE + 14;

	while (connection_obj->open && !connection_obj->close_after_write && connection_obj->upgraded) {
		const ssize_t bytes_read = recv(connection_obj->fd, chunk, sizeof(chunk), 0);

		if (bytes_read > 0) {
			connection_obj->last_activity_usec = websocket_server_now_usec();

			if (!websocket_connection_ensure_read_capacity(connection_obj, (size_t) bytes_read, read_limit)) {
				(void) websocket_server_close_with_code(connection_obj, WEBSOCKET_CLOSE_MESSAGE_TOO_BIG, "message too big");
				return true;
			}

			memcpy(connection_obj->read_buffer + connection_obj->read_buffer_len, chunk, (size_t) bytes_read);
			connection_obj->read_buffer_len += (size_t) bytes_read;

			if (!websocket_server_process_buffered_frames(intern, connection, connection_obj)) {
				return false;
			}

			continue;
		}

		if (bytes_read == 0) {
			connection_obj->open = false;
			return true;
		}

		if (errno == EINTR) {
			continue;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return websocket_server_process_buffered_frames(intern, connection, connection_obj);
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

	if (intern->connection_count >= websocket_server_max_connections(intern)) {
		(void) websocket_server_send_bytes(client_fd, websocket_service_unavailable_response, sizeof(websocket_service_unavailable_response) - 1);
		websocket_server_close_fd(client_fd);
		return true;
	}

	if (websocket_server_set_nonblocking(client_fd) == FAILURE) {
		websocket_server_close_fd(client_fd);
		zend_throw_error(NULL, "Cannot make accepted WebSocket connection non-blocking: %s", strerror(errno));
		return false;
	}

	websocket_server_create_connection_zval(intern, &connection);
	connection_obj = Z_WEBSOCKET_CONNECTION_P(&connection);
	websocket_connection_open(connection_obj, WEBSOCKET_G(next_connection_id)++, (const struct sockaddr *) &remote_addr, remote_addr_len, client_fd);
	connection_obj->accepted_at_usec = websocket_server_now_usec();
	connection_obj->last_activity_usec = connection_obj->accepted_at_usec;
	connection_obj->max_queued_bytes = websocket_server_max_queued_bytes(intern);

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
	const uint64_t now_usec = websocket_server_now_usec();

	for (i = 0; i < intern->connection_count; i++) {
		websocket_connection_object *connection_obj = Z_WEBSOCKET_CONNECTION_P(&intern->connections[i]);

		if (connection_obj->fd != fd) {
			continue;
		}

		if (!connection_obj->open) {
			return true;
		}

		if (websocket_server_connection_expired(intern, connection_obj, now_usec)) {
			websocket_server_close_expired_connection(connection_obj);
			return true;
		}

		if (websocket_connection_has_pending_writes(connection_obj) && !websocket_connection_flush(connection_obj)) {
			return true;
		}

		if (!connection_obj->open || connection_obj->close_after_write) {
			return true;
		}

		if (!connection_obj->upgraded) {
			return websocket_server_process_handshake(intern, &intern->connections[i], connection_obj);
		}

		return websocket_server_process_frame_reads(intern, &intern->connections[i], connection_obj);
	}

	return true;
}

static bool websocket_server_process_connections(websocket_server_object *intern)
{
	size_t i;
	const uint64_t now_usec = websocket_server_now_usec();

	for (i = 0; i < intern->connection_count; i++) {
		websocket_connection_object *connection_obj = Z_WEBSOCKET_CONNECTION_P(&intern->connections[i]);

		if (!connection_obj->open) {
			continue;
		}

		if (websocket_server_connection_expired(intern, connection_obj, now_usec)) {
			websocket_server_close_expired_connection(connection_obj);
			continue;
		}

		if (websocket_connection_has_pending_writes(connection_obj) && !websocket_connection_flush(connection_obj)) {
			continue;
		}

		if (!connection_obj->open || connection_obj->close_after_write) {
			continue;
		}

		if (!connection_obj->upgraded && !websocket_server_process_handshake(intern, &intern->connections[i], connection_obj)) {
			return false;
		}

		if (connection_obj->upgraded && connection_obj->read_buffer_len > 0 && !websocket_server_process_buffered_frames(intern, &intern->connections[i], connection_obj)) {
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
