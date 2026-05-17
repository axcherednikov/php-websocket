PHP_ARG_ENABLE([websocket],
  [whether to enable native websocket support],
  [AS_HELP_STRING([--enable-websocket],
    [Enable native WebSocket extension])],
  [no])

if test "$PHP_WEBSOCKET" != "no"; then
  AC_DEFINE([HAVE_WEBSOCKET], [1], [Whether WebSocket support is enabled])

  AC_CHECK_HEADERS([sys/epoll.h], [
    AC_DEFINE([HAVE_WEBSOCKET_EPOLL], [1], [Have epoll support])
  ])

  AC_CHECK_HEADERS([sys/event.h], [
    AC_DEFINE([HAVE_WEBSOCKET_KQUEUE], [1], [Have kqueue support])
  ])

  AC_CHECK_HEADERS([poll.h], [
    AC_DEFINE([HAVE_WEBSOCKET_POLL], [1], [Have poll support])
  ])

  WEBSOCKET_SOURCES="websocket.c websocket_server.c websocket_server_runtime.c websocket_connection.c websocket_protocol.c drivers/select.c"

  if test "$ac_cv_header_poll_h" = "yes"; then
    WEBSOCKET_SOURCES="$WEBSOCKET_SOURCES drivers/poll.c"
  fi

  if test "$ac_cv_header_sys_epoll_h" = "yes"; then
    WEBSOCKET_SOURCES="$WEBSOCKET_SOURCES drivers/epoll.c"
  fi

  if test "$ac_cv_header_sys_event_h" = "yes"; then
    WEBSOCKET_SOURCES="$WEBSOCKET_SOURCES drivers/kqueue.c"
  fi

  PHP_NEW_EXTENSION([websocket], [$WEBSOCKET_SOURCES], [$ext_shared])
  PHP_ADD_BUILD_DIR([$ext_builddir/drivers])
fi
