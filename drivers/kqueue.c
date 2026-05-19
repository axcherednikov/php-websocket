/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_websocket.h"

#ifdef HAVE_WEBSOCKET_KQUEUE

#include <errno.h>
#include <sys/event.h>
#include <time.h>
#include <unistd.h>

static int kqueue_fd = -1;

static int websocket_kqueue_init(void)
{
	kqueue_fd = kqueue();
	if (kqueue_fd < 0) {
		return FAILURE;
	}

	return SUCCESS;
}

static void websocket_kqueue_shutdown(void)
{
	if (kqueue_fd >= 0) {
		while (close(kqueue_fd) < 0 && errno == EINTR) {
		}
		kqueue_fd = -1;
	}
}

static int websocket_kqueue_watch_read(const int fd)
{
	struct kevent event;

	if (kqueue_fd < 0) {
		errno = EBADF;
		return FAILURE;
	}

	EV_SET(&event, (uintptr_t) fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL);

	while (kevent(kqueue_fd, &event, 1, NULL, 0, NULL) < 0) {
		if (errno == EINTR) {
			continue;
		}

		return FAILURE;
	}

	return SUCCESS;
}

static int websocket_kqueue_watch_write(const int fd)
{
	struct kevent event;

	if (kqueue_fd < 0) {
		errno = EBADF;
		return FAILURE;
	}

	EV_SET(&event, (uintptr_t) fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL);

	while (kevent(kqueue_fd, &event, 1, NULL, 0, NULL) < 0) {
		if (errno == EINTR) {
			continue;
		}

		return FAILURE;
	}

	return SUCCESS;
}

static void websocket_kqueue_unwatch_event(const int fd, const int16_t filter)
{
	struct kevent event;

	if (kqueue_fd < 0) {
		return;
	}

	EV_SET(&event, (uintptr_t) fd, filter, EV_DELETE, 0, 0, NULL);
	while (kevent(kqueue_fd, &event, 1, NULL, 0, NULL) < 0 && errno == EINTR) {
	}
}

static void websocket_kqueue_unwatch_write(const int fd)
{
	const int saved_errno = errno;

	websocket_kqueue_unwatch_event(fd, EVFILT_WRITE);
	errno = saved_errno;
}

static void websocket_kqueue_unwatch(const int fd)
{
	const int saved_errno = errno;

	websocket_kqueue_unwatch_event(fd, EVFILT_READ);
	websocket_kqueue_unwatch_event(fd, EVFILT_WRITE);
	errno = saved_errno;
}

static int websocket_kqueue_wait(const int timeout_usec, int *ready_fd)
{
	struct kevent event;
	struct timespec timeout;
	int ready;

	if (kqueue_fd < 0) {
		errno = EBADF;
		return -1;
	}

	timeout.tv_sec = timeout_usec / 1000000;
	timeout.tv_nsec = (timeout_usec % 1000000) * 1000;

	ready = kevent(kqueue_fd, NULL, 0, &event, 1, &timeout);
	if (ready < 0) {
		return -1;
	}

	if (ready > 0 && (event.flags & EV_ERROR) != 0) {
		errno = event.data != 0 ? (int) event.data : EIO;
		return -1;
	}

	if (ready > 0) {
		*ready_fd = (int) event.ident;
	}

	return ready;
}

static websocket_driver kqueue_driver = {
	"kqueue",
	websocket_kqueue_init,
	websocket_kqueue_shutdown,
	websocket_kqueue_watch_read,
	websocket_kqueue_watch_write,
	websocket_kqueue_unwatch_write,
	websocket_kqueue_unwatch,
	websocket_kqueue_wait,
};

websocket_driver *websocket_driver_kqueue_get(void)
{
	return &kqueue_driver;
}

#endif /* HAVE_WEBSOCKET_KQUEUE */
