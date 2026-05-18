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

void websocket_connection_close_socket(websocket_connection_object *intern)
{
	if (intern->fd >= 0) {
		const int fd = intern->fd;

		if (GC_REFCOUNT(&intern->std) > 1) {
			websocket_connection_cache_remote_address(intern);
		}

		intern->fd = -1;
		while (close(fd) < 0 && errno == EINTR) {
		}
	}

	intern->open = false;
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

	if (!intern->open) {
		zend_throw_error(NULL, "Cannot send on a closed WebSocket connection");
		RETURN_THROWS();
	}

	(void) payload;
	(void) type;
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

	(void) reason;
	websocket_connection_object *intern = Z_WEBSOCKET_CONNECTION_P(ZEND_THIS);
	if (intern->defer_close) {
		intern->open = false;
		return;
	}

	websocket_connection_close_socket(intern);
}

PHP_METHOD(WebSocket_Connection, isOpen)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_BOOL(Z_WEBSOCKET_CONNECTION_P(ZEND_THIS)->open);
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
