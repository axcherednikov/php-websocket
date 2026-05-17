# ext-websocket

Native WebSocket extension for PHP.

The project is at `0.3.0-dev`: the extension builds, registers the public PHP API, contains native RFC 6455 protocol helpers, and has the first TCP server runtime. WebSocket upgrade and message processing are still in progress.

The goal is to keep the WebSocket protocol work in C and expose a small PHP API that can be used from normal PHP code, async runtimes, and the native server runtime that will live in this extension.

## Requirements

- PHP >= 8.1
- Linux, macOS, BSD, or another POSIX-compatible OS
- `phpize`, `php-config`, make, and a C compiler

PHP 8.3 is the recommended version for local development.

## Build

```bash
git clone https://github.com/axcherednikov/php-websocket.git
cd php-websocket

phpize
./configure --enable-websocket
make
make test
sudo make install
```

Enable the extension:

```ini
extension=websocket
```

Check that PHP can load it:

```bash
php -m | grep websocket
```

With Homebrew PHP 8.3, the build usually looks like this:

```bash
phpize8.3
./configure --enable-websocket --with-php-config="$(command -v php-config8.3)"
make -j"$(sysctl -n hw.ncpu)"
```

## Protocol Helpers

```php
<?php

use WebSocket\MessageType;
use WebSocket\Protocol;

$bytes = Protocol::encode('hello', MessageType::Text);
$frame = Protocol::decode($bytes);

echo $frame->payload; // hello
```

For lower-level code, `pack()` and `unpack()` expose opcode, flags, and close frames:

```php
<?php

use WebSocket\CloseFrame;
use WebSocket\Frame;
use WebSocket\MessageType;
use WebSocket\Protocol;

$frame = new Frame(MessageType::Text, 'hel', final: false);
$bytes = Protocol::pack($frame);

$close = new CloseFrame(Protocol::CLOSE_MESSAGE_TOO_BIG, 'too big');
$decoded = Protocol::unpack(Protocol::pack($close));

echo $decoded->code; // 1009
```

## API Reference

The public API lives in the `WebSocket` namespace.

### `Server`

`WebSocket\Server` is the native server runtime. It currently accepts TCP connections and exposes the lifecycle callbacks; WebSocket upgrade and message processing are still in progress.

| Method | Description |
|---|---|
| `__construct(array $options = [])` | Create a server instance with optional runtime options |
| `listen(string $host, int $port): void` | Store the address the server should bind to |
| `onOpen(Closure $handler): void` | Register a callback for accepted WebSocket connections; returning `false` closes the connection immediately |
| `onMessage(Closure $handler): void` | Register a callback for received messages |
| `onClose(Closure $handler): void` | Register a callback for closed connections |
| `onError(Closure $handler): void` | Register a callback for runtime errors |
| `run(): void` | Start the native TCP accept loop |
| `stop(): void` | Request server shutdown |
| `getDriver(): string` | Return the selected native I/O driver name |

### `Connection`

`WebSocket\Connection` represents one accepted client connection.

| Method | Description |
|---|---|
| `send(string $payload, MessageType $type = MessageType::Text): void` | Send a message to the connection |
| `close(int $code = 1000, string $reason = ''): void` | Close the connection with a WebSocket close code and reason |
| `isOpen(): bool` | Check whether the connection is open |

Read-only properties:

| Property | Description |
|---|---|
| `id` | Connection id |
| `remoteAddress` | Remote peer address |

### `Protocol`

`WebSocket\Protocol` contains stateless RFC 6455 helpers.

| Method | Description |
|---|---|
| `acceptKey(string $key): string` | Build `Sec-WebSocket-Accept` |
| `encode(string $payload, MessageType $type = MessageType::Text, bool $masked = false): string` | Encode one complete frame |
| `decode(string $buffer): Frame\|CloseFrame\|null` | Decode one frame, or return `null` for incomplete input |
| `pack(string\|Frame\|CloseFrame $data, int $opcode = Protocol::OPCODE_TEXT, int $flags = Protocol::FLAG_FIN): string` | Encode one frame with raw opcode and flags |
| `unpack(string $buffer): Frame\|CloseFrame\|null` | Decode one frame |

### `Frame`

`WebSocket\Frame` is a decoded or user-created non-close WebSocket frame.

| Method | Description |
|---|---|
| `__construct(MessageType $type, string $payload, bool $final = true)` | Create a frame from message type, payload, and FIN state |

Read-only properties:

| Property | Description |
|---|---|
| `type` | High-level message type |
| `opcode` | Raw WebSocket opcode |
| `flags` | Raw frame flags |
| `payload` | Frame payload |
| `final` | Whether the frame has the FIN bit set |
| `bytesConsumed` | Bytes consumed while decoding; `0` for manually created frames |

### `CloseFrame`

`WebSocket\CloseFrame` stores a parsed or user-created close frame.

| Method | Description |
|---|---|
| `__construct(int $code = Protocol::CLOSE_NORMAL, string $reason = '')` | Create a close frame from code and reason |

Read-only properties:

| Property | Description |
|---|---|
| `code` | WebSocket close code |
| `reason` | Close reason |
| `flags` | Raw frame flags |
| `bytesConsumed` | Bytes consumed while decoding; `0` for manually created frames |

### `MessageType`

`WebSocket\MessageType` maps PHP-friendly names to WebSocket opcodes.

| Case | Description |
|---|---|
| `Continuation` | Continuation frame |
| `Text` | Text message |
| `Binary` | Binary message |
| `Ping` | Ping control frame |
| `Pong` | Pong control frame |
| `Close` | Close control frame |

## Benchmarks

### Protocol

**Environment:** PHP 8.3.31, `zend.assertions=-1`, Apple Silicon macOS, 100,000 iterations for 64B payloads and 20,000 iterations for 1024B payloads. Results from May 14, 2026.
AMPHP WebSocket Server v4.0.0, Ratchet v0.4.0, and Workerman v5.2.0 were installed from Composer. OpenSwoole v26.2.0 was installed from PECL.

| Benchmark | amphp/websocket-server | ratchet/rfc6455 | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|--:|
| `encode text 64B` | 3,091,927 ops/sec | 1,495,896 ops/sec | 2,740,039 ops/sec | 7,613,850 ops/sec | **13,863,296 ops/sec** |
| `decode masked text 64B` | 605,068 ops/sec | 568,839 ops/sec | 1,639,640 ops/sec | 4,171,098 ops/sec | **7,080,043 ops/sec** |
| `encode text 1024B` | 2,495,412 ops/sec | 1,327,041 ops/sec | 3,558,508 ops/sec | 6,403,416 ops/sec | **11,334,123 ops/sec** |
| `decode masked text 1024B` | 362,809 ops/sec | 270,385 ops/sec | 818,987 ops/sec | 3,658,342 ops/sec | **5,000,625 ops/sec** |

Run:

```bash
php -d zend.assertions=-1 \
  -d extension="$PWD/modules/websocket.so" \
  bench/protocol/websocket.php
```

### Server Runtime

**Environment:** PHP 8.3.31, `zend.assertions=-1`, Apple Silicon macOS, 1,000 TCP connections, average of 3 runs. Results from May 17, 2026. ext-websocket used the native `kqueue` driver.

| Benchmark | amphp/socket | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|
| `tcp accept/close` | 19,924 connections/sec | **21,365 connections/sec** | 15,750 connections/sec | 19,188 connections/sec |
| `client connect loop` | **10,239 connections/sec** | 10,218 connections/sec | 4,949 connections/sec | 10,016 connections/sec |

This benchmark starts a fresh server process, then measures TCP listen/accept/close for the current server runtime surface up to the last accepted connection. It does not include WebSocket upgrade, frame parsing, or message dispatch yet. The ext-websocket row uses the native zero-argument `onOpen(): false` fast-reject path; callback-based libraries expose the accepted socket or connection object. The `ratchet/rfc6455` protocol package is omitted from this runtime table because it is not a TCP server runtime.

Run:

```bash
php -d zend.assertions=-1 \
  -d extension="$PWD/modules/websocket.so" \
  bench/server-runtime/websocket.php 1000 3

php -d zend.assertions=-1 bench/server-runtime/amphp.php 1000 3
php -d zend.assertions=-1 bench/server-runtime/workerman.php 1000 3
php -d zend.assertions=-1 bench/server-runtime/openswoole.php 1000 3
```

Full WebSocket upgrade/message benchmarks will be added once message processing lands.

**[Run all benchmarks yourself ->](bench/)**

## Testing

```bash
make test
```

To run tests against the built module directly:

```bash
TEST_PHP_ARGS="-d extension=$PWD/modules/websocket.so" php run-tests.php -q tests
```
