# ext-websocket

Native WebSocket extension for PHP.

The project is at `0.1.0`: the extension builds, registers the public PHP API, and contains the first native RFC 6455 protocol helpers. The WebSocket server runtime is not implemented yet.

The goal is to keep the expensive protocol work in C and expose a small PHP API that can be used from normal PHP code, async runtimes, and later from a Channels-compatible broadcasting server.

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

## Current API

`WebSocket\Protocol`:

| Method | Description |
|---|---|
| `acceptKey(string $key): string` | Build `Sec-WebSocket-Accept` |
| `encode(string $payload, MessageType $type = MessageType::Text, bool $masked = false): string` | Encode one complete frame |
| `decode(string $buffer): Frame\|CloseFrame\|null` | Decode one frame, or return `null` for incomplete input |
| `pack(string\|Frame\|CloseFrame $data, int $opcode = Protocol::OPCODE_TEXT, int $flags = Protocol::FLAG_FIN): string` | Encode one frame with raw opcode and flags |
| `unpack(string $buffer): Frame\|CloseFrame\|null` | Decode one frame |

`WebSocket\Frame` exposes `type`, `opcode`, `flags`, `payload`, `final`, and `bytesConsumed`.

`WebSocket\CloseFrame` exposes `code`, `reason`, `flags`, and `bytesConsumed`.

These classes are already registered, but their runtime behavior is still pending:

- `WebSocket\Server`
- `WebSocket\Connection`
- `Channels\Server`
- `Channels\App`

## Testing

```bash
make test
```

To run tests against the built module directly:

```bash
TEST_PHP_ARGS="-d extension=$PWD/modules/websocket.so" php run-tests.php -q tests
```
