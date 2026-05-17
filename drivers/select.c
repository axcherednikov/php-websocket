#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#include <errno.h>
#include <sys/select.h>

static int select_fd = -1;

static int websocket_select_init(void)
{
	select_fd = -1;
	return SUCCESS;
}

static void websocket_select_shutdown(void)
{
	select_fd = -1;
}

static int websocket_select_watch_read(const int fd)
{
	select_fd = fd;
	return SUCCESS;
}

static void websocket_select_unwatch(const int fd)
{
	if (select_fd == fd) {
		select_fd = -1;
	}
}

static int websocket_select_wait(const int timeout_usec)
{
	fd_set read_fds;
	struct timeval timeout;

	if (select_fd < 0) {
		errno = EBADF;
		return -1;
	}

	FD_ZERO(&read_fds);
	FD_SET(select_fd, &read_fds);

	timeout.tv_sec = timeout_usec / 1000000;
	timeout.tv_usec = timeout_usec % 1000000;

	return select(select_fd + 1, &read_fds, NULL, NULL, &timeout);
}

static websocket_driver select_driver = {
	"select",
	websocket_select_init,
	websocket_select_shutdown,
	websocket_select_watch_read,
	websocket_select_unwatch,
	websocket_select_wait,
};

websocket_driver *websocket_driver_select_get(void)
{
	return &select_driver;
}
