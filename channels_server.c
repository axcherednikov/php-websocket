#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_websocket.h"
#include "php_websocket_compat.h"
#include "websocket_arginfo.h"

static zend_object_handlers channels_server_handlers;

channels_server_object *channels_server_from_obj(zend_object *obj)
{
	return (channels_server_object *)((char *)(obj) - XtOffsetOf(channels_server_object, std));
}

static zend_object *channels_server_create_object(zend_class_entry *ce)
{
	channels_server_object *intern = zend_object_alloc(sizeof(channels_server_object), ce);

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);

	ZVAL_UNDEF(&intern->apps);
	ZVAL_UNDEF(&intern->options);
	ZVAL_UNDEF(&intern->on_connection);
	ZVAL_UNDEF(&intern->on_subscribe);
	ZVAL_UNDEF(&intern->on_unsubscribe);
	ZVAL_UNDEF(&intern->on_client_event);
	intern->host = NULL;
	intern->port = 0;
	intern->listening = false;
	intern->running = false;

	intern->std.handlers = &channels_server_handlers;

	return &intern->std;
}

static void channels_server_free_object(zend_object *object)
{
	channels_server_object *intern = channels_server_from_obj(object);

	zval_ptr_dtor(&intern->apps);
	zval_ptr_dtor(&intern->options);
	zval_ptr_dtor(&intern->on_connection);
	zval_ptr_dtor(&intern->on_subscribe);
	zval_ptr_dtor(&intern->on_unsubscribe);
	zval_ptr_dtor(&intern->on_client_event);

	if (intern->host) {
		zend_string_release(intern->host);
	}

	zend_object_std_dtor(&intern->std);
}

static void channels_server_store_closure(zval *slot, zval *handler)
{
	zval_ptr_dtor(slot);
	ZVAL_COPY(slot, handler);
}

static void channels_return_empty_result(INTERNAL_FUNCTION_PARAMETERS)
{
	object_init(return_value);
}

PHP_METHOD(Channels_Server, __construct)
{
	zval *apps;
	zval *options = NULL;
	channels_server_object *intern = Z_CHANNELS_SERVER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ARRAY(apps)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(options)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_COPY(&intern->apps, apps);
	if (options) {
		ZVAL_COPY(&intern->options, options);
	} else {
		array_init(&intern->options);
	}
}

PHP_METHOD(Channels_Server, listen)
{
	zend_string *host;
	zend_long port;
	channels_server_object *intern = Z_CHANNELS_SERVER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(host)
		Z_PARAM_LONG(port)
	ZEND_PARSE_PARAMETERS_END();

	if (port < 1 || port > 65535) {
		zend_argument_value_error(2, "must be between 1 and 65535");
		RETURN_THROWS();
	}

	if (intern->host) {
		zend_string_release(intern->host);
	}

	intern->host = zend_string_copy(host);
	intern->port = port;
	intern->listening = true;
}

PHP_METHOD(Channels_Server, onConnection)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	channels_server_store_closure(&Z_CHANNELS_SERVER_P(ZEND_THIS)->on_connection, handler);
}

PHP_METHOD(Channels_Server, onSubscribe)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	channels_server_store_closure(&Z_CHANNELS_SERVER_P(ZEND_THIS)->on_subscribe, handler);
}

PHP_METHOD(Channels_Server, onUnsubscribe)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	channels_server_store_closure(&Z_CHANNELS_SERVER_P(ZEND_THIS)->on_unsubscribe, handler);
}

PHP_METHOD(Channels_Server, onClientEvent)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	channels_server_store_closure(&Z_CHANNELS_SERVER_P(ZEND_THIS)->on_client_event, handler);
}

PHP_METHOD(Channels_Server, trigger)
{
	zval *channels;
	zend_string *event;
	zval *data;
	zval *params = NULL;
	bool already_encoded = false;

	ZEND_PARSE_PARAMETERS_START(3, 5)
		Z_PARAM_ZVAL(channels)
		Z_PARAM_STR(event)
		Z_PARAM_ZVAL(data)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(params)
		Z_PARAM_BOOL(already_encoded)
	ZEND_PARSE_PARAMETERS_END();

	if (Z_TYPE_P(channels) != IS_STRING && Z_TYPE_P(channels) != IS_ARRAY) {
		zend_argument_type_error(1, "must be of type string|array, %s given", zend_zval_value_name(channels));
		RETURN_THROWS();
	}

	channels_return_empty_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(Channels_Server, triggerBatch)
{
	zval *batch = NULL;
	bool already_encoded = false;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(batch)
		Z_PARAM_BOOL(already_encoded)
	ZEND_PARSE_PARAMETERS_END();

	channels_return_empty_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(Channels_Server, getChannelInfo)
{
	zend_string *channel;
	zval *params = NULL;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR(channel)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(params)
	ZEND_PARSE_PARAMETERS_END();

	channels_return_empty_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(Channels_Server, getChannels)
{
	zval *params = NULL;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(params)
	ZEND_PARSE_PARAMETERS_END();

	channels_return_empty_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(Channels_Server, getPresenceUsers)
{
	zend_string *channel;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(channel)
	ZEND_PARSE_PARAMETERS_END();

	channels_return_empty_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(Channels_Server, terminateUserConnections)
{
	zend_string *user_id;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(user_id)
	ZEND_PARSE_PARAMETERS_END();

	channels_return_empty_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(Channels_Server, run)
{
	channels_server_object *intern = Z_CHANNELS_SERVER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_NONE();

	if (UNEXPECTED(WEBSOCKET_G(running))) {
		zend_throw_error(NULL, "The Channels server is already running");
		RETURN_THROWS();
	}

	if (!intern->listening) {
		zend_throw_error(NULL, "Cannot run Channels server before listen()");
		RETURN_THROWS();
	}

	if (!WEBSOCKET_G(driver)) {
		WEBSOCKET_G(driver) = websocket_select_best_driver();
		if (UNEXPECTED(!WEBSOCKET_G(driver))) {
			zend_throw_error(NULL, "No suitable I/O driver available");
			RETURN_THROWS();
		}
	}

	WEBSOCKET_G(running) = true;
	WEBSOCKET_G(stopped) = false;
	intern->running = true;

	/* Phase 1 will replace this placeholder with listener accept/poll. */
	intern->running = false;
	WEBSOCKET_G(running) = false;
}

PHP_METHOD(Channels_Server, stop)
{
	ZEND_PARSE_PARAMETERS_NONE();

	Z_CHANNELS_SERVER_P(ZEND_THIS)->running = false;
	WEBSOCKET_G(stopped) = true;
}

void websocket_register_channels_server_class(void)
{
	channels_server_ce = register_class_Channels_Server();
	channels_server_ce->create_object = channels_server_create_object;

	memcpy(&channels_server_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	channels_server_handlers.offset = XtOffsetOf(channels_server_object, std);
	channels_server_handlers.free_obj = channels_server_free_object;
	channels_server_handlers.clone_obj = NULL;
	WEBSOCKET_SET_DEFAULT_HANDLERS(channels_server_ce, &channels_server_handlers);
}
