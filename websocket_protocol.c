/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

#include "php_websocket.h"
#include "php_websocket_compat.h"
#include "websocket_arginfo.h"
#include "ext/standard/base64.h"
#include "ext/standard/sha1.h"
#include "Zend/zend_enum.h"

#include <string.h>

#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static uint32_t websocket_frame_prop_type_num;
static uint32_t websocket_frame_prop_opcode_num;
static uint32_t websocket_frame_prop_flags_num;
static uint32_t websocket_frame_prop_payload_num;
static uint32_t websocket_frame_prop_final_num;
static uint32_t websocket_frame_prop_bytes_consumed_num;
static uint32_t websocket_close_frame_prop_code_num;
static uint32_t websocket_close_frame_prop_reason_num;
static uint32_t websocket_close_frame_prop_flags_num;
static uint32_t websocket_close_frame_prop_bytes_consumed_num;

zend_string *websocket_protocol_accept_key(zend_string *key)
{
	PHP_SHA1_CTX ctx;
	unsigned char digest[20];

	PHP_SHA1Init(&ctx);
	PHP_SHA1Update(&ctx, (unsigned char *) ZSTR_VAL(key), ZSTR_LEN(key));
	PHP_SHA1Update(&ctx, (unsigned char *) WEBSOCKET_GUID, strlen(WEBSOCKET_GUID));
	PHP_SHA1Final(digest, &ctx);

	return php_base64_encode(digest, sizeof(digest));
}

static uint32_t websocket_property_num(zend_class_entry *ce, const char *name, size_t name_len)
{
	zend_string *property_name = zend_string_init(name, name_len, 0);
	zend_property_info *property_info = zend_hash_find_ptr(&ce->properties_info, property_name);

	zend_string_release(property_name);
	ZEND_ASSERT(property_info != NULL);

	return OBJ_PROP_TO_NUM(property_info->offset);
}

bool websocket_protocol_opcode_is_valid(zend_long opcode)
{
	switch (opcode) {
		case WEBSOCKET_OPCODE_CONTINUATION:
		case WEBSOCKET_OPCODE_TEXT:
		case WEBSOCKET_OPCODE_BINARY:
		case WEBSOCKET_OPCODE_CLOSE:
		case WEBSOCKET_OPCODE_PING:
		case WEBSOCKET_OPCODE_PONG:
			return true;
		default:
			return false;
	}
}

bool websocket_protocol_opcode_is_control(zend_long opcode)
{
	return opcode >= 0x8 && opcode <= 0xf;
}

uint8_t websocket_protocol_message_type_opcode(zval *type)
{
	zval *case_name_zv;
	zend_string *case_name;

	case_name_zv = zend_enum_fetch_case_name(Z_OBJ_P(type));
	case_name = Z_STR_P(case_name_zv);

	if (zend_string_equals_literal(case_name, "Continuation")) {
		return WEBSOCKET_OPCODE_CONTINUATION;
	}
	if (zend_string_equals_literal(case_name, "Text")) {
		return WEBSOCKET_OPCODE_TEXT;
	}
	if (zend_string_equals_literal(case_name, "Binary")) {
		return WEBSOCKET_OPCODE_BINARY;
	}
	if (zend_string_equals_literal(case_name, "Close")) {
		return WEBSOCKET_OPCODE_CLOSE;
	}
	if (zend_string_equals_literal(case_name, "Ping")) {
		return WEBSOCKET_OPCODE_PING;
	}
	if (zend_string_equals_literal(case_name, "Pong")) {
		return WEBSOCKET_OPCODE_PONG;
	}

	return WEBSOCKET_OPCODE_TEXT;
}

zend_object *websocket_protocol_message_type_from_opcode(uint8_t opcode)
{
	const char *name;

	switch (opcode) {
		case WEBSOCKET_OPCODE_CONTINUATION:
			name = "Continuation";
			break;
		case WEBSOCKET_OPCODE_TEXT:
			name = "Text";
			break;
		case WEBSOCKET_OPCODE_BINARY:
			name = "Binary";
			break;
		case WEBSOCKET_OPCODE_CLOSE:
			name = "Close";
			break;
		case WEBSOCKET_OPCODE_PING:
			name = "Ping";
			break;
		case WEBSOCKET_OPCODE_PONG:
			name = "Pong";
			break;
		default:
			return NULL;
	}

	return zend_enum_get_case_cstr(websocket_message_type_ce, name);
}

static zend_always_inline void websocket_mask_payload_copy(unsigned char *dst, const unsigned char *src, size_t len, const uint8_t mask[4])
{
	size_t i = 0;
	uint32_t mask32;
	uint64_t mask64;

	memcpy(&mask32, mask, sizeof(mask32));
	mask64 = ((uint64_t) mask32 << 32) | mask32;

	while (i + sizeof(uint64_t) <= len) {
		uint64_t chunk;

		memcpy(&chunk, src + i, sizeof(chunk));
		chunk ^= mask64;
		memcpy(dst + i, &chunk, sizeof(chunk));
		i += sizeof(chunk);
	}

	if (i + sizeof(uint32_t) <= len) {
		uint32_t chunk;

		memcpy(&chunk, src + i, sizeof(chunk));
		chunk ^= mask32;
		memcpy(dst + i, &chunk, sizeof(chunk));
		i += sizeof(chunk);
	}

	for (; i < len; i++) {
		dst[i] = src[i] ^ mask[i & 3];
	}
}

zend_string *websocket_protocol_pack_payload(zend_string *payload, uint8_t opcode, uint8_t flags)
{
	size_t payload_len = ZSTR_LEN(payload);
	size_t header_len = 2;
	bool masked = (flags & WEBSOCKET_FLAG_MASK) != 0;
	size_t mask_len = masked ? 4 : 0;
	size_t pos = 0;
	zend_string *frame;
	unsigned char *out;
	uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
	size_t i;

	if (payload_len >= 126 && payload_len <= 0xffff) {
		header_len += 2;
	} else if (payload_len > 0xffff) {
		header_len += 8;
	}

	frame = zend_string_alloc(header_len + mask_len + payload_len, 0);
	out = (unsigned char *) ZSTR_VAL(frame);

	out[pos] = opcode & 0x0f;
	if (flags & WEBSOCKET_FLAG_FIN) {
		out[pos] |= 0x80;
	}
	if (flags & (WEBSOCKET_FLAG_COMPRESS | WEBSOCKET_FLAG_RSV1)) {
		out[pos] |= 0x40;
	}
	if (flags & WEBSOCKET_FLAG_RSV2) {
		out[pos] |= 0x20;
	}
	if (flags & WEBSOCKET_FLAG_RSV3) {
		out[pos] |= 0x10;
	}
	pos++;

	if (payload_len < 126) {
		out[pos++] = (masked ? 0x80 : 0) | (uint8_t) payload_len;
	} else if (payload_len <= 0xffff) {
		out[pos++] = (masked ? 0x80 : 0) | 126;
		out[pos++] = (payload_len >> 8) & 0xff;
		out[pos++] = payload_len & 0xff;
	} else {
		out[pos++] = (masked ? 0x80 : 0) | 127;
		for (i = 0; i < 8; i++) {
			out[pos++] = (payload_len >> (56 - (i * 8))) & 0xff;
		}
	}

	if (masked) {
		memcpy(out + pos, mask, sizeof(mask));
		pos += sizeof(mask);

		websocket_mask_payload_copy(out + pos, (const unsigned char *) ZSTR_VAL(payload), payload_len, mask);
	} else {
		memcpy(out + pos, ZSTR_VAL(payload), payload_len);
	}

	ZSTR_VAL(frame)[ZSTR_LEN(frame)] = '\0';
	return frame;
}

static void websocket_frame_update_properties(zval *object, zval *type, zend_string *payload, bool final, zend_long bytes_consumed, zend_long opcode, zend_long flags)
{
	zend_update_property(websocket_frame_ce, Z_OBJ_P(object), "type", strlen("type"), type);
	zend_update_property_long(websocket_frame_ce, Z_OBJ_P(object), "opcode", strlen("opcode"), opcode);
	zend_update_property_long(websocket_frame_ce, Z_OBJ_P(object), "flags", strlen("flags"), flags);
	zend_update_property_str(websocket_frame_ce, Z_OBJ_P(object), "payload", strlen("payload"), payload);
	zend_update_property_bool(websocket_frame_ce, Z_OBJ_P(object), "final", strlen("final"), final);
	zend_update_property_long(websocket_frame_ce, Z_OBJ_P(object), "bytesConsumed", strlen("bytesConsumed"), bytes_consumed);
}

static zend_always_inline void websocket_frame_init_properties(zval *object, zval *type, zend_string *payload, bool final, zend_long bytes_consumed, zend_long opcode, zend_long flags)
{
	zend_object *frame = Z_OBJ_P(object);

	zval *property = OBJ_PROP_NUM(frame, websocket_frame_prop_type_num);
	ZVAL_COPY(property, type);

	property = OBJ_PROP_NUM(frame, websocket_frame_prop_opcode_num);
	ZVAL_LONG(property, opcode);

	property = OBJ_PROP_NUM(frame, websocket_frame_prop_flags_num);
	ZVAL_LONG(property, flags);

	property = OBJ_PROP_NUM(frame, websocket_frame_prop_payload_num);
	ZVAL_STR_COPY(property, payload);

	property = OBJ_PROP_NUM(frame, websocket_frame_prop_final_num);
	ZVAL_BOOL(property, final);

	property = OBJ_PROP_NUM(frame, websocket_frame_prop_bytes_consumed_num);
	ZVAL_LONG(property, bytes_consumed);
}

static void websocket_close_frame_update_properties(zval *object, zend_long code, zend_string *reason, zend_long flags, zend_long bytes_consumed)
{
	zend_update_property_long(websocket_close_frame_ce, Z_OBJ_P(object), "code", strlen("code"), code);
	zend_update_property_str(websocket_close_frame_ce, Z_OBJ_P(object), "reason", strlen("reason"), reason);
	zend_update_property_long(websocket_close_frame_ce, Z_OBJ_P(object), "flags", strlen("flags"), flags);
	zend_update_property_long(websocket_close_frame_ce, Z_OBJ_P(object), "bytesConsumed", strlen("bytesConsumed"), bytes_consumed);
}

static zend_always_inline void websocket_close_frame_init_properties(zval *object, zend_long code, zend_string *reason, zend_long flags, zend_long bytes_consumed)
{
	zend_object *frame = Z_OBJ_P(object);

	zval *property = OBJ_PROP_NUM(frame, websocket_close_frame_prop_code_num);
	ZVAL_LONG(property, code);

	property = OBJ_PROP_NUM(frame, websocket_close_frame_prop_reason_num);
	ZVAL_STR_COPY(property, reason);

	property = OBJ_PROP_NUM(frame, websocket_close_frame_prop_flags_num);
	ZVAL_LONG(property, flags);

	property = OBJ_PROP_NUM(frame, websocket_close_frame_prop_bytes_consumed_num);
	ZVAL_LONG(property, bytes_consumed);
}

zend_string *websocket_protocol_close_payload(zend_long code, zend_string *reason)
{
	zend_string *payload;

	if (ZSTR_LEN(reason) > WEBSOCKET_CLOSE_REASON_MAX_LEN) {
		zend_argument_value_error(2, "must be at most %d bytes", WEBSOCKET_CLOSE_REASON_MAX_LEN);
		return NULL;
	}

	payload = zend_string_alloc(2 + ZSTR_LEN(reason), 0);
	ZSTR_VAL(payload)[0] = (char) ((code >> 8) & 0xff);
	ZSTR_VAL(payload)[1] = (char) (code & 0xff);
	if (ZSTR_LEN(reason) > 0) {
		memcpy(ZSTR_VAL(payload) + 2, ZSTR_VAL(reason), ZSTR_LEN(reason));
	}
	ZSTR_VAL(payload)[ZSTR_LEN(payload)] = '\0';

	return payload;
}

PHP_METHOD(WebSocket_Frame, __construct)
{
	zval *type;
	zend_string *payload;
	bool final = true;
	zend_long opcode;
	zend_long flags;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_OBJECT_OF_CLASS(type, websocket_message_type_ce)
		Z_PARAM_STR(payload)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(final)
	ZEND_PARSE_PARAMETERS_END();

	opcode = websocket_protocol_message_type_opcode(type);
	flags = final ? WEBSOCKET_FLAG_FIN : 0;

	websocket_frame_update_properties(ZEND_THIS, type, payload, final, 0, opcode, flags);
}

PHP_METHOD(WebSocket_CloseFrame, __construct)
{
	zend_long code = WEBSOCKET_CLOSE_NORMAL;
	zend_string *reason = NULL;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(code)
		Z_PARAM_STR(reason)
	ZEND_PARSE_PARAMETERS_END();

	if (!reason) {
		reason = ZSTR_EMPTY_ALLOC();
	}
	if (ZSTR_LEN(reason) > WEBSOCKET_CLOSE_REASON_MAX_LEN) {
		zend_argument_value_error(2, "must be at most %d bytes", WEBSOCKET_CLOSE_REASON_MAX_LEN);
		RETURN_THROWS();
	}

	websocket_close_frame_update_properties(ZEND_THIS, code, reason, WEBSOCKET_FLAG_FIN, 0);
}

PHP_METHOD(WebSocket_Protocol, acceptKey)
{
	zend_string *key;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(key)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_STR(websocket_protocol_accept_key(key));
}

PHP_METHOD(WebSocket_Protocol, encode)
{
	zend_string *payload;
	zval *type = NULL;
	bool masked = false;
	uint8_t opcode = WEBSOCKET_OPCODE_TEXT;
	uint8_t flags = WEBSOCKET_FLAG_FIN;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_STR(payload)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJECT_OF_CLASS(type, websocket_message_type_ce)
		Z_PARAM_BOOL(masked)
	ZEND_PARSE_PARAMETERS_END();

	if (type) {
		opcode = websocket_protocol_message_type_opcode(type);
	}
	if (masked) {
		flags |= WEBSOCKET_FLAG_MASK;
	}

	RETURN_STR(websocket_protocol_pack_payload(payload, opcode, flags));
}

PHP_METHOD(WebSocket_Protocol, pack)
{
	zval *data;
	zend_long opcode = WEBSOCKET_OPCODE_TEXT;
	zend_long flags = WEBSOCKET_FLAG_FIN;
	zend_string *payload = NULL;
	zend_string *tmp_payload = NULL;
	zend_string *reason = NULL;
	zval rv;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_ZVAL(data)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(opcode)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	if (Z_TYPE_P(data) == IS_STRING) {
		payload = Z_STR_P(data);
	} else if (Z_TYPE_P(data) == IS_OBJECT && instanceof_function(Z_OBJCE_P(data), websocket_frame_ce)) {
		zval *prop;

		prop = zend_read_property(websocket_frame_ce, Z_OBJ_P(data), "payload", strlen("payload"), 0, &rv);
		payload = zval_get_string(prop);
		tmp_payload = payload;

		prop = zend_read_property(websocket_frame_ce, Z_OBJ_P(data), "opcode", strlen("opcode"), 0, &rv);
		opcode = zval_get_long(prop);

		prop = zend_read_property(websocket_frame_ce, Z_OBJ_P(data), "flags", strlen("flags"), 0, &rv);
		flags = zval_get_long(prop);
	} else if (Z_TYPE_P(data) == IS_OBJECT && instanceof_function(Z_OBJCE_P(data), websocket_close_frame_ce)) {
		zval *prop;
		zend_long code;

		prop = zend_read_property(websocket_close_frame_ce, Z_OBJ_P(data), "code", strlen("code"), 0, &rv);
		code = zval_get_long(prop);

		prop = zend_read_property(websocket_close_frame_ce, Z_OBJ_P(data), "reason", strlen("reason"), 0, &rv);
		reason = zval_get_string(prop);
		payload = websocket_protocol_close_payload(code, reason);
		zend_string_release(reason);
		if (!payload) {
			RETURN_THROWS();
		}
		tmp_payload = payload;

		prop = zend_read_property(websocket_close_frame_ce, Z_OBJ_P(data), "flags", strlen("flags"), 0, &rv);
		flags = zval_get_long(prop);
		opcode = WEBSOCKET_OPCODE_CLOSE;
	} else {
		zend_argument_type_error(1, "must be of type string|WebSocket\\Frame|WebSocket\\CloseFrame, %s given", websocket_zval_value_name(data));
		RETURN_THROWS();
	}

	if (!websocket_protocol_opcode_is_valid(opcode)) {
		zend_argument_value_error(2, "must be a valid WebSocket opcode");
		if (tmp_payload) {
			zend_string_release(tmp_payload);
		}
		RETURN_THROWS();
	}

	if (websocket_protocol_opcode_is_control(opcode) && !(flags & WEBSOCKET_FLAG_FIN)) {
		zend_argument_value_error(3, "must include FIN for control frames");
		if (tmp_payload) {
			zend_string_release(tmp_payload);
		}
		RETURN_THROWS();
	}

	if (websocket_protocol_opcode_is_control(opcode) && ZSTR_LEN(payload) > 125) {
		zend_argument_value_error(1, "control frame payload must be at most 125 bytes");
		if (tmp_payload) {
			zend_string_release(tmp_payload);
		}
		RETURN_THROWS();
	}

	RETVAL_STR(websocket_protocol_pack_payload(payload, (uint8_t) opcode, (uint8_t) (flags & WEBSOCKET_FLAGS_ALL)));

	if (tmp_payload) {
		zend_string_release(tmp_payload);
	}
}

static void websocket_protocol_unpack(INTERNAL_FUNCTION_PARAMETERS)
{
	zend_string *buffer;
	const unsigned char *in;
	size_t len;
	size_t pos = 2;
	uint8_t b0, b1, opcode;
	bool final;
	bool masked;
	uint64_t payload_len;
	uint8_t flags = 0;
	uint8_t mask[4];
	zend_string *payload;
	size_t i;
	zend_object *type_case;
	zval type_zv;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(buffer)
	ZEND_PARSE_PARAMETERS_END();

	in = (const unsigned char *) ZSTR_VAL(buffer);
	len = ZSTR_LEN(buffer);

	if (len < 2) {
		RETURN_NULL();
	}

	b0 = in[0];
	b1 = in[1];
	final = (b0 & 0x80) != 0;
	opcode = b0 & 0x0f;
	masked = (b1 & 0x80) != 0;
	payload_len = b1 & 0x7f;

	if (final) {
		flags |= WEBSOCKET_FLAG_FIN;
	}
	if (b0 & 0x40) {
		flags |= WEBSOCKET_FLAG_RSV1;
	}
	if (b0 & 0x20) {
		flags |= WEBSOCKET_FLAG_RSV2;
	}
	if (b0 & 0x10) {
		flags |= WEBSOCKET_FLAG_RSV3;
	}
	if (masked) {
		flags |= WEBSOCKET_FLAG_MASK;
	}

	if (payload_len == 126) {
		if (len < 4) {
			RETURN_NULL();
		}
		payload_len = ((uint64_t) in[2] << 8) | in[3];
		pos = 4;
	} else if (payload_len == 127) {
		if (len < 10) {
			RETURN_NULL();
		}
		payload_len = 0;
		for (i = 0; i < 8; i++) {
			payload_len = (payload_len << 8) | in[2 + i];
		}
		pos = 10;
	}

	if (masked) {
		if (len < pos + 4) {
			RETURN_NULL();
		}
		memcpy(mask, in + pos, sizeof(mask));
		pos += 4;
	}

	if (payload_len > SIZE_MAX || payload_len > SIZE_MAX - pos || len < pos + (size_t) payload_len) {
		RETURN_NULL();
	}

	if (!websocket_protocol_opcode_is_valid(opcode)) {
		zend_throw_error(NULL, "Unsupported WebSocket opcode %u", opcode);
		RETURN_THROWS();
	}

	if (websocket_protocol_opcode_is_control(opcode) && (!final || payload_len > 125)) {
		zend_throw_error(NULL, "Invalid WebSocket control frame");
		RETURN_THROWS();
	}

	if (opcode == WEBSOCKET_OPCODE_CLOSE && payload_len == 1) {
		zend_throw_error(NULL, "Invalid WebSocket close frame");
		RETURN_THROWS();
	}

	payload = zend_string_alloc((size_t) payload_len, 0);
	if (masked) {
		websocket_mask_payload_copy((unsigned char *) ZSTR_VAL(payload), in + pos, (size_t) payload_len, mask);
	} else {
		memcpy(ZSTR_VAL(payload), in + pos, (size_t) payload_len);
	}
	ZSTR_VAL(payload)[payload_len] = '\0';

	if (opcode == WEBSOCKET_OPCODE_CLOSE) {
		zend_long code = WEBSOCKET_CLOSE_NORMAL;
		zend_string *reason;
		unsigned char *close_payload;

		if (payload_len >= 2) {
			close_payload = (unsigned char *) ZSTR_VAL(payload);
			code = ((zend_long) close_payload[0] << 8) | close_payload[1];
			reason = zend_string_init(ZSTR_VAL(payload) + 2, (size_t) payload_len - 2, 0);
		} else {
			reason = ZSTR_EMPTY_ALLOC();
		}

		object_init_ex(return_value, websocket_close_frame_ce);
		websocket_close_frame_init_properties(return_value, code, reason, flags, (zend_long) (pos + (size_t) payload_len));
		zend_string_release(reason);
		zend_string_release(payload);
		return;
	}

	type_case = websocket_protocol_message_type_from_opcode(opcode);
	if (!type_case) {
		zend_string_release(payload);
		zend_throw_error(NULL, "Unsupported WebSocket opcode %u", opcode);
		RETURN_THROWS();
	}

	object_init_ex(return_value, websocket_frame_ce);

	ZVAL_OBJ(&type_zv, type_case);
	websocket_frame_init_properties(return_value, &type_zv, payload, final, (zend_long) (pos + (size_t) payload_len), opcode, flags);

	zend_string_release(payload);
}

PHP_METHOD(WebSocket_Protocol, decode)
{
	websocket_protocol_unpack(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

PHP_METHOD(WebSocket_Protocol, unpack)
{
	websocket_protocol_unpack(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

void websocket_register_protocol_classes(void)
{
	websocket_frame_ce = register_class_WebSocket_Frame();
	websocket_close_frame_ce = register_class_WebSocket_CloseFrame();
	websocket_protocol_ce = register_class_WebSocket_Protocol();

	websocket_frame_prop_type_num = websocket_property_num(websocket_frame_ce, "type", strlen("type"));
	websocket_frame_prop_opcode_num = websocket_property_num(websocket_frame_ce, "opcode", strlen("opcode"));
	websocket_frame_prop_flags_num = websocket_property_num(websocket_frame_ce, "flags", strlen("flags"));
	websocket_frame_prop_payload_num = websocket_property_num(websocket_frame_ce, "payload", strlen("payload"));
	websocket_frame_prop_final_num = websocket_property_num(websocket_frame_ce, "final", strlen("final"));
	websocket_frame_prop_bytes_consumed_num = websocket_property_num(websocket_frame_ce, "bytesConsumed", strlen("bytesConsumed"));
	websocket_close_frame_prop_code_num = websocket_property_num(websocket_close_frame_ce, "code", strlen("code"));
	websocket_close_frame_prop_reason_num = websocket_property_num(websocket_close_frame_ce, "reason", strlen("reason"));
	websocket_close_frame_prop_flags_num = websocket_property_num(websocket_close_frame_ce, "flags", strlen("flags"));
	websocket_close_frame_prop_bytes_consumed_num = websocket_property_num(websocket_close_frame_ce, "bytesConsumed", strlen("bytesConsumed"));
}
