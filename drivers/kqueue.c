#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#ifdef HAVE_WEBSOCKET_KQUEUE

static int websocket_kqueue_init(void)
{
	return SUCCESS;
}

static void websocket_kqueue_shutdown(void)
{
}

static int websocket_kqueue_poll(double timeout)
{
	(void) timeout;
	return 0;
}

static websocket_driver kqueue_driver = {
	"kqueue",
	websocket_kqueue_init,
	websocket_kqueue_shutdown,
	websocket_kqueue_poll,
};

websocket_driver *websocket_driver_kqueue_get(void)
{
	return &kqueue_driver;
}

#endif /* HAVE_WEBSOCKET_KQUEUE */
