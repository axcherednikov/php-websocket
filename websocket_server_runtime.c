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

typedef enum _websocket_server_frame_status {
	WEBSOCKET_SERVER_FRAME_INCOMPLETE = 0,
	WEBSOCKET_SERVER_FRAME_OK = 1,
	WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR = 2,
	WEBSOCKET_SERVER_FRAME_MESSAGE_TOO_BIG = 3,
} websocket_server_frame_status;

typedef struct _websocket_server_frame {
	uint8_t opcode;
	bool final;
	size_t bytes_consumed;
	zend_string *payload;
} websocket_server_frame;

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

static size_t websocket_server_max_message_size(websocket_server_object *intern)
{
	zval *value;

	if (Z_TYPE(intern->options) != IS_ARRAY) {
		return WEBSOCKET_DEFAULT_MAX_MESSAGE_SIZE;
	}

	value = zend_hash_str_find(Z_ARRVAL(intern->options), "maxMessageSize", sizeof("maxMessageSize") - 1);
	if (!value) {
		return WEBSOCKET_DEFAULT_MAX_MESSAGE_SIZE;
	}

	const zend_long max_message_size = zval_get_long(value);
	if (max_message_size <= 0) {
		return WEBSOCKET_DEFAULT_MAX_MESSAGE_SIZE;
	}

	return (size_t) max_message_size;
}

static bool websocket_server_close_with_code(websocket_connection_object *connection_obj, const zend_long code, const char *reason)
{
	zend_string *reason_string;
	bool ok;

	reason_string = reason ? zend_string_init(reason, strlen(reason), false) : zend_string_init("", 0, false);
	ok = websocket_connection_send_close_frame(connection_obj, code, reason_string);
	zend_string_release(reason_string);
	connection_obj->open = false;

	return ok;
}

static zend_always_inline void websocket_server_mask_payload(unsigned char *dst, const unsigned char *src, const size_t len, const uint8_t mask[4])
{
	size_t i;

	for (i = 0; i < len; i++) {
		dst[i] = src[i] ^ mask[i & 3];
	}
}

static bool websocket_server_close_code_is_valid(const zend_long code)
{
	if (code < 1000 || code > 4999) {
		return false;
	}

	switch (code) {
		case 1004:
		case 1005:
		case 1006:
		case 1015:
			return false;
		default:
			return true;
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

			if (!websocket_server_close_code_is_valid(close_code)) {
				return WEBSOCKET_SERVER_FRAME_PROTOCOL_ERROR;
			}
		}
	}

	frame->payload = zend_string_alloc((size_t) payload_len, false);
	websocket_server_mask_payload((unsigned char *) ZSTR_VAL(frame->payload), in + pos, (size_t) payload_len, mask);
	ZSTR_VAL(frame->payload)[payload_len] = '\0';
	frame->bytes_consumed = pos + (size_t) payload_len;

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
			connection_obj->open = false;
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

	while (connection_obj->open && connection_obj->upgraded && connection_obj->read_buffer_len > 0) {
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
			size_t bytes_consumed = 0;
			websocket_http_upgrade_result result;

			if (!websocket_connection_ensure_read_capacity(connection_obj, (size_t) bytes_read, WEBSOCKET_HTTP_MAX_REQUEST_SIZE)) {
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

static bool websocket_server_process_frame_reads(websocket_server_object *intern, zval *connection, websocket_connection_object *connection_obj)
{
	char chunk[WEBSOCKET_READ_CHUNK_SIZE];
	const size_t max_message_size = websocket_server_max_message_size(intern);
	const size_t read_limit = max_message_size > SIZE_MAX - WEBSOCKET_READ_CHUNK_SIZE - 14 ? SIZE_MAX : max_message_size + WEBSOCKET_READ_CHUNK_SIZE + 14;

	while (connection_obj->open && connection_obj->upgraded) {
		const ssize_t bytes_read = recv(connection_obj->fd, chunk, sizeof(chunk), 0);

		if (bytes_read > 0) {
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

		if (!connection_obj->open) {
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

	for (i = 0; i < intern->connection_count; i++) {
		websocket_connection_object *connection_obj = Z_WEBSOCKET_CONNECTION_P(&intern->connections[i]);

		if (!connection_obj->open) {
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
