/* Generated-by-hand bootstrap arginfo.
 * Keep this compatible with PHP 8.1+ until gen_stub.php can run in CI. */

#include "Zend/zend_enum.h"

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_WebSocket_Server___construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_listen, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, host, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, port, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_onOpen, 0, 1, IS_VOID, 0)
	ZEND_ARG_OBJ_INFO(0, handler, Closure, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_WebSocket_Server_onMessage arginfo_class_WebSocket_Server_onOpen
#define arginfo_class_WebSocket_Server_onClose arginfo_class_WebSocket_Server_onOpen
#define arginfo_class_WebSocket_Server_onError arginfo_class_WebSocket_Server_onOpen

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_send, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, connection, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, payload, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, type, IS_OBJECT, 0, "WebSocket\\MessageType::Text")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_close, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, connection, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, code, IS_LONG, 0, "1000")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reason, IS_STRING, 0, "''")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_run, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_WebSocket_Server_stop arginfo_class_WebSocket_Server_run

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_getDriver, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Connection_send, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, payload, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Connection_close, 0, 0, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, code, IS_LONG, 0, "1000")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reason, IS_STRING, 0, "''")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Connection_isOpen, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Channels_Server___construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, apps, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

#define arginfo_class_Channels_Server_listen arginfo_class_WebSocket_Server_listen
#define arginfo_class_Channels_Server_onConnection arginfo_class_WebSocket_Server_onOpen
#define arginfo_class_Channels_Server_onSubscribe arginfo_class_WebSocket_Server_onOpen
#define arginfo_class_Channels_Server_onUnsubscribe arginfo_class_WebSocket_Server_onOpen
#define arginfo_class_Channels_Server_onClientEvent arginfo_class_WebSocket_Server_onOpen

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Channels_Server_trigger, 0, 3, IS_OBJECT, 0)
	ZEND_ARG_TYPE_MASK(0, channels, MAY_BE_STRING|MAY_BE_ARRAY, NULL)
	ZEND_ARG_TYPE_INFO(0, event, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 0, "[]")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, alreadyEncoded, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Channels_Server_triggerBatch, 0, 0, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, batch, IS_ARRAY, 0, "[]")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, alreadyEncoded, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Channels_Server_getChannelInfo, 0, 1, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, channel, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Channels_Server_getChannels, 0, 0, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Channels_Server_getPresenceUsers, 0, 1, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, channel, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Channels_Server_terminateUserConnections, 0, 1, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, userId, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Channels_Server_run arginfo_class_WebSocket_Server_run
#define arginfo_class_Channels_Server_stop arginfo_class_WebSocket_Server_run

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Channels_App___construct, 0, 0, 3)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, secret, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, id, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_METHOD(WebSocket_Server, __construct);
ZEND_METHOD(WebSocket_Server, listen);
ZEND_METHOD(WebSocket_Server, onOpen);
ZEND_METHOD(WebSocket_Server, onMessage);
ZEND_METHOD(WebSocket_Server, onClose);
ZEND_METHOD(WebSocket_Server, onError);
ZEND_METHOD(WebSocket_Server, send);
ZEND_METHOD(WebSocket_Server, close);
ZEND_METHOD(WebSocket_Server, run);
ZEND_METHOD(WebSocket_Server, stop);
ZEND_METHOD(WebSocket_Server, getDriver);
ZEND_METHOD(WebSocket_Connection, send);
ZEND_METHOD(WebSocket_Connection, close);
ZEND_METHOD(WebSocket_Connection, isOpen);
ZEND_METHOD(Channels_Server, __construct);
ZEND_METHOD(Channels_Server, listen);
ZEND_METHOD(Channels_Server, onConnection);
ZEND_METHOD(Channels_Server, onSubscribe);
ZEND_METHOD(Channels_Server, onUnsubscribe);
ZEND_METHOD(Channels_Server, onClientEvent);
ZEND_METHOD(Channels_Server, trigger);
ZEND_METHOD(Channels_Server, triggerBatch);
ZEND_METHOD(Channels_Server, getChannelInfo);
ZEND_METHOD(Channels_Server, getChannels);
ZEND_METHOD(Channels_Server, getPresenceUsers);
ZEND_METHOD(Channels_Server, terminateUserConnections);
ZEND_METHOD(Channels_Server, run);
ZEND_METHOD(Channels_Server, stop);
ZEND_METHOD(Channels_App, __construct);

static const zend_function_entry class_WebSocket_Server_methods[] = {
	ZEND_ME(WebSocket_Server, __construct, arginfo_class_WebSocket_Server___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, listen, arginfo_class_WebSocket_Server_listen, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onOpen, arginfo_class_WebSocket_Server_onOpen, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onMessage, arginfo_class_WebSocket_Server_onMessage, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onClose, arginfo_class_WebSocket_Server_onClose, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onError, arginfo_class_WebSocket_Server_onError, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, send, arginfo_class_WebSocket_Server_send, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, close, arginfo_class_WebSocket_Server_close, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, run, arginfo_class_WebSocket_Server_run, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, stop, arginfo_class_WebSocket_Server_stop, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, getDriver, arginfo_class_WebSocket_Server_getDriver, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_WebSocket_Connection_methods[] = {
	ZEND_ME(WebSocket_Connection, send, arginfo_class_WebSocket_Connection_send, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Connection, close, arginfo_class_WebSocket_Connection_close, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Connection, isOpen, arginfo_class_WebSocket_Connection_isOpen, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_Channels_Server_methods[] = {
	ZEND_ME(Channels_Server, __construct, arginfo_class_Channels_Server___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, listen, arginfo_class_Channels_Server_listen, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, onConnection, arginfo_class_Channels_Server_onConnection, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, onSubscribe, arginfo_class_Channels_Server_onSubscribe, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, onUnsubscribe, arginfo_class_Channels_Server_onUnsubscribe, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, onClientEvent, arginfo_class_Channels_Server_onClientEvent, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, trigger, arginfo_class_Channels_Server_trigger, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, triggerBatch, arginfo_class_Channels_Server_triggerBatch, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, getChannelInfo, arginfo_class_Channels_Server_getChannelInfo, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, getChannels, arginfo_class_Channels_Server_getChannels, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, getPresenceUsers, arginfo_class_Channels_Server_getPresenceUsers, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, terminateUserConnections, arginfo_class_Channels_Server_terminateUserConnections, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, run, arginfo_class_Channels_Server_run, ZEND_ACC_PUBLIC)
	ZEND_ME(Channels_Server, stop, arginfo_class_Channels_Server_stop, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_Channels_App_methods[] = {
	ZEND_ME(Channels_App, __construct, arginfo_class_Channels_App___construct, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_WebSocket_Server(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "Server", class_WebSocket_Server_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_Connection(void)
{
	zend_class_entry ce, *class_entry;
	zval property_id_default_value;
	zval property_remoteAddress_default_value;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "Connection", class_WebSocket_Connection_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	ZVAL_UNDEF(&property_id_default_value);
	zend_declare_typed_property(class_entry, zend_string_init("id", sizeof("id") - 1, 1), &property_id_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING));

	ZVAL_UNDEF(&property_remoteAddress_default_value);
	zend_declare_typed_property(class_entry, zend_string_init("remoteAddress", sizeof("remoteAddress") - 1, 1), &property_remoteAddress_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING));

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_MessageType(void)
{
	zend_class_entry *class_entry = zend_register_internal_enum("WebSocket\\MessageType", IS_UNDEF, NULL);

	zend_enum_add_case_cstr(class_entry, "Text", NULL);
	zend_enum_add_case_cstr(class_entry, "Binary", NULL);
	zend_enum_add_case_cstr(class_entry, "Ping", NULL);
	zend_enum_add_case_cstr(class_entry, "Pong", NULL);
	zend_enum_add_case_cstr(class_entry, "Close", NULL);

	return class_entry;
}

static zend_class_entry *register_class_Channels_Server(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Channels", "Server", class_Channels_Server_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	return class_entry;
}

static zend_class_entry *register_class_Channels_App(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Channels", "App", class_Channels_App_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	return class_entry;
}
