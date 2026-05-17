#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#ifdef HAVE_WEBSOCKET_POLL

#include <errno.h>
#include <poll.h>

static int poll_fd = -1;

static int websocket_poll_init(void)
{
	poll_fd = -1;
	return SUCCESS;
}

static void websocket_poll_shutdown(void)
{
	poll_fd = -1;
}

static int websocket_poll_watch_read(const int fd)
{
	poll_fd = fd;
	return SUCCESS;
}

static void websocket_poll_unwatch(const int fd)
{
	if (poll_fd == fd) {
		poll_fd = -1;
	}
}

static int websocket_poll_wait(const int timeout_usec)
{
	struct pollfd event;

	if (poll_fd < 0) {
		errno = EBADF;
		return -1;
	}

	event.fd = poll_fd;
	event.events = POLLIN;
	event.revents = 0;

	return poll(&event, 1, timeout_usec / 1000);
}

static websocket_driver poll_driver = {
	"poll",
	websocket_poll_init,
	websocket_poll_shutdown,
	websocket_poll_watch_read,
	websocket_poll_unwatch,
	websocket_poll_wait,
};

websocket_driver *websocket_driver_poll_get(void)
{
	return &poll_driver;
}

#endif /* HAVE_WEBSOCKET_POLL */
