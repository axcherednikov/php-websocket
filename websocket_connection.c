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
#include "websocket_arginfo.h"

#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static zend_object_handlers websocket_connection_handlers;

websocket_connection_object *websocket_connection_from_obj(zend_object *obj)
{
	return (websocket_connection_object *)((char *)(obj) - XtOffsetOf(websocket_connection_object, std));
}

static zend_object *websocket_connection_create_object(zend_class_entry *ce)
{
	websocket_connection_object *intern = zend_object_alloc(sizeof(websocket_connection_object), ce);

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);

	intern->id = NULL;
	intern->remote_address = NULL;
	intern->remote_addr_len = 0;
	intern->numeric_id = 0;
	intern->fd = -1;
	intern->open = false;
	intern->upgraded = false;
	intern->close_notified = false;
	intern->has_remote_addr = false;
	intern->defer_close = false;
	intern->read_buffer = NULL;
	intern->read_buffer_len = 0;
	intern->read_buffer_capacity = 0;
	intern->write_queue = NULL;
	intern->write_queue_count = 0;
	intern->write_queue_capacity = 0;
	intern->write_queue_offset = 0;
	intern->queued_bytes = 0;
	intern->max_queued_bytes = WEBSOCKET_DEFAULT_MAX_QUEUED_BYTES;
	intern->write_watched = false;
	intern->close_after_write = false;
	intern->fragmented = false;
	intern->fragmented_opcode = 0;
	intern->fragmented_payload = NULL;

	intern->std.handlers = &websocket_connection_handlers;

	return &intern->std;
}

static zend_string *websocket_connection_make_id(websocket_connection_object *intern)
{
	if (!intern->id) {
		intern->id = strpprintf(0, "0." ZEND_ULONG_FMT, (zend_ulong) intern->numeric_id);
	}

	return intern->id;
}

static zend_string *websocket_connection_make_remote_address(websocket_connection_object *intern)
{
	char host[NI_MAXHOST];
	char port[NI_MAXSERV];
	const struct sockaddr *addr = (const struct sockaddr *) &intern->remote_addr;

	if (intern->remote_address) {
		return intern->remote_address;
	}

	if (!intern->has_remote_addr && intern->fd >= 0) {
		websocket_connection_cache_remote_address(intern);
	}

	if (!intern->has_remote_addr || getnameinfo(addr, intern->remote_addr_len, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
		intern->remote_address = zend_string_init("", 0, false);
		return intern->remote_address;
	}

	if (addr->sa_family == AF_INET6) {
		intern->remote_address = strpprintf(0, "[%s]:%s", host, port);
	} else {
		intern->remote_address = strpprintf(0, "%s:%s", host, port);
	}

	return intern->remote_address;
}

void websocket_connection_cache_remote_address(websocket_connection_object *intern)
{
	struct sockaddr_storage remote_addr;
	socklen_t remote_addr_len = sizeof(remote_addr);

	if (intern->has_remote_addr || intern->fd < 0) {
		return;
	}

	if (getpeername(intern->fd, (struct sockaddr *) &remote_addr, &remote_addr_len) == 0 && remote_addr_len <= sizeof(intern->remote_addr)) {
		memcpy(&intern->remote_addr, &remote_addr, remote_addr_len);
		intern->remote_addr_len = remote_addr_len;
		intern->has_remote_addr = true;
	}
}

static void websocket_connection_clear_write_queue(websocket_connection_object *intern)
{
	size_t i;

	for (i = 0; i < intern->write_queue_count; i++) {
		zend_string_release(intern->write_queue[i]);
	}

	intern->write_queue_count = 0;
	intern->write_queue_offset = 0;
	intern->queued_bytes = 0;

	if (intern->write_watched && intern->fd >= 0 && WEBSOCKET_G(driver)) {
		WEBSOCKET_G(driver)->unwatch_write(intern->fd);
	}

	intern->write_watched = false;
	intern->close_after_write = false;
}

void websocket_connection_close_socket(websocket_connection_object *intern)
{
	if (intern->fd >= 0) {
		const int fd = intern->fd;

		if (WEBSOCKET_G(driver)) {
			WEBSOCKET_G(driver)->unwatch(fd);
		}

		if (GC_REFCOUNT(&intern->std) > 1) {
			websocket_connection_cache_remote_address(intern);
		}

		intern->fd = -1;
		while (close(fd) < 0 && errno == EINTR) {
		}
	}

	intern->open = false;
	websocket_connection_clear_write_queue(intern);

	if (intern->fragmented_payload) {
		zend_string_release(intern->fragmented_payload);
		intern->fragmented_payload = NULL;
	}
	intern->fragmented = false;
	intern->fragmented_opcode = 0;
}

bool websocket_connection_has_pending_writes(websocket_connection_object *intern)
{
	return intern->write_queue_count > 0;
}

static bool websocket_connection_write_some(const int fd, zend_string *frame, size_t *offset, size_t *written_bytes)
{
	const size_t len = ZSTR_LEN(frame);

	*written_bytes = 0;

	while (*offset < len) {
		ssize_t written;

#ifdef MSG_NOSIGNAL
		written = send(fd, ZSTR_VAL(frame) + *offset, len - *offset, MSG_NOSIGNAL);
#else
		written = send(fd, ZSTR_VAL(frame) + *offset, len - *offset, 0);
#endif
		if (written > 0) {
			*offset += (size_t) written;
			*written_bytes += (size_t) written;
			continue;
		}

		if (written < 0 && errno == EINTR) {
			continue;
		}

		if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			return true;
		}

		return false;
	}

	return true;
}

static bool websocket_connection_ensure_write_capacity(websocket_connection_object *intern)
{
	if (intern->write_queue_count < intern->write_queue_capacity) {
		return true;
	}

	intern->write_queue_capacity = intern->write_queue_capacity > 0 ? intern->write_queue_capacity * 2 : 8;
	intern->write_queue = intern->write_queue ? erealloc(intern->write_queue, sizeof(zend_string *) * intern->write_queue_capacity) : emalloc(sizeof(zend_string *) * intern->write_queue_capacity);

	return true;
}

static bool websocket_connection_queue_frame(websocket_connection_object *intern, zend_string *frame, const size_t offset)
{
	const size_t remaining = ZSTR_LEN(frame) - offset;

	if (remaining > intern->max_queued_bytes || intern->queued_bytes > intern->max_queued_bytes - remaining) {
		errno = ENOBUFS;
		return false;
	}

	if (!websocket_connection_ensure_write_capacity(intern)) {
		return false;
	}

	if (intern->write_queue_count == 0) {
		intern->write_queue_offset = offset;
	}

	zend_string_addref(frame);
	intern->write_queue[intern->write_queue_count++] = frame;
	intern->queued_bytes += remaining;

	if (!intern->write_watched && WEBSOCKET_G(driver)) {
		if (WEBSOCKET_G(driver)->watch_write(intern->fd) == FAILURE) {
			return false;
		}
		intern->write_watched = true;
	}

	return true;
}

bool websocket_connection_flush(websocket_connection_object *intern)
{
	if (!intern->open || intern->fd < 0) {
		return false;
	}

	while (intern->write_queue_count > 0) {
		zend_string *frame = intern->write_queue[0];
		size_t offset = intern->write_queue_offset;
		size_t written_bytes = 0;

		if (!websocket_connection_write_some(intern->fd, frame, &offset, &written_bytes)) {
			intern->open = false;
			return false;
		}

		if (written_bytes > intern->queued_bytes) {
			intern->queued_bytes = 0;
		} else {
			intern->queued_bytes -= written_bytes;
		}

		if (offset < ZSTR_LEN(frame)) {
			intern->write_queue_offset = offset;
			return true;
		}

		zend_string_release(frame);
		if (intern->write_queue_count > 1) {
			memmove(&intern->write_queue[0], &intern->write_queue[1], sizeof(zend_string *) * (intern->write_queue_count - 1));
		}
		intern->write_queue_count--;
		intern->write_queue_offset = 0;
	}

	if (intern->write_watched && WEBSOCKET_G(driver)) {
		WEBSOCKET_G(driver)->unwatch_write(intern->fd);
		intern->write_watched = false;
	}

	if (intern->close_after_write) {
		websocket_connection_close_socket(intern);
	}

	return true;
}

void websocket_connection_close_after_write(websocket_connection_object *intern)
{
	if (!intern->open) {
		return;
	}

	if (!websocket_connection_has_pending_writes(intern)) {
		websocket_connection_close_socket(intern);
		return;
	}

	intern->close_after_write = true;
}

bool websocket_connection_send_frame(websocket_connection_object *intern, zend_string *payload, const uint8_t opcode)
{
	zend_string *frame;
	bool ok;
	size_t offset = 0;
	size_t written_bytes = 0;

	if (!intern->open || intern->close_after_write || !intern->upgraded || intern->fd < 0) {
		return false;
	}

	frame = websocket_protocol_pack_payload(payload, opcode, WEBSOCKET_FLAG_FIN);

	if (ZSTR_LEN(frame) > intern->max_queued_bytes) {
		errno = ENOBUFS;
		zend_string_release(frame);
		return false;
	}

	if (intern->write_queue_count > 0) {
		ok = websocket_connection_queue_frame(intern, frame, 0);
		zend_string_release(frame);
		if (!ok && errno != ENOBUFS) {
			intern->open = false;
		}
		return ok;
	}

	ok = websocket_connection_write_some(intern->fd, frame, &offset, &written_bytes);
	if (ok && offset < ZSTR_LEN(frame)) {
		ok = websocket_connection_queue_frame(intern, frame, offset);
	}
	zend_string_release(frame);

	if (!ok && errno != ENOBUFS) {
		intern->open = false;
	}

	return ok;
}

bool websocket_connection_send_close_frame(websocket_connection_object *intern, const zend_long code, zend_string *reason)
{
	zend_string *payload;
	bool ok;

	if (!intern->open || !intern->upgraded || intern->fd < 0) {
		return false;
	}

	payload = websocket_protocol_close_payload(code, reason);
	if (!payload) {
		return false;
	}

	ok = websocket_connection_send_frame(intern, payload, WEBSOCKET_OPCODE_CLOSE);
	zend_string_release(payload);

	return ok;
}

void websocket_connection_open(websocket_connection_object *intern, uint64_t id, const struct sockaddr *remote_addr, socklen_t remote_addr_len, const int fd)
{
	if (intern->id) {
		zend_string_release(intern->id);
	}
	if (intern->remote_address) {
		zend_string_release(intern->remote_address);
	}

	intern->id = NULL;
	intern->remote_address = NULL;
	intern->numeric_id = id;
	intern->has_remote_addr = false;
	intern->remote_addr_len = 0;

	if (remote_addr && remote_addr_len <= sizeof(intern->remote_addr)) {
		memcpy(&intern->remote_addr, remote_addr, remote_addr_len);
		intern->remote_addr_len = remote_addr_len;
		intern->has_remote_addr = true;
	}

	intern->fd = fd;
	intern->open = true;
	intern->upgraded = false;
	intern->close_notified = false;
	intern->defer_close = false;
	intern->read_buffer_len = 0;
	websocket_connection_clear_write_queue(intern);
	intern->max_queued_bytes = WEBSOCKET_DEFAULT_MAX_QUEUED_BYTES;
	if (intern->fragmented_payload) {
		zend_string_release(intern->fragmented_payload);
		intern->fragmented_payload = NULL;
	}
	intern->fragmented = false;
	intern->fragmented_opcode = 0;
}

static void websocket_connection_free_object(zend_object *object)
{
	websocket_connection_object *intern = websocket_connection_from_obj(object);

	websocket_connection_close_socket(intern);

	if (intern->id) {
		zend_string_release(intern->id);
	}
	if (intern->remote_address) {
		zend_string_release(intern->remote_address);
	}
	if (intern->read_buffer) {
		efree(intern->read_buffer);
	}
	if (intern->write_queue) {
		efree(intern->write_queue);
	}
	if (intern->fragmented_payload) {
		zend_string_release(intern->fragmented_payload);
	}

	zend_object_std_dtor(&intern->std);
}

static zval *websocket_connection_read_property(zend_object *object, zend_string *name, int type, void **cache_slot, zval *rv)
{
	websocket_connection_object *intern = websocket_connection_from_obj(object);

	if (zend_string_equals_literal(name, "id")) {
		ZVAL_STR_COPY(rv, websocket_connection_make_id(intern));
		return rv;
	}

	if (zend_string_equals_literal(name, "remoteAddress")) {
		ZVAL_STR_COPY(rv, websocket_connection_make_remote_address(intern));
		return rv;
	}

	return zend_std_read_property(object, name, type, cache_slot, rv);
}

PHP_METHOD(WebSocket_Connection, send)
{
	zend_string *payload;
	zval *type = NULL;
	websocket_connection_object *intern = Z_WEBSOCKET_CONNECTION_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR(payload)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJECT_OF_CLASS(type, websocket_message_type_ce)
	ZEND_PARSE_PARAMETERS_END();

	if (!intern->open || intern->close_after_write) {
		zend_throw_error(NULL, "Cannot send on a closed WebSocket connection");
		RETURN_THROWS();
	}

	if (!intern->upgraded) {
		zend_throw_error(NULL, "Cannot send before the WebSocket connection is upgraded");
		RETURN_THROWS();
	}

	const uint8_t opcode = type ? websocket_protocol_message_type_opcode(type) : WEBSOCKET_OPCODE_TEXT;
	if (opcode == WEBSOCKET_OPCODE_CONTINUATION) {
		zend_argument_value_error(2, "must not be WebSocket\\MessageType::Continuation");
		RETURN_THROWS();
	}
	if (websocket_protocol_opcode_is_control(opcode) && ZSTR_LEN(payload) > 125) {
		zend_argument_value_error(1, "control frame payload must be at most 125 bytes");
		RETURN_THROWS();
	}

	if (!websocket_connection_send_frame(intern, payload, opcode)) {
		zend_throw_error(NULL, "Failed to send WebSocket frame");
		RETURN_THROWS();
	}
}

PHP_METHOD(WebSocket_Connection, close)
{
	zend_long code = 1000;
	zend_string *reason = NULL;

	if (ZEND_NUM_ARGS() > 0) {
		ZEND_PARSE_PARAMETERS_START(0, 2)
			Z_PARAM_OPTIONAL
			Z_PARAM_LONG(code)
			Z_PARAM_STR(reason)
		ZEND_PARSE_PARAMETERS_END();

		if (code < 1000 || code > 4999) {
			zend_argument_value_error(1, "must be between 1000 and 4999");
			RETURN_THROWS();
		}
	}

	if (!reason) {
		reason = ZSTR_EMPTY_ALLOC();
	}
	if (ZSTR_LEN(reason) > WEBSOCKET_CLOSE_REASON_MAX_LEN) {
		zend_argument_value_error(2, "must be at most %d bytes", WEBSOCKET_CLOSE_REASON_MAX_LEN);
		RETURN_THROWS();
	}

	websocket_connection_object *intern = Z_WEBSOCKET_CONNECTION_P(ZEND_THIS);
	if (intern->defer_close) {
		intern->open = false;
		return;
	}

	if (intern->open && intern->upgraded) {
		(void) websocket_connection_send_close_frame(intern, code, reason);
	}

	websocket_connection_close_after_write(intern);
}

PHP_METHOD(WebSocket_Connection, isOpen)
{
	ZEND_PARSE_PARAMETERS_NONE();

	websocket_connection_object *intern = Z_WEBSOCKET_CONNECTION_P(ZEND_THIS);

	RETURN_BOOL(intern->open && !intern->close_after_write);
}

void websocket_register_connection_class(void)
{
	websocket_connection_ce = register_class_WebSocket_Connection();
	websocket_connection_ce->create_object = websocket_connection_create_object;

	memcpy(&websocket_connection_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	websocket_connection_handlers.offset = XtOffsetOf(websocket_connection_object, std);
	websocket_connection_handlers.free_obj = websocket_connection_free_object;
	websocket_connection_handlers.read_property = websocket_connection_read_property;
	websocket_connection_handlers.clone_obj = NULL;
	WEBSOCKET_SET_DEFAULT_HANDLERS(websocket_connection_ce, &websocket_connection_handlers);
}
