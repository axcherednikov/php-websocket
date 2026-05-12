#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#ifdef HAVE_WEBSOCKET_EPOLL

static int websocket_epoll_init(void)
{
	return SUCCESS;
}

static void websocket_epoll_shutdown(void)
{
}

static int websocket_epoll_poll(double timeout)
{
	(void) timeout;
	return 0;
}

static websocket_driver epoll_driver = {
	"epoll",
	websocket_epoll_init,
	websocket_epoll_shutdown,
	websocket_epoll_poll,
};

websocket_driver *websocket_driver_epoll_get(void)
{
	return &epoll_driver;
}

#endif /* HAVE_WEBSOCKET_EPOLL */
