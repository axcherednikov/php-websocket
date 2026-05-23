/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

#ifndef PHP_WEBSOCKET_H
#define PHP_WEBSOCKET_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_network.h"
#include "Zend/zend_closures.h"
#include "Zend/zend_enum.h"

#include <sys/socket.h>

extern zend_module_entry websocket_module_entry;
#define phpext_websocket_ptr &websocket_module_entry

#define PHP_WEBSOCKET_VERSION "1.0.0"
#define WEBSOCKET_HTTP_MAX_REQUEST_SIZE 8192
#define WEBSOCKET_DEFAULT_MAX_MESSAGE_SIZE (16 * 1024 * 1024)
#define WEBSOCKET_DEFAULT_MAX_QUEUED_BYTES (16 * 1024 * 1024)
#define WEBSOCKET_DEFAULT_MAX_CONNECTIONS 10000
#define WEBSOCKET_DEFAULT_HANDSHAKE_TIMEOUT_MS 10000
#define WEBSOCKET_DEFAULT_IDLE_TIMEOUT_MS 120000
#define WEBSOCKET_CLOSE_REASON_MAX_LEN 123

#define WEBSOCKET_OPCODE_CONTINUATION 0x0
#define WEBSOCKET_OPCODE_TEXT         0x1
#define WEBSOCKET_OPCODE_BINARY       0x2
#define WEBSOCKET_OPCODE_CLOSE        0x8
#define WEBSOCKET_OPCODE_PING         0x9
#define WEBSOCKET_OPCODE_PONG         0xA

#define WEBSOCKET_FLAG_FIN      (1 << 0)
#define WEBSOCKET_FLAG_COMPRESS (1 << 1)
#define WEBSOCKET_FLAG_RSV1     (1 << 2)
#define WEBSOCKET_FLAG_RSV2     (1 << 3)
#define WEBSOCKET_FLAG_RSV3     (1 << 4)
#define WEBSOCKET_FLAG_MASK     (1 << 5)
#define WEBSOCKET_FLAGS_ALL \
	(WEBSOCKET_FLAG_FIN | WEBSOCKET_FLAG_COMPRESS | WEBSOCKET_FLAG_RSV1 | \
	 WEBSOCKET_FLAG_RSV2 | WEBSOCKET_FLAG_RSV3 | WEBSOCKET_FLAG_MASK)

#define WEBSOCKET_CLOSE_NORMAL 1000
#define WEBSOCKET_CLOSE_PROTOCOL_ERROR 1002
#define WEBSOCKET_CLOSE_INVALID_PAYLOAD 1007
#define WEBSOCKET_CLOSE_MESSAGE_TOO_BIG 1009

#ifdef ZTS
#include "TSRM.h"
#endif

typedef struct _websocket_driver {
	const char *name;
	int (*init)(void);
	void (*shutdown)(void);
	int (*watch_read)(int fd);
	int (*watch_write)(int fd);
	void (*unwatch_write)(int fd);
	void (*unwatch)(int fd);
	int (*wait)(int timeout_usec, int *ready_fd);
} websocket_driver;

typedef enum _websocket_http_upgrade_result {
	WEBSOCKET_HTTP_UPGRADE_INCOMPLETE = 0,
	WEBSOCKET_HTTP_UPGRADE_OK = 1,
	WEBSOCKET_HTTP_UPGRADE_INVALID = 2,
} websocket_http_upgrade_result;

ZEND_BEGIN_MODULE_GLOBALS(websocket)
	websocket_driver *driver;
	uint64_t next_connection_id;
	bool running;
	bool stopped;
ZEND_END_MODULE_GLOBALS(websocket)

ZEND_EXTERN_MODULE_GLOBALS(websocket)
#define WEBSOCKET_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(websocket, v)

#if defined(ZTS) && defined(COMPILE_DL_WEBSOCKET)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

typedef struct _websocket_server_object {
	zval options;
	zval on_open;
	zval on_message;
	zval on_close;
	zval on_error;
	zend_fcall_info_cache on_open_cache;
	uint32_t on_open_param_count;
	bool on_open_cache_initialized;
	zend_string *host;
	zend_long port;
	bool listening;
	bool running;
	int listener_fd;
	zval reusable_connection;
	zval *connections;
	size_t connection_count;
	size_t connection_capacity;
	zend_object std;
} websocket_server_object;

typedef struct _websocket_connection_object {
	zend_string *id;
	zend_string *remote_address;
	struct sockaddr_storage remote_addr;
	socklen_t remote_addr_len;
	uint64_t numeric_id;
	int fd;
	bool open;
	bool upgraded;
	bool close_notified;
	bool has_remote_addr;
	bool defer_close;
	char *read_buffer;
	size_t read_buffer_len;
	size_t read_buffer_capacity;
	zend_string **write_queue;
	size_t write_queue_count;
	size_t write_queue_capacity;
	size_t write_queue_offset;
	size_t queued_bytes;
	size_t max_queued_bytes;
	bool write_watched;
	bool close_after_write;
	bool fragmented;
	uint8_t fragmented_opcode;
	zend_string *fragmented_payload;
	uint64_t accepted_at_usec;
	uint64_t last_activity_usec;
	zend_object std;
} websocket_connection_object;

extern zend_class_entry *websocket_server_ce;
extern zend_class_entry *websocket_server_options_ce;
extern zend_class_entry *websocket_connection_ce;
extern zend_class_entry *websocket_message_type_ce;
extern zend_class_entry *websocket_frame_ce;
extern zend_class_entry *websocket_close_frame_ce;
extern zend_class_entry *websocket_protocol_ce;

void websocket_register_server_class(void);
void websocket_register_connection_class(void);
void websocket_register_protocol_classes(void);
bool websocket_server_runtime_run(websocket_server_object *intern);
void websocket_server_runtime_close(websocket_server_object *intern, bool notify);
void websocket_server_runtime_free(websocket_server_object *intern);

websocket_driver *websocket_driver_select_get(void);
#ifdef HAVE_WEBSOCKET_POLL
websocket_driver *websocket_driver_poll_get(void);
#endif
#ifdef HAVE_WEBSOCKET_EPOLL
websocket_driver *websocket_driver_epoll_get(void);
#endif
#ifdef HAVE_WEBSOCKET_KQUEUE
websocket_driver *websocket_driver_kqueue_get(void);
#endif

websocket_driver *websocket_select_best_driver(void);
const char *websocket_best_driver_name(void);

websocket_server_object *websocket_server_from_obj(zend_object *obj);
websocket_connection_object *websocket_connection_from_obj(zend_object *obj);
void websocket_connection_open(websocket_connection_object *intern, uint64_t id, const struct sockaddr *remote_addr, socklen_t remote_addr_len, const int fd);
void websocket_connection_cache_remote_address(websocket_connection_object *intern);
void websocket_connection_close_socket(websocket_connection_object *intern);
bool websocket_connection_flush(websocket_connection_object *intern);
bool websocket_connection_has_pending_writes(websocket_connection_object *intern);
void websocket_connection_close_after_write(websocket_connection_object *intern);
bool websocket_connection_send_frame(websocket_connection_object *intern, zend_string *payload, uint8_t opcode);
bool websocket_connection_send_close_frame(websocket_connection_object *intern, zend_long code, zend_string *reason);
zend_string *websocket_protocol_accept_key(zend_string *key);
uint8_t websocket_protocol_message_type_opcode(zval *type);
zend_object *websocket_protocol_message_type_from_opcode(uint8_t opcode);
bool websocket_protocol_opcode_is_valid(zend_long opcode);
bool websocket_protocol_opcode_is_control(zend_long opcode);
bool websocket_protocol_close_code_is_valid(zend_long code);
bool websocket_protocol_is_valid_utf8(const char *payload, size_t payload_len);
zend_string *websocket_protocol_pack_payload(zend_string *payload, uint8_t opcode, uint8_t flags);
zend_string *websocket_protocol_close_payload(zend_long code, zend_string *reason);
websocket_http_upgrade_result websocket_http_parse_upgrade(const char *buffer, size_t len, zend_string **accept_key, size_t *bytes_consumed);
zend_string *websocket_http_upgrade_response(zend_string *accept_key);

static zend_always_inline websocket_server_object *Z_WEBSOCKET_SERVER_P(zval *zv)
{
	return websocket_server_from_obj(Z_OBJ_P(zv));
}

static zend_always_inline websocket_connection_object *Z_WEBSOCKET_CONNECTION_P(zval *zv)
{
	return websocket_connection_from_obj(Z_OBJ_P(zv));
}

#endif
