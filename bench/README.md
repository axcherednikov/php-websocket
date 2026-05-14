# Benchmarks

These benchmarks compare the current native protocol helpers with PHP WebSocket libraries that expose RFC 6455 frame encode/decode paths.
The AMPHP entry point installs `amphp/websocket-server` and measures the RFC 6455 parser/compiler it uses through `amphp/websocket`.
The OpenSwoole entry point measures `OpenSwoole\WebSocket\Server::pack()` and `OpenSwoole\WebSocket\Server::unpack()`.
OpenSwoole is optional because it is a PHP extension installed outside Composer.

The native server runtime is not implemented yet, so this suite intentionally measures protocol hot paths:

- server-side text frame encoding
- masked client text frame decoding

Full socket/server load tests should be added once `WebSocket\Server::run()` accepts real connections.

## Results

**Environment:** PHP 8.3.31, `zend.assertions=-1`, Apple Silicon macOS, 100,000 iterations for 64B payloads and 20,000 iterations for 1024B payloads. Results from May 14, 2026.
AMPHP WebSocket Server v4.0.0, Ratchet v0.4.0, and Workerman v5.2.0 were installed from Composer. OpenSwoole v26.2.0 was installed from PECL.

| Benchmark | amphp/websocket-server | ratchet/rfc6455 | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|--:|
| `encode text 64B` | 3,091,927 ops/sec | 1,495,896 ops/sec | 2,740,039 ops/sec | 7,613,850 ops/sec | **13,863,296 ops/sec** |
| `decode masked text 64B` | 605,068 ops/sec | 568,839 ops/sec | 1,639,640 ops/sec | 4,171,098 ops/sec | **7,080,043 ops/sec** |
| `encode text 1024B` | 2,495,412 ops/sec | 1,327,041 ops/sec | 3,558,508 ops/sec | 6,403,416 ops/sec | **11,334,123 ops/sec** |
| `decode masked text 1024B` | 362,809 ops/sec | 270,385 ops/sec | 818,987 ops/sec | 3,658,342 ops/sec | **5,000,625 ops/sec** |

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
php -d zend.assertions=-1 -d extension="$PWD/modules/websocket.so" bench/bench_websocket.php [iterations]

# PHP libraries
php -d zend.assertions=-1 bench/bench_amphp.php [iterations]
php -d zend.assertions=-1 bench/bench_ratchet.php [iterations]
php -d zend.assertions=-1 bench/bench_workerman.php [iterations]

# Native OpenSwoole extension, when installed
php -d zend.assertions=-1 bench/bench_openswoole.php [iterations]
```

With Homebrew PHP 8.3:

```bash
/opt/homebrew/opt/php@8.3/bin/phpize
./configure --enable-websocket --with-php-config=/opt/homebrew/opt/php@8.3/bin/php-config
make -j"$(sysctl -n hw.ncpu)"

/opt/homebrew/opt/php@8.3/bin/php \
  -d zend.assertions=-1 \
  -d extension="$PWD/modules/websocket.so" \
  bench/bench_websocket.php
```

Default: 100,000 iterations.

## What is measured

| Benchmark | What it tests |
|---|---|
| `encode text 64B` | Server-side text frame encoding for a small payload |
| `decode masked text 64B` | Masked client text frame decoding for a small payload |
| `encode text 1024B` | Server-side text frame encoding for a larger payload |
| `decode masked text 1024B` | Masked client text frame decoding for a larger payload |

## File Structure

| File | Description |
|---|---|
| `bench.php` | Shared benchmark logic |
| `bench_websocket.php` | ext-websocket entry point |
| `bench_amphp.php` | amphp/websocket-server entry point |
| `bench_ratchet.php` | ratchet/rfc6455 entry point |
| `bench_workerman.php` | workerman/workerman entry point |
| `bench_openswoole.php` | OpenSwoole WebSocket Server entry point |
