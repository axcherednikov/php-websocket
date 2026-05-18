<?php

namespace OpenSwoole\WebSocket;

class Frame
{
    public int $fd = 0;

    public string $data = '';
}

class Server
{
    public const SIMPLE_MODE = 1;
    public const WEBSOCKET_OPCODE_TEXT = 1;
    public const WEBSOCKET_FLAG_FIN = 1;

    public function __construct(string $host, int $port, int $mode = self::SIMPLE_MODE) {}

    /**
     * @param array<string, mixed> $settings
     */
    public function set(array $settings): void {}

    public function on(string $event, callable $callback): void {}

    public function push(int $fd, string $data): bool {}

    public function start(): bool {}

    public static function pack(string $data, int $opcode = self::WEBSOCKET_OPCODE_TEXT, int $flags = self::WEBSOCKET_FLAG_FIN): string {}

    public static function unpack(string $data): Frame|false {}
}

namespace OpenSwoole\Http;

class Request
{
    public int $fd = 0;
}

namespace Swoole\WebSocket;

class Frame extends \OpenSwoole\WebSocket\Frame
{
}

class Server extends \OpenSwoole\WebSocket\Server
{
}
