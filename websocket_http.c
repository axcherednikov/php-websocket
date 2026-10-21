/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_websocket.h"
#include "ext/standard/base64.h"

#include <string.h>
#include <strings.h>

static const char *websocket_http_find_header_end(const char *buffer, const size_t len)
{
	size_t i;

	if (len < 4) {
		return NULL;
	}

	for (i = 0; i + 3 < len; i++) {
		if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
			return buffer + i;
		}
	}

	return NULL;
}

static void websocket_http_trim(const char **value, size_t *len)
{
	while (*len > 0 && ((*value)[0] == ' ' || (*value)[0] == '\t')) {
		(*value)++;
		(*len)--;
	}

	while (*len > 0 && ((*value)[*len - 1] == ' ' || (*value)[*len - 1] == '\t')) {
		(*len)--;
	}
}

static bool websocket_http_equals_ci(const char *value, const size_t value_len, const char *expected, const size_t expected_len)
{
	return value_len == expected_len && strncasecmp(value, expected, expected_len) == 0;
}

static bool websocket_http_header_contains_token(const char *value, const size_t value_len, const char *token, const size_t token_len)
{
	const char *part = value;
	size_t offset = 0;

	while (offset <= value_len) {
		const char *token_start = part;
		size_t token_part_len;
		const char *comma = memchr(part, ',', value_len - offset);

		if (comma) {
			token_part_len = (size_t) (comma - part);
		} else {
			token_part_len = value_len - offset;
		}

		websocket_http_trim(&token_start, &token_part_len);
		if (websocket_http_equals_ci(token_start, token_part_len, token, token_len)) {
			return true;
		}

		if (!comma) {
			break;
		}

		offset += token_part_len + 1;
		part = comma + 1;
		offset = (size_t) (part - value);
	}

	return false;
}

bool websocket_http_validate_subprotocol_token(const char *value, const size_t value_len)
{
	size_t i;

	if (value_len == 0) {
		return false;
	}

	for (i = 0; i < value_len; i++) {
		const unsigned char ch = (unsigned char) value[i];

		if (ch <= 32 || ch >= 127) {
			return false;
		}

		switch (ch) {
			case '(':
			case ')':
			case '<':
			case '>':
			case '@':
			case ',':
			case ';':
			case ':':
			case '\\':
			case '"':
			case '/':
			case '[':
			case ']':
			case '?':
			case '=':
			case '{':
			case '}':
				return false;
			default:
				break;
		}
	}

	return true;
}

static bool websocket_http_select_subprotocol(const char *value, const size_t value_len, HashTable *supported_subprotocols, zend_string **selected_subprotocol)
{
	const char *part = value;
	size_t offset = 0;

	while (offset <= value_len) {
		const char *token_start = part;
		size_t token_len;
		const char *comma = memchr(part, ',', value_len - offset);

		if (comma) {
			token_len = (size_t) (comma - part);
		} else {
			token_len = value_len - offset;
		}

		websocket_http_trim(&token_start, &token_len);
		if (!websocket_http_validate_subprotocol_token(token_start, token_len)) {
			return false;
		}

		if (!*selected_subprotocol && supported_subprotocols && zend_hash_num_elements(supported_subprotocols) > 0) {
			zend_string *offered = zend_string_init(token_start, token_len, false);

			if (zend_hash_exists(supported_subprotocols, offered)) {
				*selected_subprotocol = zend_string_copy(offered);
			}

			zend_string_release(offered);
		}

		if (!comma) {
			break;
		}

		part = comma + 1;
		offset = (size_t) (part - value);
	}

	return true;
}

static bool websocket_http_parse_request_line(const char *line, const size_t line_len, websocket_http_upgrade *upgrade)
{
	const char *method_end = memchr(line, ' ', line_len);
	const char *target_start;
	const char *target_end;
	const char *version_start;
	size_t method_len;
	size_t target_len;
	size_t version_len;

	if (!method_end) {
		return false;
	}

	method_len = (size_t) (method_end - line);
	if (!websocket_http_equals_ci(line, method_len, "GET", strlen("GET"))) {
		return false;
	}

	target_start = method_end + 1;
	target_end = memchr(target_start, ' ', line_len - method_len - 1);
	if (!target_end) {
		return false;
	}

	target_len = (size_t) (target_end - target_start);
	if (target_len == 0) {
		return false;
	}

	version_start = target_end + 1;
	version_len = (size_t) ((line + line_len) - version_start);
	if (!websocket_http_equals_ci(version_start, version_len, "HTTP/1.1", strlen("HTTP/1.1"))) {
		return false;
	}

	upgrade->method = line;
	upgrade->method_len = method_len;
	upgrade->target = target_start;
	upgrade->target_len = target_len;

	return true;
}

static bool websocket_http_validate_key(const char *value, const size_t value_len, zend_string **accept_key)
{
	zend_string *decoded;
	zend_string *key;

	decoded = php_base64_decode_ex((const unsigned char *) value, value_len, true);
	if (!decoded) {
		return false;
	}

	if (ZSTR_LEN(decoded) != 16) {
		zend_string_release(decoded);
		return false;
	}

	zend_string_release(decoded);

	key = zend_string_init(value, value_len, false);
	*accept_key = websocket_protocol_accept_key(key);
	zend_string_release(key);

	return true;
}

void websocket_http_upgrade_init(websocket_http_upgrade *upgrade)
{
	memset(upgrade, 0, sizeof(*upgrade));
}

void websocket_http_upgrade_destroy(websocket_http_upgrade *upgrade)
{
	if (upgrade->accept_key) {
		zend_string_release(upgrade->accept_key);
	}
	if (upgrade->selected_subprotocol) {
		zend_string_release(upgrade->selected_subprotocol);
	}
	if (upgrade->headers) {
		efree(upgrade->headers);
	}
	websocket_http_upgrade_init(upgrade);
}

static void websocket_http_append_header(websocket_http_upgrade *upgrade, const char *name, const size_t name_len, const char *value, const size_t value_len)
{
	if (upgrade->header_count == upgrade->header_capacity) {
		upgrade->header_capacity = upgrade->header_capacity ? upgrade->header_capacity * 2 : 8;
		upgrade->headers = erealloc(upgrade->headers, upgrade->header_capacity * sizeof(*upgrade->headers));
	}

	upgrade->headers[upgrade->header_count].name = name;
	upgrade->headers[upgrade->header_count].name_len = name_len;
	upgrade->headers[upgrade->header_count].value = value;
	upgrade->headers[upgrade->header_count].value_len = value_len;
	upgrade->header_count++;
}

static zend_string *websocket_http_lower_header_name(const char *name, const size_t name_len)
{
	zend_string *header = zend_string_init(name, name_len, false);
	zend_string *lower = zend_string_tolower(header);
	zend_string_release(header);
	return lower;
}

void websocket_http_upgrade_request_headers(const websocket_http_upgrade *upgrade, zval *headers)
{
	size_t i;

	array_init(headers);
	for (i = 0; i < upgrade->header_count; i++) {
		const websocket_http_header *header = &upgrade->headers[i];
		zend_string *lower_name = websocket_http_lower_header_name(header->name, header->name_len);
		zval *existing = zend_hash_find(Z_ARRVAL_P(headers), lower_name);
		zval header_value;

		if (existing && Z_TYPE_P(existing) == IS_STRING) {
			zend_string *joined = strpprintf(0, "%s, %.*s", Z_STRVAL_P(existing), (int) header->value_len, header->value);
			ZVAL_STR(&header_value, joined);
			zend_hash_update(Z_ARRVAL_P(headers), lower_name, &header_value);
		} else {
			ZVAL_STRINGL(&header_value, header->value, header->value_len);
			zend_hash_update(Z_ARRVAL_P(headers), lower_name, &header_value);
		}

		zend_string_release(lower_name);
	}
}

websocket_http_upgrade_result websocket_http_parse_upgrade(const char *buffer, const size_t len, HashTable *supported_subprotocols, websocket_http_upgrade *upgrade)
{
	const char *header_end;
	const char *line;
	const char *request_line_end;
	bool has_upgrade = false;
	bool has_connection = false;
	bool has_version = false;

	if (len > WEBSOCKET_HTTP_MAX_REQUEST_SIZE) {
		return WEBSOCKET_HTTP_UPGRADE_INVALID;
	}

	header_end = websocket_http_find_header_end(buffer, len);
	if (!header_end) {
		return WEBSOCKET_HTTP_UPGRADE_INCOMPLETE;
	}

	upgrade->bytes_consumed = (size_t) ((header_end - buffer) + 4);
	request_line_end = memchr(buffer, '\r', (size_t) (header_end - buffer));
	if (!request_line_end || request_line_end + 1 >= buffer + len || request_line_end[1] != '\n') {
		return WEBSOCKET_HTTP_UPGRADE_INVALID;
	}

	if (!websocket_http_parse_request_line(buffer, (size_t) (request_line_end - buffer), upgrade)) {
		return WEBSOCKET_HTTP_UPGRADE_INVALID;
	}

	line = request_line_end + 2;
	while (line < header_end) {
		const char *line_end = memchr(line, '\r', (size_t) (header_end - line) + 1);
		const char *colon;
		const char *name;
		const char *value;
		size_t line_len;
		size_t name_len;
		size_t value_len;

		if (!line_end || line_end + 1 >= buffer + len || line_end[1] != '\n') {
			return WEBSOCKET_HTTP_UPGRADE_INVALID;
		}

		line_len = (size_t) (line_end - line);
		if (line_len == 0) {
			break;
		}

		colon = memchr(line, ':', line_len);
		if (!colon) {
			return WEBSOCKET_HTTP_UPGRADE_INVALID;
		}

		name = line;
		name_len = (size_t) (colon - line);
		value = colon + 1;
		value_len = line_len - name_len - 1;
		websocket_http_trim(&name, &name_len);
		websocket_http_trim(&value, &value_len);
		websocket_http_append_header(upgrade, name, name_len, value, value_len);

		if (websocket_http_equals_ci(name, name_len, "Upgrade", strlen("Upgrade"))) {
			has_upgrade = websocket_http_header_contains_token(value, value_len, "websocket", strlen("websocket"));
		} else if (websocket_http_equals_ci(name, name_len, "Connection", strlen("Connection"))) {
			has_connection = websocket_http_header_contains_token(value, value_len, "upgrade", strlen("upgrade"));
		} else if (websocket_http_equals_ci(name, name_len, "Sec-WebSocket-Version", strlen("Sec-WebSocket-Version"))) {
			has_version = websocket_http_equals_ci(value, value_len, "13", strlen("13"));
		} else if (websocket_http_equals_ci(name, name_len, "Sec-WebSocket-Key", strlen("Sec-WebSocket-Key"))) {
			if (upgrade->accept_key) {
				return WEBSOCKET_HTTP_UPGRADE_INVALID;
			}

			if (!websocket_http_validate_key(value, value_len, &upgrade->accept_key)) {
				return WEBSOCKET_HTTP_UPGRADE_INVALID;
			}
		} else if (websocket_http_equals_ci(name, name_len, "Sec-WebSocket-Protocol", strlen("Sec-WebSocket-Protocol"))) {
			if (!websocket_http_select_subprotocol(value, value_len, supported_subprotocols, &upgrade->selected_subprotocol)) {
				return WEBSOCKET_HTTP_UPGRADE_INVALID;
			}
		}

		line = line_end + 2;
	}

	if (!has_upgrade || !has_connection || !has_version || !upgrade->accept_key) {
		return WEBSOCKET_HTTP_UPGRADE_INVALID;
	}

	return WEBSOCKET_HTTP_UPGRADE_OK;
}

zend_string *websocket_http_upgrade_response(zend_string *accept_key, zend_string *selected_subprotocol)
{
	if (selected_subprotocol) {
		return strpprintf(0,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"Sec-WebSocket-Protocol: %s\r\n"
			"\r\n",
			ZSTR_VAL(accept_key),
			ZSTR_VAL(selected_subprotocol));
	}

	return strpprintf(0,
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n"
		"\r\n",
		ZSTR_VAL(accept_key));
}
