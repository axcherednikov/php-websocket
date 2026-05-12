#include "php_websocket.h"
#include "ext/standard/info.h"
#include "php_websocket_compat.h"
#include "websocket_arginfo.h"

ZEND_DECLARE_MODULE_GLOBALS(websocket)

zend_class_entry *websocket_server_ce;
zend_class_entry *websocket_connection_ce;
zend_class_entry *websocket_message_type_ce;
zend_class_entry *websocket_frame_ce;
zend_class_entry *websocket_close_frame_ce;
zend_class_entry *websocket_protocol_ce;
zend_class_entry *channels_app_ce;
zend_class_entry *channels_server_ce;

websocket_driver *websocket_select_best_driver(void)
{
	websocket_driver *d;

#ifdef HAVE_WEBSOCKET_EPOLL
	d = websocket_driver_epoll_get();
	if (d && d->init() == SUCCESS) {
		return d;
	}
#endif
#ifdef HAVE_WEBSOCKET_KQUEUE
	d = websocket_driver_kqueue_get();
	if (d && d->init() == SUCCESS) {
		return d;
	}
#endif
#ifdef HAVE_WEBSOCKET_POLL
	d = websocket_driver_poll_get();
	if (d && d->init() == SUCCESS) {
		return d;
	}
#endif
	d = websocket_driver_select_get();
	if (d && d->init() == SUCCESS) {
		return d;
	}

	return NULL;
}

const char *websocket_best_driver_name(void)
{
#ifdef HAVE_WEBSOCKET_EPOLL
	return "epoll";
#elif defined(HAVE_WEBSOCKET_KQUEUE)
	return "kqueue";
#elif defined(HAVE_WEBSOCKET_POLL)
	return "poll";
#else
	return "select";
#endif
}

static PHP_GINIT_FUNCTION(websocket)
{
#if defined(COMPILE_DL_WEBSOCKET) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	memset(websocket_globals, 0, sizeof(*websocket_globals));
}

PHP_MINIT_FUNCTION(websocket)
{
	websocket_register_server_class();
	websocket_register_connection_class();
	websocket_message_type_ce = register_class_WebSocket_MessageType();
	websocket_register_protocol_classes();
	websocket_register_channels_app_class();
	websocket_register_channels_server_class();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(websocket)
{
	WEBSOCKET_G(driver) = NULL;
	WEBSOCKET_G(next_connection_id) = 1;
	WEBSOCKET_G(running) = false;
	WEBSOCKET_G(stopped) = false;

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(websocket)
{
	if (WEBSOCKET_G(driver)) {
		WEBSOCKET_G(driver)->shutdown();
		WEBSOCKET_G(driver) = NULL;
	}

	return SUCCESS;
}

PHP_MINFO_FUNCTION(websocket)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "websocket support", "enabled");
	php_info_print_table_row(2, "version", PHP_WEBSOCKET_VERSION);
	php_info_print_table_row(2, "best available driver", websocket_best_driver_name());
#ifdef HAVE_WEBSOCKET_EPOLL
	php_info_print_table_row(2, "epoll support", "yes");
#else
	php_info_print_table_row(2, "epoll support", "no");
#endif
#ifdef HAVE_WEBSOCKET_KQUEUE
	php_info_print_table_row(2, "kqueue support", "yes");
#else
	php_info_print_table_row(2, "kqueue support", "no");
#endif
#ifdef HAVE_WEBSOCKET_POLL
	php_info_print_table_row(2, "poll support", "yes");
#else
	php_info_print_table_row(2, "poll support", "no");
#endif
	php_info_print_table_row(2, "select support", "yes");
	php_info_print_table_end();
}

zend_module_entry websocket_module_entry = {
	STANDARD_MODULE_HEADER,
	"websocket",
	NULL,
	PHP_MINIT(websocket),
	NULL,
	PHP_RINIT(websocket),
	PHP_RSHUTDOWN(websocket),
	PHP_MINFO(websocket),
	PHP_WEBSOCKET_VERSION,
	PHP_MODULE_GLOBALS(websocket),
	PHP_GINIT(websocket),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_WEBSOCKET
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(websocket)
#endif
