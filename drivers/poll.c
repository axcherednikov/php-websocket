#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#ifdef HAVE_WEBSOCKET_POLL

#include <errno.h>
#include <poll.h>
#include <string.h>

static struct pollfd *poll_fds = NULL;
static size_t poll_fd_count = 0;
static size_t poll_fd_capacity = 0;

static int websocket_poll_init(void)
{
	poll_fds = NULL;
	poll_fd_count = 0;
	poll_fd_capacity = 0;
	return SUCCESS;
}

static void websocket_poll_shutdown(void)
{
	if (poll_fds) {
		efree(poll_fds);
		poll_fds = NULL;
	}

	poll_fd_count = 0;
	poll_fd_capacity = 0;
}

static int websocket_poll_watch_read(const int fd)
{
	size_t i;

	for (i = 0; i < poll_fd_count; i++) {
		if (poll_fds[i].fd == fd) {
			return SUCCESS;
		}
	}

	if (poll_fd_count == poll_fd_capacity) {
		poll_fd_capacity = poll_fd_capacity > 0 ? poll_fd_capacity * 2 : 8;
		poll_fds = poll_fds ? erealloc(poll_fds, sizeof(struct pollfd) * poll_fd_capacity) : emalloc(sizeof(struct pollfd) * poll_fd_capacity);
	}

	poll_fds[poll_fd_count].fd = fd;
	poll_fds[poll_fd_count].events = POLLIN;
	poll_fds[poll_fd_count].revents = 0;
	poll_fd_count++;

	return SUCCESS;
}

static void websocket_poll_unwatch(const int fd)
{
	size_t i;

	for (i = 0; i < poll_fd_count; i++) {
		if (poll_fds[i].fd != fd) {
			continue;
		}

		if (i + 1 < poll_fd_count) {
			memmove(&poll_fds[i], &poll_fds[i + 1], sizeof(struct pollfd) * (poll_fd_count - i - 1));
		}
		poll_fd_count--;
		break;
	}
}

static int websocket_poll_wait(const int timeout_usec, int *ready_fd)
{
	size_t i;

	if (poll_fd_count == 0) {
		errno = EBADF;
		return -1;
	}

	const int ready = poll(poll_fds, (nfds_t) poll_fd_count, timeout_usec / 1000);
	if (ready <= 0) {
		return ready;
	}

	for (i = 0; i < poll_fd_count; i++) {
		if (poll_fds[i].revents != 0) {
			*ready_fd = poll_fds[i].fd;
			return ready;
		}
	}

	return ready;
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
