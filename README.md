# ext-websocket

Native WebSocket extension for PHP.

The project is at `0.1.0`: the extension builds, registers the public PHP API, and contains the first native RFC 6455 protocol helpers. The WebSocket server runtime is not implemented yet.

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

`WebSocket\Server` is the future native server runtime. The class is registered now so the API can settle before the socket loop is implemented.

| Method | Description |
|---|---|
| `__construct(array $options = [])` | Create a server instance with optional runtime options |
| `listen(string $host, int $port): void` | Store the address the server should bind to |
| `onOpen(Closure $handler): void` | Register a callback for accepted WebSocket connections |
| `onMessage(Closure $handler): void` | Register a callback for received messages |
| `onClose(Closure $handler): void` | Register a callback for closed connections |
| `onError(Closure $handler): void` | Register a callback for runtime errors |
| `run(): void` | Start the server loop; currently validates the configured state |
| `stop(): void` | Request server shutdown |
| `getDriver(): string` | Return the selected native I/O driver name |

### `Connection`

`WebSocket\Connection` represents one client connection once the server runtime is implemented.

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

**Environment:** PHP 8.3.31, `zend.assertions=-1`, Apple Silicon macOS, 100,000 iterations for 64B payloads and 20,000 iterations for 1024B payloads. Results from May 14, 2026.
AMPHP WebSocket Server v4.0.0, Ratchet v0.4.0, and Workerman v5.2.0 were installed from Composer. OpenSwoole v26.2.0 was installed from PECL.

| Benchmark | amphp/websocket-server | ratchet/rfc6455 | workerman/workerman | openswoole | ext-websocket |
|---|--:|--:|--:|--:|--:|
| `encode text 64B` | 3,086,015 ops/sec | 1,516,834 ops/sec | 4,515,106 ops/sec | 7,883,119 ops/sec | **13,941,734 ops/sec** |
| `decode masked text 64B` | 606,358 ops/sec | 569,765 ops/sec | 1,657,240 ops/sec | **4,151,983 ops/sec** | 3,003,172 ops/sec |
| `encode text 1024B` | 2,508,768 ops/sec | 1,341,775 ops/sec | 3,598,605 ops/sec | 6,218,503 ops/sec | **11,459,952 ops/sec** |
| `decode masked text 1024B` | 361,919 ops/sec | 271,721 ops/sec | 817,038 ops/sec | **3,758,780 ops/sec** | 1,405,572 ops/sec |

> These benchmarks measure RFC 6455 frame encode/decode throughput. Full socket/server load tests should be added once the native server runtime accepts real connections.
>
> **[Run the benchmarks yourself ->](bench/)**

## Testing

```bash
make test
```

To run tests against the built module directly:

```bash
TEST_PHP_ARGS="-d extension=$PWD/modules/websocket.so" php run-tests.php -q tests
```
