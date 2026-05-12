#include "php_websocket.h"
#include "php_websocket_compat.h"
#include "websocket_arginfo.h"
#include "ext/standard/base64.h"
#include "ext/standard/sha1.h"
#include "Zend/zend_enum.h"

#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static uint8_t websocket_message_type_opcode(zval *type)
{
	zval *case_name_zv;
	zend_string *case_name;

	case_name_zv = zend_enum_fetch_case_name(Z_OBJ_P(type));
	case_name = Z_STR_P(case_name_zv);

	if (zend_string_equals_literal(case_name, "Text")) {
		return 0x1;
	}
	if (zend_string_equals_literal(case_name, "Binary")) {
		return 0x2;
	}
	if (zend_string_equals_literal(case_name, "Close")) {
		return 0x8;
	}
	if (zend_string_equals_literal(case_name, "Ping")) {
		return 0x9;
	}
	if (zend_string_equals_literal(case_name, "Pong")) {
		return 0xA;
	}

	return 0x1;
}

static zend_object *websocket_message_type_from_opcode(uint8_t opcode)
{
	const char *name;

	switch (opcode) {
		case 0x1:
			name = "Text";
			break;
		case 0x2:
			name = "Binary";
			break;
		case 0x8:
			name = "Close";
			break;
		case 0x9:
			name = "Ping";
			break;
		case 0xA:
			name = "Pong";
			break;
		default:
			return NULL;
	}

	return zend_enum_get_case_cstr(websocket_message_type_ce, name);
}

static zend_string *websocket_encode_frame(zend_string *payload, uint8_t opcode, bool masked)
{
	size_t payload_len = ZSTR_LEN(payload);
	size_t header_len = 2;
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

	out[pos++] = 0x80 | (opcode & 0x0f);

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

		for (i = 0; i < payload_len; i++) {
			out[pos + i] = ((unsigned char *) ZSTR_VAL(payload))[i] ^ mask[i % 4];
		}
	} else {
		memcpy(out + pos, ZSTR_VAL(payload), payload_len);
	}

	ZSTR_VAL(frame)[ZSTR_LEN(frame)] = '\0';
	return frame;
}

PHP_METHOD(WebSocket_Frame, __construct)
{
	zval *type;
	zend_string *payload;
	bool final = true;
	zend_long bytes_consumed = 0;

	ZEND_PARSE_PARAMETERS_START(2, 4)
		Z_PARAM_OBJECT_OF_CLASS(type, websocket_message_type_ce)
		Z_PARAM_STR(payload)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(final)
		Z_PARAM_LONG(bytes_consumed)
	ZEND_PARSE_PARAMETERS_END();

	zend_update_property(websocket_frame_ce, Z_OBJ_P(ZEND_THIS), "type", sizeof("type") - 1, type);
	zend_update_property_str(websocket_frame_ce, Z_OBJ_P(ZEND_THIS), "payload", sizeof("payload") - 1, payload);
	zend_update_property_bool(websocket_frame_ce, Z_OBJ_P(ZEND_THIS), "final", sizeof("final") - 1, final);
	zend_update_property_long(websocket_frame_ce, Z_OBJ_P(ZEND_THIS), "bytesConsumed", sizeof("bytesConsumed") - 1, bytes_consumed);
}

PHP_METHOD(WebSocket_Protocol, acceptKey)
{
	zend_string *key;
	PHP_SHA1_CTX ctx;
	unsigned char digest[20];
	zend_string *accept;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(key)
	ZEND_PARSE_PARAMETERS_END();

	PHP_SHA1Init(&ctx);
	PHP_SHA1Update(&ctx, (unsigned char *) ZSTR_VAL(key), ZSTR_LEN(key));
	PHP_SHA1Update(&ctx, (unsigned char *) WEBSOCKET_GUID, sizeof(WEBSOCKET_GUID) - 1);
	PHP_SHA1Final(digest, &ctx);

	accept = php_base64_encode(digest, sizeof(digest));
	RETURN_STR(accept);
}

PHP_METHOD(WebSocket_Protocol, encode)
{
	zend_string *payload;
	zval *type = NULL;
	bool masked = false;
	uint8_t opcode = 0x1;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_STR(payload)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJECT_OF_CLASS(type, websocket_message_type_ce)
		Z_PARAM_BOOL(masked)
	ZEND_PARSE_PARAMETERS_END();

	if (type) {
		opcode = websocket_message_type_opcode(type);
	}

	RETURN_STR(websocket_encode_frame(payload, opcode, masked));
}

PHP_METHOD(WebSocket_Protocol, decode)
{
	zend_string *buffer;
	const unsigned char *in;
	size_t len;
	size_t pos = 2;
	uint8_t b0, b1, opcode;
	bool final;
	bool masked;
	uint64_t payload_len;
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

	if (payload_len > SIZE_MAX || len < pos + (size_t) payload_len) {
		RETURN_NULL();
	}

	type_case = websocket_message_type_from_opcode(opcode);
	if (!type_case) {
		zend_throw_error(NULL, "Unsupported WebSocket opcode %u", opcode);
		RETURN_THROWS();
	}

	payload = zend_string_alloc((size_t) payload_len, 0);
	if (masked) {
		for (i = 0; i < (size_t) payload_len; i++) {
			ZSTR_VAL(payload)[i] = in[pos + i] ^ mask[i % 4];
		}
	} else {
		memcpy(ZSTR_VAL(payload), in + pos, (size_t) payload_len);
	}
	ZSTR_VAL(payload)[payload_len] = '\0';

	object_init_ex(return_value, websocket_frame_ce);

	ZVAL_OBJ(&type_zv, type_case);
	zend_update_property(websocket_frame_ce, Z_OBJ_P(return_value), "type", sizeof("type") - 1, &type_zv);
	zend_update_property_str(websocket_frame_ce, Z_OBJ_P(return_value), "payload", sizeof("payload") - 1, payload);
	zend_update_property_bool(websocket_frame_ce, Z_OBJ_P(return_value), "final", sizeof("final") - 1, final);
	zend_update_property_long(websocket_frame_ce, Z_OBJ_P(return_value), "bytesConsumed", sizeof("bytesConsumed") - 1, (zend_long) (pos + (size_t) payload_len));

	zend_string_release(payload);
}

void websocket_register_protocol_classes(void)
{
	websocket_frame_ce = register_class_WebSocket_Frame();
	websocket_protocol_ce = register_class_WebSocket_Protocol();
}
