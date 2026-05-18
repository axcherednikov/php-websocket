#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#include <errno.h>
#include <string.h>
#include <sys/select.h>

static int *select_fds = NULL;
static size_t select_fd_count = 0;
static size_t select_fd_capacity = 0;

static int websocket_select_init(void)
{
	select_fds = NULL;
	select_fd_count = 0;
	select_fd_capacity = 0;
	return SUCCESS;
}

static void websocket_select_shutdown(void)
{
	if (select_fds) {
		efree(select_fds);
		select_fds = NULL;
	}

	select_fd_count = 0;
	select_fd_capacity = 0;
}

static int websocket_select_watch_read(const int fd)
{
	size_t i;

	for (i = 0; i < select_fd_count; i++) {
		if (select_fds[i] == fd) {
			return SUCCESS;
		}
	}

	if (fd >= FD_SETSIZE) {
		errno = EMFILE;
		return FAILURE;
	}

	if (select_fd_count == select_fd_capacity) {
		select_fd_capacity = select_fd_capacity > 0 ? select_fd_capacity * 2 : 8;
		select_fds = select_fds ? erealloc(select_fds, sizeof(int) * select_fd_capacity) : emalloc(sizeof(int) * select_fd_capacity);
	}

	select_fds[select_fd_count++] = fd;

	return SUCCESS;
}

static void websocket_select_unwatch(const int fd)
{
	size_t i;

	for (i = 0; i < select_fd_count; i++) {
		if (select_fds[i] != fd) {
			continue;
		}

		if (i + 1 < select_fd_count) {
			memmove(&select_fds[i], &select_fds[i + 1], sizeof(int) * (select_fd_count - i - 1));
		}
		select_fd_count--;
		break;
	}
}

static int websocket_select_wait(const int timeout_usec, int *ready_fd)
{
	fd_set read_fds;
	struct timeval timeout;
	int max_fd = -1;
	size_t i;

	if (select_fd_count == 0) {
		errno = EBADF;
		return -1;
	}

	FD_ZERO(&read_fds);
	for (i = 0; i < select_fd_count; i++) {
		FD_SET(select_fds[i], &read_fds);
		if (select_fds[i] > max_fd) {
			max_fd = select_fds[i];
		}
	}

	timeout.tv_sec = timeout_usec / 1000000;
	timeout.tv_usec = timeout_usec % 1000000;

	const int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
	if (ready <= 0) {
		return ready;
	}

	for (i = 0; i < select_fd_count; i++) {
		if (FD_ISSET(select_fds[i], &read_fds)) {
			*ready_fd = select_fds[i];
			return ready;
		}
	}

	return ready;
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
