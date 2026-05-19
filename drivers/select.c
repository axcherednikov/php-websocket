/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#include <errno.h>
#include <string.h>
#include <sys/select.h>

typedef struct _websocket_select_fd {
	int fd;
	bool read;
	bool write;
} websocket_select_fd;

static websocket_select_fd *select_fds = NULL;
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

static websocket_select_fd *websocket_select_find_fd(const int fd)
{
	size_t i;

	for (i = 0; i < select_fd_count; i++) {
		if (select_fds[i].fd == fd) {
			return &select_fds[i];
		}
	}

	return NULL;
}

static websocket_select_fd *websocket_select_ensure_fd(const int fd)
{
	websocket_select_fd *entry;

	if (fd >= FD_SETSIZE) {
		errno = EMFILE;
		return NULL;
	}

	entry = websocket_select_find_fd(fd);
	if (entry) {
		return entry;
	}

	if (select_fd_count == select_fd_capacity) {
		select_fd_capacity = select_fd_capacity > 0 ? select_fd_capacity * 2 : 8;
		select_fds = select_fds ? erealloc(select_fds, sizeof(websocket_select_fd) * select_fd_capacity) : emalloc(sizeof(websocket_select_fd) * select_fd_capacity);
	}

	entry = &select_fds[select_fd_count++];
	entry->fd = fd;
	entry->read = false;
	entry->write = false;

	return entry;
}

static int websocket_select_watch_read(const int fd)
{
	websocket_select_fd *entry = websocket_select_ensure_fd(fd);

	if (!entry) {
		return FAILURE;
	}

	entry->read = true;

	return SUCCESS;
}

static int websocket_select_watch_write(const int fd)
{
	websocket_select_fd *entry = websocket_select_ensure_fd(fd);

	if (!entry) {
		return FAILURE;
	}

	entry->write = true;

	return SUCCESS;
}

static void websocket_select_remove_fd_at(const size_t index)
{
	if (index + 1 < select_fd_count) {
		memmove(&select_fds[index], &select_fds[index + 1], sizeof(websocket_select_fd) * (select_fd_count - index - 1));
	}
	select_fd_count--;
}

static void websocket_select_unwatch_write(const int fd)
{
	size_t i;

	for (i = 0; i < select_fd_count; i++) {
		if (select_fds[i].fd != fd) {
			continue;
		}

		select_fds[i].write = false;
		if (!select_fds[i].read) {
			websocket_select_remove_fd_at(i);
		}
		break;
	}
}

static void websocket_select_unwatch(const int fd)
{
	size_t i;

	for (i = 0; i < select_fd_count; i++) {
		if (select_fds[i].fd == fd) {
			websocket_select_remove_fd_at(i);
			break;
		}
	}
}

static int websocket_select_wait(const int timeout_usec, int *ready_fd)
{
	fd_set read_fds;
	fd_set write_fds;
	struct timeval timeout;
	int max_fd = -1;
	size_t i;
	int ready;

	if (select_fd_count == 0) {
		errno = EBADF;
		return -1;
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	for (i = 0; i < select_fd_count; i++) {
		if (select_fds[i].read) {
			FD_SET(select_fds[i].fd, &read_fds);
		}
		if (select_fds[i].write) {
			FD_SET(select_fds[i].fd, &write_fds);
		}
		if (select_fds[i].fd > max_fd) {
			max_fd = select_fds[i].fd;
		}
	}

	timeout.tv_sec = timeout_usec / 1000000;
	timeout.tv_usec = timeout_usec % 1000000;

	ready = select(max_fd + 1, &read_fds, &write_fds, NULL, &timeout);
	if (ready <= 0) {
		return ready;
	}

	for (i = 0; i < select_fd_count; i++) {
		if ((select_fds[i].read && FD_ISSET(select_fds[i].fd, &read_fds)) || (select_fds[i].write && FD_ISSET(select_fds[i].fd, &write_fds))) {
			*ready_fd = select_fds[i].fd;
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
	websocket_select_watch_write,
	websocket_select_unwatch_write,
	websocket_select_unwatch,
	websocket_select_wait,
};

websocket_driver *websocket_driver_select_get(void)
{
	return &select_driver;
}
