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
#include "Zend/zend_exceptions.h"

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
	ZVAL_UNDEF(&intern->on_handshake);
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
	zval_ptr_dtor(&intern->on_handshake);
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

PHP_METHOD(WebSocket_Server, onHandshake)
{
	zval *handler;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(handler, zend_ce_closure)
	ZEND_PARSE_PARAMETERS_END();

	websocket_server_store_closure(&Z_WEBSOCKET_SERVER_P(ZEND_THIS)->on_handshake, handler);
}

PHP_METHOD(WebSocket_ServerOptions, __construct)
{
	zend_long max_message_size = WEBSOCKET_DEFAULT_MAX_MESSAGE_SIZE;
	zend_long max_queued_bytes = WEBSOCKET_DEFAULT_MAX_QUEUED_BYTES;
	zend_long max_connections = WEBSOCKET_DEFAULT_MAX_CONNECTIONS;
	zend_long handshake_timeout_ms = WEBSOCKET_DEFAULT_HANDSHAKE_TIMEOUT_MS;
	zend_long idle_timeout_ms = WEBSOCKET_DEFAULT_IDLE_TIMEOUT_MS;
	zend_long ping_interval_ms = WEBSOCKET_DEFAULT_PING_INTERVAL_MS;
	zend_long pong_timeout_ms = WEBSOCKET_DEFAULT_PONG_TIMEOUT_MS;

	ZEND_PARSE_PARAMETERS_START(0, 7)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(max_message_size)
		Z_PARAM_LONG(max_queued_bytes)
		Z_PARAM_LONG(max_connections)
		Z_PARAM_LONG(handshake_timeout_ms)
		Z_PARAM_LONG(idle_timeout_ms)
		Z_PARAM_LONG(ping_interval_ms)
		Z_PARAM_LONG(pong_timeout_ms)
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
	if (ping_interval_ms < 0) {
		zend_argument_value_error(6, "must be at least 0");
		RETURN_THROWS();
	}
	if (pong_timeout_ms < 1) {
		zend_argument_value_error(7, "must be at least 1");
		RETURN_THROWS();
	}

	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "maxMessageSize", strlen("maxMessageSize"), max_message_size);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "maxQueuedBytes", strlen("maxQueuedBytes"), max_queued_bytes);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "maxConnections", strlen("maxConnections"), max_connections);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "handshakeTimeoutMs", strlen("handshakeTimeoutMs"), handshake_timeout_ms);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "idleTimeoutMs", strlen("idleTimeoutMs"), idle_timeout_ms);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "pingIntervalMs", strlen("pingIntervalMs"), ping_interval_ms);
	zend_update_property_long(websocket_server_options_ce, Z_OBJ_P(ZEND_THIS), "pongTimeoutMs", strlen("pongTimeoutMs"), pong_timeout_ms);
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

PHP_METHOD(WebSocket_Request, header)
{
	zend_string *name;
	zend_string *lower_name;
	zval *headers;
	zval *value;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(name)
	ZEND_PARSE_PARAMETERS_END();

	lower_name = zend_string_tolower(name);
	headers = zend_read_property(websocket_request_ce, Z_OBJ_P(ZEND_THIS), "headers", strlen("headers"), 0, NULL);
	value = Z_TYPE_P(headers) == IS_ARRAY ? zend_hash_find(Z_ARRVAL_P(headers), lower_name) : NULL;
	zend_string_release(lower_name);

	if (!value) {
		RETURN_NULL();
	}

	RETURN_STR_COPY(Z_STR_P(value));
}

static bool websocket_header_value_is_valid(zend_string *value)
{
	return !memchr(ZSTR_VAL(value), '\r', ZSTR_LEN(value)) && !memchr(ZSTR_VAL(value), '\n', ZSTR_LEN(value));
}

static void websocket_handshake_response_set_properties(zval *object, zend_long status, zval *headers, zend_string *body)
{
	zend_update_property_long(websocket_handshake_response_ce, Z_OBJ_P(object), "status", strlen("status"), status);
	zend_update_property(websocket_handshake_response_ce, Z_OBJ_P(object), "headers", strlen("headers"), headers);
	zend_update_property_str(websocket_handshake_response_ce, Z_OBJ_P(object), "body", strlen("body"), body);
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

PHP_METHOD(WebSocket_HandshakeResponse, __construct)
{
	zend_long status = 403;
	zval *headers = NULL;
	zend_string *body = ZSTR_EMPTY_ALLOC();
	zval normalized;
	zend_string *name;
	zval *value;

	ZEND_PARSE_PARAMETERS_START(0, 3)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(status)
		Z_PARAM_ARRAY(headers)
		Z_PARAM_STR(body)
	ZEND_PARSE_PARAMETERS_END();

	if (status < 100 || status > 599 || status == 101) {
		zend_argument_value_error(1, "must be a valid non-101 HTTP status code");
		RETURN_THROWS();
	}

	array_init(&normalized);
	if (headers) {
		ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(headers), name, value) {
			zval header_value;

			if (!name || !websocket_http_validate_subprotocol_token(ZSTR_VAL(name), ZSTR_LEN(name))) {
				zval_ptr_dtor(&normalized);
				zend_argument_value_error(2, "must contain valid HTTP header names");
				RETURN_THROWS();
			}
			if (Z_TYPE_P(value) != IS_STRING) {
				zval_ptr_dtor(&normalized);
				zend_argument_type_error(2, "must contain string header values, %s given", websocket_zval_value_name(value));
				RETURN_THROWS();
			}
			if (!websocket_header_value_is_valid(Z_STR_P(value))) {
				zval_ptr_dtor(&normalized);
				zend_argument_value_error(2, "must contain HTTP header values without CR or LF");
				RETURN_THROWS();
			}

			ZVAL_STR_COPY(&header_value, Z_STR_P(value));
			zend_hash_add_new(Z_ARRVAL(normalized), name, &header_value);
		} ZEND_HASH_FOREACH_END();
	}

	websocket_handshake_response_set_properties(ZEND_THIS, status, &normalized, body);
	zval_ptr_dtor(&normalized);
}

PHP_METHOD(WebSocket_HandshakeException, __construct)
{
	zval *response = NULL;
	zval default_response;
	bool has_default_response = false;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJECT_OF_CLASS_OR_NULL(response, websocket_handshake_response_ce)
	ZEND_PARSE_PARAMETERS_END();

	if (!response) {
		zval headers;

		object_init_ex(&default_response, websocket_handshake_response_ce);
		array_init(&headers);
		websocket_handshake_response_set_properties(&default_response, 403, &headers, ZSTR_EMPTY_ALLOC());
		zval_ptr_dtor(&headers);
		response = &default_response;
		has_default_response = true;
	}

	zend_update_property(websocket_handshake_exception_ce, Z_OBJ_P(ZEND_THIS), "response", strlen("response"), response);
	zend_update_property_string(zend_ce_exception, Z_OBJ_P(ZEND_THIS), "message", strlen("message"), "WebSocket handshake rejected");

	if (has_default_response) {
		zval_ptr_dtor(&default_response);
	}
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
	websocket_request_ce = register_class_WebSocket_Request();
	websocket_handshake_response_ce = register_class_WebSocket_HandshakeResponse();
	websocket_handshake_exception_ce = register_class_WebSocket_HandshakeException(zend_ce_exception);
	websocket_server_ce->create_object = websocket_server_create_object;

	memcpy(&websocket_server_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	websocket_server_handlers.offset = XtOffsetOf(websocket_server_object, std);
	websocket_server_handlers.free_obj = websocket_server_free_object;
	websocket_server_handlers.clone_obj = NULL;
	WEBSOCKET_SET_DEFAULT_HANDLERS(websocket_server_ce, &websocket_server_handlers);
}
