# ext-websocket

Native WebSocket extension for PHP.

`ext-websocket` keeps RFC 6455 protocol work in C and exposes a small PHP API for synchronous PHP code, async runtimes, and the native server runtime included in the extension.

Current version: `0.8.0-dev`.

## Requirements

- PHP >= 8.1
- Linux, macOS, BSD, or another POSIX-compatible OS
- `phpize`, `php-config`, make, and a C compiler

PHP 8.3+ is recommended for local development.

## Build

```bash
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

With Homebrew PHP:

```bash
/opt/homebrew/opt/php@8.3/bin/phpize
./configure --enable-websocket --with-php-config=/opt/homebrew/opt/php@8.3/bin/php-config
make -j"$(sysctl -n hw.ncpu)"
```

## Quick Start

```php
<?php

use WebSocket\Connection;
use WebSocket\MessageType;
use WebSocket\Server;

$server = new Server();
$server->listen('127.0.0.1', 8080);

$server->onOpen(static function (Connection $connection): void {
    echo "open {$connection->id}\n";
});

$server->onMessage(static function (Connection $connection, string $message): void {
    $connection->send($message, MessageType::Text);
});

$server->onClose(static function (Connection $connection): void {
    echo "close {$connection->id}\n";
});

$server->run();
```

See [examples/run_server.php](examples/run_server.php) for a runnable example.

## Protocol Helpers

```php
<?php

use WebSocket\MessageType;
use WebSocket\Protocol;

$bytes = Protocol::encode('hello', MessageType::Text);
$frame = Protocol::decode($bytes);

echo $frame->payload; // hello
```

## Public API

### `WebSocket\Server`

Options:

| Option | Description |
|---|---|
| `maxMessageSize` | Maximum incoming text/binary message size; defaults to 16 MiB |
| `maxQueuedBytes` | Maximum outgoing queued bytes per connection; defaults to 16 MiB |

Methods:

| Method | Description |
|---|---|
| `__construct(array $options = [])` | Create a server |
| `listen(string $host, int $port): void` | Bind address for `run()` |
| `onOpen(Closure $handler): void` | Register upgraded connection callback |
| `onMessage(Closure $handler): void` | Register text/binary message callback |
| `onClose(Closure $handler): void` | Register close callback |
| `onError(Closure $handler): void` | Register runtime error callback |
| `run(): void` | Start accept, HTTP Upgrade, and frame processing loop |
| `stop(): void` | Request shutdown |
| `getDriver(): string` | Return selected I/O driver |

### `WebSocket\Connection`

| Method / property | Description |
|---|---|
| `send(string $payload, MessageType $type = MessageType::Text): void` | Send text, binary, or control payload |
| `close(int $code = 1000, string $reason = ''): void` | Send close frame and close the connection |
| `isOpen(): bool` | Check connection state |
| `readonly string $id` | Connection id |
| `readonly string $remoteAddress` | Remote peer address |

### `WebSocket\Protocol`

| Method | Description |
|---|---|
| `acceptKey(string $key): string` | Build `Sec-WebSocket-Accept` |
| `encode(string $payload, MessageType $type = MessageType::Text, bool $masked = false): string` | Encode one frame |
| `decode(string $buffer): Frame\|CloseFrame\|null` | Decode one frame |
| `pack(string\|Frame\|CloseFrame $data, int $opcode = Protocol::OPCODE_TEXT, int $flags = Protocol::FLAG_FIN): string` | Encode with raw opcode and flags |
| `unpack(string $buffer): Frame\|CloseFrame\|null` | Decode with raw opcode and flags |

### Value Objects

| Class | Description |
|---|---|
| `Frame` | Non-close WebSocket frame with `type`, `opcode`, `flags`, `payload`, `final`, and `bytesConsumed` |
| `CloseFrame` | Close frame with `code`, `reason`, `flags`, and `bytesConsumed` |
| `MessageType` | Enum cases: `Continuation`, `Text`, `Binary`, `Ping`, `Pong`, `Close` |

## Benchmarks

Detailed benchmark results and commands live in [bench/README.md](bench/README.md).

The current benchmark suite covers protocol encode/decode, server accept/upgrade runtime, and real `ws://` / `wss://` message runtime against AMPHP, Workerman, and OpenSwoole.

## Testing

```bash
make test
```

To run tests against the built module directly:

```bash
TEST_PHP_ARGS="-d extension=$PWD/modules/websocket.so" php run-tests.php -q tests
```

## License

MIT. See [LICENSE](LICENSE).
