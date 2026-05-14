#ifndef PHP_WEBSOCKET_H
#define PHP_WEBSOCKET_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_network.h"
#include "Zend/zend_closures.h"

extern zend_module_entry websocket_module_entry;
#define phpext_websocket_ptr &websocket_module_entry

#define PHP_WEBSOCKET_VERSION "0.1.0-dev"

#ifdef ZTS
#include "TSRM.h"
#endif

typedef struct _websocket_driver {
	const char *name;
	int (*init)(void);
	void (*shutdown)(void);
	int (*poll)(double timeout);
} websocket_driver;

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
	zend_object std;
	zval options;
	zval on_open;
	zval on_message;
	zval on_close;
	zval on_error;
	zend_string *host;
	zend_long port;
	bool listening;
	bool running;
} websocket_server_object;

typedef struct _websocket_connection_object {
	zend_object std;
	zend_string *id;
	zend_string *remote_address;
	bool open;
} websocket_connection_object;

extern zend_class_entry *websocket_server_ce;
extern zend_class_entry *websocket_connection_ce;
extern zend_class_entry *websocket_message_type_ce;
extern zend_class_entry *websocket_frame_ce;
extern zend_class_entry *websocket_close_frame_ce;
extern zend_class_entry *websocket_protocol_ce;

void websocket_register_server_class(void);
void websocket_register_connection_class(void);
void websocket_register_protocol_classes(void);

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

static zend_always_inline websocket_server_object *Z_WEBSOCKET_SERVER_P(zval *zv)
{
	return websocket_server_from_obj(Z_OBJ_P(zv));
}

static zend_always_inline websocket_connection_object *Z_WEBSOCKET_CONNECTION_P(zval *zv)
{
	return websocket_connection_from_obj(Z_OBJ_P(zv));
}

#endif
