# ext-websocket

Native PHP extension for WebSocket workers and Pusher Channels compatible realtime broadcasting.

This repository currently contains the first contract spike from the implementation plan:

- `WebSocket\Server`, `WebSocket\Connection`, and `WebSocket\MessageType`
- `Channels\Server` and `Channels\App`
- PHP stubs plus C arginfo/class registration
- PHP 8.1-8.5 compatibility shims in `php_websocket_compat.h`
- request-scoped module globals and native driver selection, following the `php-eventloop` layout
- a `.phpt` contract test

## PHP compatibility target

The extension is written to target PHP 8.1+ APIs. PHP 8.1 is the minimum because the public contract uses enums and readonly properties.

For open-source development, PHP 8.3 is the recommended baseline to run locally and in CI:

```sh
php8.3 -v
phpize8.3
./configure --enable-websocket --with-php-config="$(command -v php-config8.3)"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)"
TEST_PHP_EXECUTABLE="$(command -v php8.3)" \
TEST_PHP_ARGS="-d extension=$PWD/modules/websocket.so" \
php8.3 run-tests.php -q tests
```

If your system exposes PHP 8.3 as plain `php`, `phpize`, and `php-config`, use the same commands without the `8.3` suffix:

```sh
phpize
./configure --enable-websocket
make -j2
TEST_PHP_ARGS="-d extension=$PWD/modules/websocket.so" php run-tests.php -q tests
```

## Current status

The extension builds and loads, but the networking engine is not implemented yet. `listen()`, callback registration, `run()`, and Channels API methods currently establish the object lifecycle and validation surface for the next phase.

The internal shape intentionally mirrors `php-eventloop`: `config.m4` detects `epoll`, `kqueue`, `poll`, and `select`; `php_websocket.h` owns the module globals and driver interface; `websocket.c` owns `MINIT`, `RINIT`, `RSHUTDOWN`, `MINFO`, and best-driver selection.

Next implementation phase:

1. TCP listener and accept loop.
2. HTTP upgrade parser.
3. RFC 6455 text frame receive/send.
4. `onOpen`, `onMessage`, `onClose`, and `onError` callback dispatch.
