/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 0b1d9b784f9f267416396b70e5c2d1523bc87820 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_WebSocket_Server___construct, 0, 0, 0)
	ZEND_ARG_OBJ_TYPE_MASK(0, options, WebSocket\\ServerOptions, MAY_BE_ARRAY, "[]")
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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_run, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_WebSocket_Server_stop arginfo_class_WebSocket_Server_run

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Server_getDriver, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_WebSocket_ServerOptions___construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxMessageSize, IS_LONG, 0, "16 * 1024 * 1024")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxQueuedBytes, IS_LONG, 0, "16 * 1024 * 1024")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxConnections, IS_LONG, 0, "10000")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, handshakeTimeoutMs, IS_LONG, 0, "10000")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, idleTimeoutMs, IS_LONG, 0, "120000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Connection_send, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, payload, IS_STRING, 0)
	ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(0, type, WebSocket\\MessageType, 0, "WebSocket\\MessageType::Text")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Connection_close, 0, 0, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, code, IS_LONG, 0, "1000")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reason, IS_STRING, 0, "\'\'")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Connection_isOpen, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_WebSocket_Frame___construct, 0, 0, 2)
	ZEND_ARG_OBJ_INFO(0, type, WebSocket\\MessageType, 0)
	ZEND_ARG_TYPE_INFO(0, payload, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, final, _IS_BOOL, 0, "true")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_WebSocket_CloseFrame___construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, code, IS_LONG, 0, "WebSocket\\Protocol::CLOSE_NORMAL")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reason, IS_STRING, 0, "\'\'")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Protocol_acceptKey, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Protocol_encode, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, payload, IS_STRING, 0)
	ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(0, type, WebSocket\\MessageType, 0, "WebSocket\\MessageType::Text")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, masked, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX(arginfo_class_WebSocket_Protocol_decode, 0, 1, WebSocket\\Frame|WebSocket\\CloseFrame, MAY_BE_NULL)
	ZEND_ARG_TYPE_INFO(0, buffer, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_WebSocket_Protocol_pack, 0, 1, IS_STRING, 0)
	ZEND_ARG_OBJ_TYPE_MASK(0, data, WebSocket\\Frame|WebSocket\\CloseFrame, MAY_BE_STRING, NULL)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, opcode, IS_LONG, 0, "WebSocket\\Protocol::OPCODE_TEXT")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "WebSocket\\Protocol::FLAG_FIN")
ZEND_END_ARG_INFO()

#define arginfo_class_WebSocket_Protocol_unpack arginfo_class_WebSocket_Protocol_decode

ZEND_METHOD(WebSocket_Server, __construct);
ZEND_METHOD(WebSocket_Server, listen);
ZEND_METHOD(WebSocket_Server, onOpen);
ZEND_METHOD(WebSocket_Server, onMessage);
ZEND_METHOD(WebSocket_Server, onClose);
ZEND_METHOD(WebSocket_Server, onError);
ZEND_METHOD(WebSocket_Server, run);
ZEND_METHOD(WebSocket_Server, stop);
ZEND_METHOD(WebSocket_Server, getDriver);
ZEND_METHOD(WebSocket_ServerOptions, __construct);
ZEND_METHOD(WebSocket_Connection, send);
ZEND_METHOD(WebSocket_Connection, close);
ZEND_METHOD(WebSocket_Connection, isOpen);
ZEND_METHOD(WebSocket_Frame, __construct);
ZEND_METHOD(WebSocket_CloseFrame, __construct);
ZEND_METHOD(WebSocket_Protocol, acceptKey);
ZEND_METHOD(WebSocket_Protocol, encode);
ZEND_METHOD(WebSocket_Protocol, decode);
ZEND_METHOD(WebSocket_Protocol, pack);
ZEND_METHOD(WebSocket_Protocol, unpack);

static const zend_function_entry class_WebSocket_Server_methods[] = {
	ZEND_ME(WebSocket_Server, __construct, arginfo_class_WebSocket_Server___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, listen, arginfo_class_WebSocket_Server_listen, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onOpen, arginfo_class_WebSocket_Server_onOpen, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onMessage, arginfo_class_WebSocket_Server_onMessage, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onClose, arginfo_class_WebSocket_Server_onClose, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, onError, arginfo_class_WebSocket_Server_onError, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, run, arginfo_class_WebSocket_Server_run, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, stop, arginfo_class_WebSocket_Server_stop, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Server, getDriver, arginfo_class_WebSocket_Server_getDriver, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_WebSocket_ServerOptions_methods[] = {
	ZEND_ME(WebSocket_ServerOptions, __construct, arginfo_class_WebSocket_ServerOptions___construct, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_WebSocket_Connection_methods[] = {
	ZEND_ME(WebSocket_Connection, send, arginfo_class_WebSocket_Connection_send, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Connection, close, arginfo_class_WebSocket_Connection_close, ZEND_ACC_PUBLIC)
	ZEND_ME(WebSocket_Connection, isOpen, arginfo_class_WebSocket_Connection_isOpen, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_WebSocket_Frame_methods[] = {
	ZEND_ME(WebSocket_Frame, __construct, arginfo_class_WebSocket_Frame___construct, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_WebSocket_CloseFrame_methods[] = {
	ZEND_ME(WebSocket_CloseFrame, __construct, arginfo_class_WebSocket_CloseFrame___construct, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_WebSocket_Protocol_methods[] = {
	ZEND_ME(WebSocket_Protocol, acceptKey, arginfo_class_WebSocket_Protocol_acceptKey, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(WebSocket_Protocol, encode, arginfo_class_WebSocket_Protocol_encode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(WebSocket_Protocol, decode, arginfo_class_WebSocket_Protocol_decode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(WebSocket_Protocol, pack, arginfo_class_WebSocket_Protocol_pack, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(WebSocket_Protocol, unpack, arginfo_class_WebSocket_Protocol_unpack, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_WebSocket_Server(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "Server", class_WebSocket_Server_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_ServerOptions(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "ServerOptions", class_WebSocket_ServerOptions_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval property_maxMessageSize_default_value;
	ZVAL_UNDEF(&property_maxMessageSize_default_value);
	zend_string *property_maxMessageSize_name = zend_string_init("maxMessageSize", sizeof("maxMessageSize") - 1, 1);
	zend_declare_typed_property(class_entry, property_maxMessageSize_name, &property_maxMessageSize_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_maxMessageSize_name);

	zval property_maxQueuedBytes_default_value;
	ZVAL_UNDEF(&property_maxQueuedBytes_default_value);
	zend_string *property_maxQueuedBytes_name = zend_string_init("maxQueuedBytes", sizeof("maxQueuedBytes") - 1, 1);
	zend_declare_typed_property(class_entry, property_maxQueuedBytes_name, &property_maxQueuedBytes_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_maxQueuedBytes_name);

	zval property_maxConnections_default_value;
	ZVAL_UNDEF(&property_maxConnections_default_value);
	zend_string *property_maxConnections_name = zend_string_init("maxConnections", sizeof("maxConnections") - 1, 1);
	zend_declare_typed_property(class_entry, property_maxConnections_name, &property_maxConnections_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_maxConnections_name);

	zval property_handshakeTimeoutMs_default_value;
	ZVAL_UNDEF(&property_handshakeTimeoutMs_default_value);
	zend_string *property_handshakeTimeoutMs_name = zend_string_init("handshakeTimeoutMs", sizeof("handshakeTimeoutMs") - 1, 1);
	zend_declare_typed_property(class_entry, property_handshakeTimeoutMs_name, &property_handshakeTimeoutMs_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_handshakeTimeoutMs_name);

	zval property_idleTimeoutMs_default_value;
	ZVAL_UNDEF(&property_idleTimeoutMs_default_value);
	zend_string *property_idleTimeoutMs_name = zend_string_init("idleTimeoutMs", sizeof("idleTimeoutMs") - 1, 1);
	zend_declare_typed_property(class_entry, property_idleTimeoutMs_name, &property_idleTimeoutMs_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_idleTimeoutMs_name);

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_Connection(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "Connection", class_WebSocket_Connection_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval property_id_default_value;
	ZVAL_UNDEF(&property_id_default_value);
	zend_string *property_id_name = zend_string_init("id", sizeof("id") - 1, 1);
	zend_declare_typed_property(class_entry, property_id_name, &property_id_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING));
	zend_string_release(property_id_name);

	zval property_remoteAddress_default_value;
	ZVAL_UNDEF(&property_remoteAddress_default_value);
	zend_string *property_remoteAddress_name = zend_string_init("remoteAddress", sizeof("remoteAddress") - 1, 1);
	zend_declare_typed_property(class_entry, property_remoteAddress_name, &property_remoteAddress_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING));
	zend_string_release(property_remoteAddress_name);

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_MessageType(void)
{
	zend_class_entry *class_entry = zend_register_internal_enum("WebSocket\\MessageType", IS_UNDEF, NULL);

	zend_enum_add_case_cstr(class_entry, "Continuation", NULL);

	zend_enum_add_case_cstr(class_entry, "Text", NULL);

	zend_enum_add_case_cstr(class_entry, "Binary", NULL);

	zend_enum_add_case_cstr(class_entry, "Ping", NULL);

	zend_enum_add_case_cstr(class_entry, "Pong", NULL);

	zend_enum_add_case_cstr(class_entry, "Close", NULL);

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_Frame(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "Frame", class_WebSocket_Frame_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval property_type_default_value;
	ZVAL_UNDEF(&property_type_default_value);
	zend_string *property_type_name = zend_string_init("type", sizeof("type") - 1, 1);
	zend_string *property_type_class_WebSocket_MessageType = zend_string_init("WebSocket\\MessageType", sizeof("WebSocket\\MessageType")-1, 1);
	zend_declare_typed_property(class_entry, property_type_name, &property_type_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_CLASS(property_type_class_WebSocket_MessageType, 0, 0));
	zend_string_release(property_type_name);

	zval property_opcode_default_value;
	ZVAL_UNDEF(&property_opcode_default_value);
	zend_string *property_opcode_name = zend_string_init("opcode", sizeof("opcode") - 1, 1);
	zend_declare_typed_property(class_entry, property_opcode_name, &property_opcode_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_opcode_name);

	zval property_flags_default_value;
	ZVAL_UNDEF(&property_flags_default_value);
	zend_string *property_flags_name = zend_string_init("flags", sizeof("flags") - 1, 1);
	zend_declare_typed_property(class_entry, property_flags_name, &property_flags_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_flags_name);

	zval property_payload_default_value;
	ZVAL_UNDEF(&property_payload_default_value);
	zend_string *property_payload_name = zend_string_init("payload", sizeof("payload") - 1, 1);
	zend_declare_typed_property(class_entry, property_payload_name, &property_payload_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING));
	zend_string_release(property_payload_name);

	zval property_final_default_value;
	ZVAL_UNDEF(&property_final_default_value);
	zend_string *property_final_name = zend_string_init("final", sizeof("final") - 1, 1);
	zend_declare_typed_property(class_entry, property_final_name, &property_final_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_BOOL));
	zend_string_release(property_final_name);

	zval property_bytesConsumed_default_value;
	ZVAL_UNDEF(&property_bytesConsumed_default_value);
	zend_string *property_bytesConsumed_name = zend_string_init("bytesConsumed", sizeof("bytesConsumed") - 1, 1);
	zend_declare_typed_property(class_entry, property_bytesConsumed_name, &property_bytesConsumed_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_bytesConsumed_name);

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_CloseFrame(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "CloseFrame", class_WebSocket_CloseFrame_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval property_code_default_value;
	ZVAL_UNDEF(&property_code_default_value);
	zend_string *property_code_name = zend_string_init("code", sizeof("code") - 1, 1);
	zend_declare_typed_property(class_entry, property_code_name, &property_code_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_code_name);

	zval property_reason_default_value;
	ZVAL_UNDEF(&property_reason_default_value);
	zend_string *property_reason_name = zend_string_init("reason", sizeof("reason") - 1, 1);
	zend_declare_typed_property(class_entry, property_reason_name, &property_reason_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING));
	zend_string_release(property_reason_name);

	zval property_flags_default_value;
	ZVAL_UNDEF(&property_flags_default_value);
	zend_string *property_flags_name = zend_string_init("flags", sizeof("flags") - 1, 1);
	zend_declare_typed_property(class_entry, property_flags_name, &property_flags_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_flags_name);

	zval property_bytesConsumed_default_value;
	ZVAL_UNDEF(&property_bytesConsumed_default_value);
	zend_string *property_bytesConsumed_name = zend_string_init("bytesConsumed", sizeof("bytesConsumed") - 1, 1);
	zend_declare_typed_property(class_entry, property_bytesConsumed_name, &property_bytesConsumed_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_bytesConsumed_name);

	return class_entry;
}

static zend_class_entry *register_class_WebSocket_Protocol(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "WebSocket", "Protocol", class_WebSocket_Protocol_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval const_OPCODE_CONTINUATION_value;
	ZVAL_LONG(&const_OPCODE_CONTINUATION_value, 0x0);
	zend_string *const_OPCODE_CONTINUATION_name = zend_string_init_interned("OPCODE_CONTINUATION", sizeof("OPCODE_CONTINUATION") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_OPCODE_CONTINUATION_name, &const_OPCODE_CONTINUATION_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_OPCODE_CONTINUATION_name);

	zval const_OPCODE_TEXT_value;
	ZVAL_LONG(&const_OPCODE_TEXT_value, 0x1);
	zend_string *const_OPCODE_TEXT_name = zend_string_init_interned("OPCODE_TEXT", sizeof("OPCODE_TEXT") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_OPCODE_TEXT_name, &const_OPCODE_TEXT_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_OPCODE_TEXT_name);

	zval const_OPCODE_BINARY_value;
	ZVAL_LONG(&const_OPCODE_BINARY_value, 0x2);
	zend_string *const_OPCODE_BINARY_name = zend_string_init_interned("OPCODE_BINARY", sizeof("OPCODE_BINARY") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_OPCODE_BINARY_name, &const_OPCODE_BINARY_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_OPCODE_BINARY_name);

	zval const_OPCODE_CLOSE_value;
	ZVAL_LONG(&const_OPCODE_CLOSE_value, 0x8);
	zend_string *const_OPCODE_CLOSE_name = zend_string_init_interned("OPCODE_CLOSE", sizeof("OPCODE_CLOSE") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_OPCODE_CLOSE_name, &const_OPCODE_CLOSE_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_OPCODE_CLOSE_name);

	zval const_OPCODE_PING_value;
	ZVAL_LONG(&const_OPCODE_PING_value, 0x9);
	zend_string *const_OPCODE_PING_name = zend_string_init_interned("OPCODE_PING", sizeof("OPCODE_PING") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_OPCODE_PING_name, &const_OPCODE_PING_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_OPCODE_PING_name);

	zval const_OPCODE_PONG_value;
	ZVAL_LONG(&const_OPCODE_PONG_value, 0xa);
	zend_string *const_OPCODE_PONG_name = zend_string_init_interned("OPCODE_PONG", sizeof("OPCODE_PONG") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_OPCODE_PONG_name, &const_OPCODE_PONG_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_OPCODE_PONG_name);

	zval const_FLAG_FIN_value;
	ZVAL_LONG(&const_FLAG_FIN_value, 1 << 0);
	zend_string *const_FLAG_FIN_name = zend_string_init_interned("FLAG_FIN", sizeof("FLAG_FIN") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_FLAG_FIN_name, &const_FLAG_FIN_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_FLAG_FIN_name);

	zval const_FLAG_COMPRESS_value;
	ZVAL_LONG(&const_FLAG_COMPRESS_value, 1 << 1);
	zend_string *const_FLAG_COMPRESS_name = zend_string_init_interned("FLAG_COMPRESS", sizeof("FLAG_COMPRESS") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_FLAG_COMPRESS_name, &const_FLAG_COMPRESS_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_FLAG_COMPRESS_name);

	zval const_FLAG_RSV1_value;
	ZVAL_LONG(&const_FLAG_RSV1_value, 1 << 2);
	zend_string *const_FLAG_RSV1_name = zend_string_init_interned("FLAG_RSV1", sizeof("FLAG_RSV1") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_FLAG_RSV1_name, &const_FLAG_RSV1_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_FLAG_RSV1_name);

	zval const_FLAG_RSV2_value;
	ZVAL_LONG(&const_FLAG_RSV2_value, 1 << 3);
	zend_string *const_FLAG_RSV2_name = zend_string_init_interned("FLAG_RSV2", sizeof("FLAG_RSV2") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_FLAG_RSV2_name, &const_FLAG_RSV2_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_FLAG_RSV2_name);

	zval const_FLAG_RSV3_value;
	ZVAL_LONG(&const_FLAG_RSV3_value, 1 << 4);
	zend_string *const_FLAG_RSV3_name = zend_string_init_interned("FLAG_RSV3", sizeof("FLAG_RSV3") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_FLAG_RSV3_name, &const_FLAG_RSV3_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_FLAG_RSV3_name);

	zval const_FLAG_MASK_value;
	ZVAL_LONG(&const_FLAG_MASK_value, 1 << 5);
	zend_string *const_FLAG_MASK_name = zend_string_init_interned("FLAG_MASK", sizeof("FLAG_MASK") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_FLAG_MASK_name, &const_FLAG_MASK_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_FLAG_MASK_name);

	zval const_CLOSE_NORMAL_value;
	ZVAL_LONG(&const_CLOSE_NORMAL_value, 1000);
	zend_string *const_CLOSE_NORMAL_name = zend_string_init_interned("CLOSE_NORMAL", sizeof("CLOSE_NORMAL") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_NORMAL_name, &const_CLOSE_NORMAL_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_NORMAL_name);

	zval const_CLOSE_GOING_AWAY_value;
	ZVAL_LONG(&const_CLOSE_GOING_AWAY_value, 1001);
	zend_string *const_CLOSE_GOING_AWAY_name = zend_string_init_interned("CLOSE_GOING_AWAY", sizeof("CLOSE_GOING_AWAY") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_GOING_AWAY_name, &const_CLOSE_GOING_AWAY_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_GOING_AWAY_name);

	zval const_CLOSE_PROTOCOL_ERROR_value;
	ZVAL_LONG(&const_CLOSE_PROTOCOL_ERROR_value, 1002);
	zend_string *const_CLOSE_PROTOCOL_ERROR_name = zend_string_init_interned("CLOSE_PROTOCOL_ERROR", sizeof("CLOSE_PROTOCOL_ERROR") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_PROTOCOL_ERROR_name, &const_CLOSE_PROTOCOL_ERROR_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_PROTOCOL_ERROR_name);

	zval const_CLOSE_UNSUPPORTED_DATA_value;
	ZVAL_LONG(&const_CLOSE_UNSUPPORTED_DATA_value, 1003);
	zend_string *const_CLOSE_UNSUPPORTED_DATA_name = zend_string_init_interned("CLOSE_UNSUPPORTED_DATA", sizeof("CLOSE_UNSUPPORTED_DATA") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_UNSUPPORTED_DATA_name, &const_CLOSE_UNSUPPORTED_DATA_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_UNSUPPORTED_DATA_name);

	zval const_CLOSE_NO_STATUS_value;
	ZVAL_LONG(&const_CLOSE_NO_STATUS_value, 1005);
	zend_string *const_CLOSE_NO_STATUS_name = zend_string_init_interned("CLOSE_NO_STATUS", sizeof("CLOSE_NO_STATUS") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_NO_STATUS_name, &const_CLOSE_NO_STATUS_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_NO_STATUS_name);

	zval const_CLOSE_ABNORMAL_value;
	ZVAL_LONG(&const_CLOSE_ABNORMAL_value, 1006);
	zend_string *const_CLOSE_ABNORMAL_name = zend_string_init_interned("CLOSE_ABNORMAL", sizeof("CLOSE_ABNORMAL") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_ABNORMAL_name, &const_CLOSE_ABNORMAL_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_ABNORMAL_name);

	zval const_CLOSE_INVALID_PAYLOAD_value;
	ZVAL_LONG(&const_CLOSE_INVALID_PAYLOAD_value, 1007);
	zend_string *const_CLOSE_INVALID_PAYLOAD_name = zend_string_init_interned("CLOSE_INVALID_PAYLOAD", sizeof("CLOSE_INVALID_PAYLOAD") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_INVALID_PAYLOAD_name, &const_CLOSE_INVALID_PAYLOAD_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_INVALID_PAYLOAD_name);

	zval const_CLOSE_POLICY_VIOLATION_value;
	ZVAL_LONG(&const_CLOSE_POLICY_VIOLATION_value, 1008);
	zend_string *const_CLOSE_POLICY_VIOLATION_name = zend_string_init_interned("CLOSE_POLICY_VIOLATION", sizeof("CLOSE_POLICY_VIOLATION") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_POLICY_VIOLATION_name, &const_CLOSE_POLICY_VIOLATION_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_POLICY_VIOLATION_name);

	zval const_CLOSE_MESSAGE_TOO_BIG_value;
	ZVAL_LONG(&const_CLOSE_MESSAGE_TOO_BIG_value, 1009);
	zend_string *const_CLOSE_MESSAGE_TOO_BIG_name = zend_string_init_interned("CLOSE_MESSAGE_TOO_BIG", sizeof("CLOSE_MESSAGE_TOO_BIG") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_MESSAGE_TOO_BIG_name, &const_CLOSE_MESSAGE_TOO_BIG_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_MESSAGE_TOO_BIG_name);

	zval const_CLOSE_EXTENSION_MISSING_value;
	ZVAL_LONG(&const_CLOSE_EXTENSION_MISSING_value, 1010);
	zend_string *const_CLOSE_EXTENSION_MISSING_name = zend_string_init_interned("CLOSE_EXTENSION_MISSING", sizeof("CLOSE_EXTENSION_MISSING") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_EXTENSION_MISSING_name, &const_CLOSE_EXTENSION_MISSING_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_EXTENSION_MISSING_name);

	zval const_CLOSE_SERVER_ERROR_value;
	ZVAL_LONG(&const_CLOSE_SERVER_ERROR_value, 1011);
	zend_string *const_CLOSE_SERVER_ERROR_name = zend_string_init_interned("CLOSE_SERVER_ERROR", sizeof("CLOSE_SERVER_ERROR") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_SERVER_ERROR_name, &const_CLOSE_SERVER_ERROR_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_SERVER_ERROR_name);

	zval const_CLOSE_TLS_value;
	ZVAL_LONG(&const_CLOSE_TLS_value, 1015);
	zend_string *const_CLOSE_TLS_name = zend_string_init_interned("CLOSE_TLS", sizeof("CLOSE_TLS") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_CLOSE_TLS_name, &const_CLOSE_TLS_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_CLOSE_TLS_name);

	return class_entry;
}
