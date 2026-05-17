#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#ifdef HAVE_WEBSOCKET_EPOLL

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

static int epoll_fd = -1;

static int websocket_epoll_init(void)
{
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd < 0) {
		return FAILURE;
	}

	return SUCCESS;
}

static void websocket_epoll_shutdown(void)
{
	if (epoll_fd >= 0) {
		while (close(epoll_fd) < 0 && errno == EINTR) {
		}
		epoll_fd = -1;
	}
}

static int websocket_epoll_watch_read(const int fd)
{
	struct epoll_event event;

	if (epoll_fd < 0) {
		errno = EBADF;
		return FAILURE;
	}

	event.events = EPOLLIN;
	event.data.fd = fd;

	return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == 0 ? SUCCESS : FAILURE;
}

static void websocket_epoll_unwatch(const int fd)
{
	const int saved_errno = errno;

	if (epoll_fd >= 0) {
		(void) epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	}

	errno = saved_errno;
}

static int websocket_epoll_wait(const int timeout_usec)
{
	struct epoll_event event;

	if (epoll_fd < 0) {
		errno = EBADF;
		return -1;
	}

	return epoll_wait(epoll_fd, &event, 1, timeout_usec / 1000);
}

static websocket_driver epoll_driver = {
	"epoll",
	websocket_epoll_init,
	websocket_epoll_shutdown,
	websocket_epoll_watch_read,
	websocket_epoll_unwatch,
	websocket_epoll_wait,
};

websocket_driver *websocket_driver_epoll_get(void)
{
	return &epoll_driver;
}

#endif /* HAVE_WEBSOCKET_EPOLL */
