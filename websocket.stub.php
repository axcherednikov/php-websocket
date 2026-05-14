<?php

namespace WebSocket;

final class Server
{
    public function __construct(array $options = []) {}

    public function listen(string $host, int $port): void {}

    public function onOpen(\Closure $handler): void {}

    public function onMessage(\Closure $handler): void {}

    public function onClose(\Closure $handler): void {}

    public function onError(\Closure $handler): void {}

    public function run(): void {}

    public function stop(): void {}

    public function getDriver(): string {}
}

final class Connection
{
    public readonly string $id;

    public readonly string $remoteAddress;

    public function send(string $payload, MessageType $type = MessageType::Text): void {}

    public function close(int $code = 1000, string $reason = ''): void {}

    public function isOpen(): bool {}
}

enum MessageType
{
    case Continuation;
    case Text;
    case Binary;
    case Ping;
    case Pong;
    case Close;
}

final class Frame
{
    public readonly MessageType $type;

    public readonly int $opcode;

    public readonly int $flags;

    public readonly string $payload;

    public readonly bool $final;

    public readonly int $bytesConsumed;

    public function __construct(MessageType $type, string $payload, bool $final = true) {}
}

final class CloseFrame
{
    public readonly int $code;

    public readonly string $reason;

    public readonly int $flags;

    public readonly int $bytesConsumed;

    public function __construct(int $code = Protocol::CLOSE_NORMAL, string $reason = '') {}
}

final class Protocol
{
    public const int OPCODE_CONTINUATION = 0x0;
    public const int OPCODE_TEXT = 0x1;
    public const int OPCODE_BINARY = 0x2;
    public const int OPCODE_CLOSE = 0x8;
    public const int OPCODE_PING = 0x9;
    public const int OPCODE_PONG = 0xA;

    public const int FLAG_FIN = 1 << 0;
    public const int FLAG_COMPRESS = 1 << 1;
    public const int FLAG_RSV1 = 1 << 2;
    public const int FLAG_RSV2 = 1 << 3;
    public const int FLAG_RSV3 = 1 << 4;
    public const int FLAG_MASK = 1 << 5;

    public const int CLOSE_NORMAL = 1000;
    public const int CLOSE_GOING_AWAY = 1001;
    public const int CLOSE_PROTOCOL_ERROR = 1002;
    public const int CLOSE_UNSUPPORTED_DATA = 1003;
    public const int CLOSE_NO_STATUS = 1005;
    public const int CLOSE_ABNORMAL = 1006;
    public const int CLOSE_INVALID_PAYLOAD = 1007;
    public const int CLOSE_POLICY_VIOLATION = 1008;
    public const int CLOSE_MESSAGE_TOO_BIG = 1009;
    public const int CLOSE_EXTENSION_MISSING = 1010;
    public const int CLOSE_SERVER_ERROR = 1011;
    public const int CLOSE_TLS = 1015;

    public static function acceptKey(string $key): string {}

    public static function encode(string $payload, MessageType $type = MessageType::Text, bool $masked = false): string {}

    public static function decode(string $buffer): Frame|CloseFrame|null {}

    public static function pack(string|Frame|CloseFrame $data, int $opcode = self::OPCODE_TEXT, int $flags = self::FLAG_FIN): string {}

    public static function unpack(string $buffer): Frame|CloseFrame|null {}
}
