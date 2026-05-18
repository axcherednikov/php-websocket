# Benchmarks

These benchmarks compare the current native protocol helpers with PHP WebSocket libraries that expose RFC 6455 frame encode/decode paths.
The AMPHP entry point installs `amphp/websocket-server` and measures the RFC 6455 parser/compiler it uses through `amphp/websocket`.
The OpenSwoole entry point measures `OpenSwoole\WebSocket\Server::pack()` and `OpenSwoole\WebSocket\Server::unpack()`.
OpenSwoole is optional because it is a PHP extension installed outside Composer.

The protocol suite measures hot paths:

- server-side text frame encoding
- masked client text frame decoding

The server runtime suite currently measures raw TCP accept loops and the native HTTP Upgrade close path. It does not measure message throughput or broadcast yet.

## Results

### Protocol

**Environment:** PHP 8.3.31, `zend.assertions=-1`, Apple Silicon macOS, 100,000 iterations for 64B payloads and 20,000 iterations for 1024B payloads. Results from May 14, 2026.
AMPHP WebSocket Server v4.0.0, Ratchet v0.4.0, and Workerman v5.2.0 were installed from Composer. OpenSwoole v26.2.0 was installed from PECL.

| Benchmark | amphp/websocket-server | ratchet/rfc6455 | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|--:|
| `encode text 64B` | 3,091,927 ops/sec | 1,495,896 ops/sec | 2,740,039 ops/sec | 7,613,850 ops/sec | **13,863,296 ops/sec** |
| `decode masked text 64B` | 605,068 ops/sec | 568,839 ops/sec | 1,639,640 ops/sec | 4,171,098 ops/sec | **7,080,043 ops/sec** |
| `encode text 1024B` | 2,495,412 ops/sec | 1,327,041 ops/sec | 3,558,508 ops/sec | 6,403,416 ops/sec | **11,334,123 ops/sec** |
| `decode masked text 1024B` | 362,809 ops/sec | 270,385 ops/sec | 818,987 ops/sec | 3,658,342 ops/sec | **5,000,625 ops/sec** |

### Server Runtime

**Environment:** Apple Silicon macOS, 1,000 connections, `zend.assertions=-1`. Raw TCP reference rows used PHP 8.3.31 and average of 3 runs. The ext-websocket HTTP Upgrade row used PHP 8.4.21 and one control run after handshake support landed. Results from May 17, 2026. ext-websocket used the native `kqueue` driver.

| Benchmark | amphp/socket | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|
| `tcp accept/close` | 19,924 connections/sec | **21,365 connections/sec** | 15,750 connections/sec | n/a |
| `client connect loop` | **10,239 connections/sec** | 10,218 connections/sec | 4,949 connections/sec | n/a |
| `websocket upgrade/close` | n/a | n/a | n/a | 10,159 connections/sec |
| `client upgrade loop` | n/a | n/a | n/a | 5,968 connections/sec |

> This starts a fresh server process, then measures the current server runtime surface up to the last accepted connection. The ext-websocket entry performs a real HTTP Upgrade before `onOpen(): false` closes the connection; the older TCP-only comparison rows are kept as raw accept-loop references. It does not include application message throughput yet. `ratchet/rfc6455` is not listed here because the benchmarked package exposes protocol helpers, not a TCP server runtime.

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
php -d zend.assertions=-1 -d extension="$PWD/modules/websocket.so" bench/protocol/websocket.php [iterations]
php -d zend.assertions=-1 -d extension="$PWD/modules/websocket.so" bench/server-runtime/websocket.php [connections] [rounds]

# PHP libraries
php -d zend.assertions=-1 bench/protocol/amphp.php [iterations]
php -d zend.assertions=-1 bench/server-runtime/amphp.php [connections] [rounds]
php -d zend.assertions=-1 bench/protocol/ratchet.php [iterations]
php -d zend.assertions=-1 bench/protocol/workerman.php [iterations]
php -d zend.assertions=-1 bench/server-runtime/workerman.php [connections] [rounds]

# Native OpenSwoole extension, when installed
php -d zend.assertions=-1 bench/protocol/openswoole.php [iterations]
php -d zend.assertions=-1 bench/server-runtime/openswoole.php [connections] [rounds]
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

Default: 100,000 protocol iterations or 1,000 server connections over 3 server-runtime rounds.

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
