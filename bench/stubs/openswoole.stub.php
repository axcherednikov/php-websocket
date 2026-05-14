<?php

namespace OpenSwoole\WebSocket;

class Frame
{
    public string $data = '';
}

class Server
{
    public const WEBSOCKET_OPCODE_TEXT = 1;
    public const WEBSOCKET_FLAG_FIN = 1;

    public static function pack(string $data, int $opcode = self::WEBSOCKET_OPCODE_TEXT, int $flags = self::WEBSOCKET_FLAG_FIN): string {}

    public static function unpack(string $data): Frame|false {}
}

namespace Swoole\WebSocket;

class Frame extends \OpenSwoole\WebSocket\Frame
{
}

class Server extends \OpenSwoole\WebSocket\Server
{
}
