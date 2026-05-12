#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#ifdef HAVE_WEBSOCKET_POLL

static int websocket_poll_init(void)
{
	return SUCCESS;
}

static void websocket_poll_shutdown(void)
{
}

static int websocket_poll_poll(double timeout)
{
	(void) timeout;
	return 0;
}

static websocket_driver poll_driver = {
	"poll",
	websocket_poll_init,
	websocket_poll_shutdown,
	websocket_poll_poll,
};

websocket_driver *websocket_driver_poll_get(void)
{
	return &poll_driver;
}

#endif /* HAVE_WEBSOCKET_POLL */
