#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

static int websocket_select_init(void)
{
	return SUCCESS;
}

static void websocket_select_shutdown(void)
{
}

static int websocket_select_poll(double timeout)
{
	(void) timeout;
	return 0;
}

static websocket_driver select_driver = {
	"select",
	websocket_select_init,
	websocket_select_shutdown,
	websocket_select_poll,
};

websocket_driver *websocket_driver_select_get(void)
{
	return &select_driver;
}
