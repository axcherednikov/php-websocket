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

#include <string.h>

static zend_object_handlers websocket_server_handlers;

websocket_server_object *websocket_server_from_obj(zend_object *obj)
{
	return (websocket_server_object *)((char *)(obj) - XtOffsetOf(websocket_server_object, std));
}

static zend_object *websocket_server_create_object(zend_class_entry *ce)
{
	websocket_server_object *intern = zend_object_alloc(sizeof(websocket_server_object), ce);

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);

	ZVAL_UNDEF(&intern->options);
	ZVAL_UNDEF(&intern->subprotocols);
	ZVAL_UNDEF(&intern->on_open);
	ZVAL_UNDEF(&intern->on_message);
	ZVAL_UNDEF(&intern->on_close);
	ZVAL_UNDEF(&intern->on_error);
	memset(&intern->on_open_cache, 0, sizeof(intern->on_open_cache));
	intern->on_open_param_count = 1;
	intern->on_open_cache_initialized = false;
	intern->host = NULL;
	intern->port = 0;
	intern->listening = false;
	intern->running = false;
	intern->listener_fd = -1;
	ZVAL_UNDEF(&intern->reusable_connection);
	intern->connections = NULL;
	intern->connection_count = 0;
	intern->connection_capacity = 0;

	intern->std.handlers = &websocket_server_handlers;

	return &intern->std;
}

static void websocket_server_free_object(zend_object *object)
{
	websocket_server_object *intern = websocket_server_from_obj(object);

	zval_ptr_dtor(&intern->options);
	zval_ptr_dtor(&intern->subprotocols);
	zval_ptr_dtor(&intern->on_open);
	zval_ptr_dtor(&intern->on_message);
	zval_ptr_dtor(&intern->on_close);
	zval_ptr_dtor(&intern->on_error);

	websocket_server_runtime_free(intern);

	if (intern->host) {
		zend_string_release(intern->host);
	}

	zend_object_std_dtor(&intern->std);
}

static void websocket_server_store_closure(zval *slot, zval *handler)
{
	zval_ptr_dtor(slot);
	ZVAL_COPY(slot, handler);
}

PHP_METHOD(WebSocket_Server, __construct)
{
	zval *options = NULL;
	websocket_server_object *intern = Z_WEBSOCKET_SERVER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_ZVAL(options)
	ZEND_PARSE_PARAMETERS_END();

	if (options) {
		if (Z_TYPE_P(options) != IS_ARRAY && (Z_TYPE_P(options) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(options), websocket_server_options_ce))) {
			zend_argument_type_error(1, "must be of type array|WebSocket\\ServerOptions, %s given", websocket_zval_value_name(options));
			RETURN_THROWS();
		}
		ZVAL_COPY(&intern->options, options);
	} else {
		array_init(&intern->options);
	}

	array_init(&intern->subprotocols);
}

PHP_METHOD(WebSocket_Server, subprotocols)
{
	zval *protocols;
	uint32_t protocol_count;
	zval normalized;
	HashTable seen;
	websocket_server_object *intern = Z_WEBSOCKET_SERVER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', protocols, protocol_count)
	ZEND_PARSE_PARAMETERS_END();

	if (intern->running) {
		zend_throw_error(NULL, "Cannot change WebSocket subprotocols while the server is running");
		RETURN_THROWS();
	}

	array_init(&normalized);
	zend_hash_init(&seen, protocol_count, NULL, NULL, 0);

	for (uint32_t i = 0; i < protocol_count; i++) {
		zval *protocol = &protocols[i];
		zend_string *protocol_str;
		zval value;

		if (Z_TYPE_P(protocol) != IS_STRING) {
			zend_hash_destroy(&seen);
			zval_ptr_dtor(&normalized);
			zend_argument_type_error(i + 1, "must be of type string, %s given", websocket_zval_value_name(protocol));
			RETURN_THROWS();
		}

		protocol_str = Z_STR_P(protocol);
		if (!websocket_http_validate_subprotocol_token(ZSTR_VAL(protocol_str), ZSTR_LEN(protocol_str))) {
			zend_hash_destroy(&seen);
			zval_ptr_dtor(&normalized);
			zend_argument_value_error(i + 1, "must be a valid WebSocket subprotocol token");
			RETURN_THROWS();
		}

		if (zend_hash_exists(&seen, protocol_str)) {
			zend_hash_destroy(&seen);
			zval_ptr_dtor(&normalized);
			zend_argument_value_error(i + 1, "must not duplicate a previous subprotocol");
			RETURN_THROWS();
		}

		zend_hash_add_empty_element(&seen, protocol_str);
		ZVAL_STR_COPY(&value, protocol_str);
		zend_hash_add_new(Z_ARRVAL(normalized), protocol_str, &value);
	}

	zend_hash_destroy(&seen);
	zval_ptr_dtor(&intern->subprotocols);
	ZVAL_COPY_VALUE(&intern->subprotocols, &normalized);
}

PHP_METHOD(WebSocket_ServerOptions, __construct)
{
	zend_long max_message_size = WEBSOCKET_DEFAULT_MAX_MESSAGE_SIZE;
	zend_long max_queued_bytes = WEBSOCKET_DEFAULT_MAX_QUEUED_BYTES;
	zend_long max_connections = WEBSOCKET_DEFAULT_MAX_CONNECTIONS;
	zend_long handshake_timeout_ms = WEBSOCKET_DEFAULT_HANDSHAKE_TIMEOUT_MS;
	zend_long idle_timeout_ms = WEBSOCKET_DEFAULT_IDLE_TIMEOUT_MS;

	ZEND_PARSE_PARAMETERS_START(0, 5)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(max_message_size)
		Z_PARAM_LONG(max_queued_bytes)
		Z_PARAM_LONG(max_connections)
		Z_PARAM_LONG(handshake_timeout_ms)
		Z_PARAM_LONG(idle_timeout_ms)
	ZEND_PARSE_PARAMETERS_END();

	if (max_message_size < 1) {
		zend_argument_value_error(1, "must be at least 1");
		RETURN_THROWS();
	}
	if (max_queued_bytes < 1) {
		zend_argument_value_error(2, "must be at least 1");
		RETURN_THROWS();
	}
	if (max_connections < 1) {
		zend_argument_value_error(3, "must be at least 1");
		RETURN_THROWS();
	}
	if (handshake_timeout_ms < 1) {
		zend_argument_value_error(4, "must be at least 1");
		RETURN_THROWS();
	}
	if (idle_timeout_ms < 1) {
		zend_argument_value_error(5, "must be at least 1");
		RETURN_THROWS();
	}

	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "maxMessageSize", strlen("maxMessageSize"), max_message_size);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "maxQueuedBytes", strlen("maxQueuedBytes"), max_queued_bytes);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "maxConnections", strlen("maxConnections"), max_connections);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "handshakeTimeoutMs", strlen("handshakeTimeoutMs"), handshake_timeout_ms);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "idleTimeoutMs", strlen("idleTimeoutMs"), idle_timeout_ms);
}

PHP_METHOD(WebSocket_Server, listen)
{
	zend_string *host;
	zend_long port;
	websocket_server_object *intern = Z_WEBSOCKET_SERVER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(host)
		Z_PARAM_LONG(port)
	ZEND_PARSE_PARAMETERS_END();

	if (port < 1 || port > 65535) {
		zend_argument_value_error(2, "must be between 1 and 65535");
		RETURN_THROWS();
	}
	if (strlen(ZSTR_VAL(host)) != ZSTR_LEN(host)) {
		zend_argument_value_error(1, "must not contain null bytes");
		RETURN_THROWS();
	}

	if (intern->host) {
		zend_string_release(intern->host);
	}

	intern->host = zend_string_copy(host);
	intern->port = port;
	intern->listening = true;
}

PHP_METHOD(WebSocket_Server, onOpen)
{
	zval *handler;
	websocket_server_object *intern = Z_WEBSOCKET_SERVER_P(ZEND_THIS);
	const zend_function *function;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	websocket_server_store_closure(&intern->on_open, handler);
	function = zend_get_closure_method_def(Z_OBJ_P(handler));
	intern->on_open_param_count = function && function->common.num_args == 0 && (function->common.fn_flags & ZEND_ACC_VARIADIC) == 0 ? 0 : 1;
	intern->on_open_cache.function_handler = (zend_function *) function;
	intern->on_open_cache.calling_scope = NULL;
	intern->on_open_cache.called_scope = Z_OBJCE_P(handler);
	intern->on_open_cache.object = Z_OBJ_P(handler);
	intern->on_open_cache_initialized = function != NULL;
}

PHP_METHOD(WebSocket_Server, onMessage)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	websocket_server_store_closure(&Z_WEBSOCKET_SERVER_P(ZEND_THIS)->on_message, handler);
}

PHP_METHOD(WebSocket_Server, onClose)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	websocket_server_store_closure(&Z_WEBSOCKET_SERVER_P(ZEND_THIS)->on_close, handler);
}

PHP_METHOD(WebSocket_Server, onError)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	websocket_server_store_closure(&Z_WEBSOCKET_SERVER_P(ZEND_THIS)->on_error, handler);
}

PHP_METHOD(WebSocket_Server, run)
{
	websocket_server_object *intern = Z_WEBSOCKET_SERVER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_NONE();

	if (UNEXPECTED(WEBSOCKET_G(running))) {
		zend_throw_error(NULL, "The WebSocket server is already running");
		RETURN_THROWS();
	}

	if (!intern->listening) {
		zend_throw_error(NULL, "Cannot run WebSocket server before listen()");
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

	(void) websocket_server_runtime_run(intern);
	intern->running = false;
	WEBSOCKET_G(running) = false;

	if (EG(exception)) {
		RETURN_THROWS();
	}
}

PHP_METHOD(WebSocket_Server, stop)
{
	ZEND_PARSE_PARAMETERS_NONE();

	Z_WEBSOCKET_SERVER_P(ZEND_THIS)->running = false;
	WEBSOCKET_G(stopped) = true;
}

PHP_METHOD(WebSocket_Server, getDriver)
{
	ZEND_PARSE_PARAMETERS_NONE();

	if (WEBSOCKET_G(driver)) {
		RETURN_STRING(WEBSOCKET_G(driver)->name);
	}

	RETURN_STRING(websocket_best_driver_name());
}

void websocket_register_server_class(void)
{
	websocket_server_ce = register_class_WebSocket_Server();
	websocket_server_options_ce = register_class_WebSocket_ServerOptions();
	websocket_server_ce->create_object = websocket_server_create_object;

	memcpy(&websocket_server_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	websocket_server_handlers.offset = XtOffsetOf(websocket_server_object, std);
	websocket_server_handlers.free_obj = websocket_server_free_object;
	websocket_server_handlers.clone_obj = NULL;
	WEBSOCKET_SET_DEFAULT_HANDLERS(websocket_server_ce, &websocket_server_handlers);
}
