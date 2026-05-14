#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_websocket.h"
#include "php_websocket_compat.h"
#include "websocket_arginfo.h"

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

	intern->id = zend_string_init("0.0", sizeof("0.0") - 1, false);
	intern->remote_address = zend_string_init("", 0, false);
	intern->open = false;

	intern->std.handlers = &websocket_connection_handlers;

	return &intern->std;
}

static void websocket_connection_free_object(zend_object *object)
{
	websocket_connection_object *intern = websocket_connection_from_obj(object);

	if (intern->id) {
		zend_string_release(intern->id);
	}
	if (intern->remote_address) {
		zend_string_release(intern->remote_address);
	}

	zend_object_std_dtor(&intern->std);
}

static zval *websocket_connection_read_property(zend_object *object, zend_string *name, int type, void **cache_slot, zval *rv)
{
	websocket_connection_object *intern = websocket_connection_from_obj(object);

	if (zend_string_equals_literal(name, "id")) {
		ZVAL_STR_COPY(rv, intern->id);
		return rv;
	}

	if (zend_string_equals_literal(name, "remoteAddress")) {
		ZVAL_STR_COPY(rv, intern->remote_address);
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
}

PHP_METHOD(WebSocket_Connection, close)
{
	zend_long code = 1000;
	zend_string *reason;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(code)
		Z_PARAM_STR(reason)
	ZEND_PARSE_PARAMETERS_END();

	if (code < 1000 || code > 4999) {
		zend_argument_value_error(1, "must be between 1000 and 4999");
		RETURN_THROWS();
	}

	Z_WEBSOCKET_CONNECTION_P(ZEND_THIS)->open = false;
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
