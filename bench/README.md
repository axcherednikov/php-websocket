# Benchmarks

These benchmarks compare the current native protocol helpers and native server runtime with PHP WebSocket libraries.
The protocol AMPHP entry point installs `amphp/websocket-server` and measures the RFC 6455 parser/compiler it uses through `amphp/websocket`.
The protocol OpenSwoole entry point measures `OpenSwoole\WebSocket\Server::pack()` and `OpenSwoole\WebSocket\Server::unpack()`.
OpenSwoole is optional because it is a PHP extension installed outside Composer.

The protocol suite measures hot paths:

- server-side text frame encoding
- masked client text frame decoding

The server runtime suite measures raw TCP accept loops and the native HTTP Upgrade close path.

The message runtime suite measures complete WebSocket server scenarios:

- upgraded idle connections
- pipelined echo messages
- broadcast fanout deliveries
- direct `ws://` transport and `wss://` through the same local TLS terminator

## Results

### Protocol

**Environment:** PHP 8.4.21, `xdebug.mode=off`, `zend.assertions=-1`, Apple Silicon macOS, 100,000 iterations for 64B payloads and 20,000 iterations for 1024B payloads. Results from May 18, 2026.
AMPHP WebSocket Server v4.0.0, Ratchet v0.4.0, and Workerman v5.2.0 were installed from Composer. OpenSwoole v26.2.0 was installed from PECL.

| Benchmark | amphp/websocket-server | ratchet/rfc6455 | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|--:|
| `encode text 64B` | 3,102,715 ops/sec | 1,493,649 ops/sec | 4,406,847 ops/sec | 2,679,178 ops/sec | **15,134,317 ops/sec** |
| `decode masked text 64B` | 592,968 ops/sec | 566,960 ops/sec | 1,776,096 ops/sec | 4,058,675 ops/sec | **7,020,335 ops/sec** |
| `encode text 1024B` | 2,467,892 ops/sec | 1,323,974 ops/sec | 3,537,032 ops/sec | 6,056,783 ops/sec | **12,041,548 ops/sec** |
| `decode masked text 1024B` | 358,676 ops/sec | 278,047 ops/sec | 846,768 ops/sec | 3,694,837 ops/sec | **5,155,027 ops/sec** |

### Server Runtime

**Environment:** PHP 8.4.21, `xdebug.mode=off`, `zend.assertions=-1`, Apple Silicon macOS, 1,000 connections, average of 3 runs. Results from May 18, 2026. ext-websocket used the native `kqueue` driver; OpenSwoole was built with `kqueue` enabled.

| Benchmark | amphp/socket | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|
| `tcp accept/close` | **16,595 connections/sec** | 16,580 connections/sec | 16,032 connections/sec | n/a |
| `client connect loop` | **7,621 connections/sec** | 7,078 connections/sec | 3,555 connections/sec | n/a |
| `websocket upgrade/close` | n/a | n/a | n/a | 11,055 connections/sec |
| `client upgrade loop` | n/a | n/a | n/a | 6,260 connections/sec |

> This starts a fresh server process, then measures the current server runtime surface up to the last accepted connection. The ext-websocket entry performs a real HTTP Upgrade before `onOpen(): false` closes the connection; the TCP-only comparison rows are kept as raw accept-loop references. Application message throughput lives in the message-runtime table below. `ratchet/rfc6455` is not listed here because the benchmarked package exposes protocol helpers, not a TCP server runtime.

### Real `ws`/`wss` Message Runtime

**Environment:** PHP 8.4.21, `xdebug.mode=off`, `zend.assertions=-1`, Apple Silicon macOS, 50 upgraded connections, 1,000 pipelined echo/broadcast source messages, 1024B text payloads, average of 3 runs. Results from May 18, 2026. The `wss` rows use the same local TLS terminator in front of each server, so the comparison isolates WebSocket runtime behavior under encrypted transport rather than comparing different TLS implementations.

`ws://`

| Benchmark | amphp/websocket-server | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|
| `idle upgraded connections` | 2,785 connections/sec | **5,363 connections/sec** | 4,783 connections/sec | 5,082 connections/sec |
| `echo pipelined messages` | 45,502 messages/sec | 51,332 messages/sec | 50,515 messages/sec | **70,059 messages/sec** |
| `broadcast fanout deliveries` | 186,907 deliveries/sec | 227,944 deliveries/sec | 229,098 deliveries/sec | **818,959 deliveries/sec** |

`wss://` with the shared local TLS terminator:

| Benchmark | amphp/websocket-server | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|
| `idle upgraded connections` | 356 connections/sec | 428 connections/sec | 412 connections/sec | **442 connections/sec** |
| `echo pipelined messages` | 24,783 messages/sec | 29,501 messages/sec | **31,391 messages/sec** | 29,735 messages/sec |
| `broadcast fanout deliveries` | 105,500 deliveries/sec | 163,485 deliveries/sec | 210,075 deliveries/sec | **610,215 deliveries/sec** |

## Install Dependencies

```bash
(cd bench && composer install)
```

OpenSwoole is optional and is installed as a PHP extension:

```bash
pecl install openswoole-26.2.0
```

On Homebrew macOS, if `pcre2.h` is not found during compilation:

```bash
CPPFLAGS="-I/opt/homebrew/include" LDFLAGS="-L/opt/homebrew/lib" pecl install -f openswoole-26.2.0
```

## Run

From the repository root, build the extension first, then run the benchmark with the same PHP binary:

```bash
# ext-websocket
php -d xdebug.mode=off -d zend.assertions=-1 -d extension="$PWD/modules/websocket.so" bench/protocol/websocket.php [iterations]
php -d xdebug.mode=off -d zend.assertions=-1 -d extension="$PWD/modules/websocket.so" bench/server-runtime/websocket.php [connections] [rounds]
php -d xdebug.mode=off -d zend.assertions=-1 -d extension="$PWD/modules/websocket.so" bench/message-runtime/websocket.php [connections] [messages] [rounds] [ws|wss|both] [payload-bytes]

# PHP libraries
php -d xdebug.mode=off -d zend.assertions=-1 bench/protocol/amphp.php [iterations]
php -d xdebug.mode=off -d zend.assertions=-1 bench/server-runtime/amphp.php [connections] [rounds]
php -d xdebug.mode=off -d zend.assertions=-1 bench/message-runtime/amphp.php [connections] [messages] [rounds] [ws|wss|both] [payload-bytes]
php -d xdebug.mode=off -d zend.assertions=-1 bench/protocol/ratchet.php [iterations]
php -d xdebug.mode=off -d zend.assertions=-1 bench/protocol/workerman.php [iterations]
php -d xdebug.mode=off -d zend.assertions=-1 bench/server-runtime/workerman.php [connections] [rounds]
php -d xdebug.mode=off -d zend.assertions=-1 bench/message-runtime/workerman.php [connections] [messages] [rounds] [ws|wss|both] [payload-bytes]

# Native OpenSwoole extension, when installed
php -d xdebug.mode=off -d zend.assertions=-1 bench/protocol/openswoole.php [iterations]
php -d xdebug.mode=off -d zend.assertions=-1 bench/server-runtime/openswoole.php [connections] [rounds]
php -d xdebug.mode=off -d zend.assertions=-1 bench/message-runtime/openswoole.php [connections] [messages] [rounds] [ws|wss|both] [payload-bytes]
```

With Homebrew PHP 8.3:

```bash
/opt/homebrew/opt/php@8.3/bin/phpize
./configure --enable-websocket --with-php-config=/opt/homebrew/opt/php@8.3/bin/php-config
make -j"$(sysctl -n hw.ncpu)"

/opt/homebrew/opt/php@8.3/bin/php \
  -d zend.assertions=-1 \
  -d extension="$PWD/modules/websocket.so" \
  bench/protocol/websocket.php
```

Default: 100,000 protocol iterations, 1,000 server-runtime connections over 3 rounds, or 50 message-runtime connections with 1,000 messages, 3 rounds, `ws`, and 64B payloads.

## What is measured

### Protocol

| Benchmark | What it tests |
|---|---|
| `encode text 64B` | Server-side text frame encoding for a small payload |
| `decode masked text 64B` | Masked client text frame decoding for a small payload |
| `encode text 1024B` | Server-side text frame encoding for a larger payload |
| `decode masked text 1024B` | Masked client text frame decoding for a larger payload |

### Server Runtime

| Benchmark | What it tests |
|---|---|
| `tcp accept/close` | Raw TCP accept loop and connection cleanup |
| `websocket upgrade/close` | Native `WebSocket\Server::run()` HTTP Upgrade path and connection cleanup |
| `client connect loop` | Client-side loop overhead while opening raw TCP benchmark connections |
| `client upgrade loop` | Client-side loop overhead while opening and upgrading benchmark WebSocket connections |

### Message Runtime

| Benchmark | What it tests |
|---|---|
| `idle upgraded connections` | Open and hold upgraded WebSocket connections |
| `echo pipelined messages` | Receive many outstanding text messages, dispatch `onMessage`, send replies, and read them client-side |
| `broadcast fanout deliveries` | One incoming message fanned out to all connected clients |

## File Structure

| File | Description |
|---|---|
| `protocol/common.php` | Shared RFC 6455 encode/decode benchmark logic |
| `protocol/websocket.php` | ext-websocket protocol benchmark |
| `protocol/amphp.php` | amphp/websocket-server protocol benchmark |
| `protocol/ratchet.php` | ratchet/rfc6455 protocol benchmark |
| `protocol/workerman.php` | workerman/workerman protocol benchmark |
| `protocol/openswoole.php` | OpenSwoole protocol benchmark |
| `server-runtime/common.php` | Shared TCP accept-loop benchmark runner |
| `server-runtime/websocket.php` | ext-websocket TCP accept-loop benchmark |
| `server-runtime/amphp.php` | AMPHP socket accept-loop benchmark |
| `server-runtime/workerman.php` | Workerman accept-loop benchmark |
| `server-runtime/openswoole.php` | OpenSwoole accept-loop benchmark |
| `server-runtime/servers/*.php` | Isolated server processes used by the runtime benchmarks |
| `message-runtime/common.php` | Shared WebSocket message-runtime benchmark runner |
| `message-runtime/tls_proxy.php` | Local TLS terminator used by `wss` message-runtime benchmarks |
| `message-runtime/websocket.php` | ext-websocket message-runtime benchmark |
| `message-runtime/amphp.php` | AMPHP WebSocket Server message-runtime benchmark |
| `message-runtime/workerman.php` | Workerman message-runtime benchmark |
| `message-runtime/openswoole.php` | OpenSwoole message-runtime benchmark |
| `message-runtime/servers/*.php` | Isolated server processes used by the message-runtime benchmarks |
