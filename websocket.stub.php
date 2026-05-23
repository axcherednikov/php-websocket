<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */

/**
 * @generate-class-entries
 */

namespace WebSocket;

/**
 * Native WebSocket server runtime.
 */
final class Server
{
    /**
     * Create a server instance.
     *
     * Supported options:
     * - maxMessageSize: maximum accepted text/binary message size in bytes.
     * - maxQueuedBytes: maximum queued outgoing bytes per connection.
     * - maxConnections: maximum concurrently accepted TCP connections.
     * - handshakeTimeoutMs: maximum idle time before HTTP Upgrade completes.
     * - idleTimeoutMs: maximum idle time after HTTP Upgrade completes.
     *
     * @param ServerOptions|array{maxMessageSize?: int, maxQueuedBytes?: int, maxConnections?: int, handshakeTimeoutMs?: int, idleTimeoutMs?: int} $options
     */
    public function __construct(ServerOptions|array $options = []) {}

    /**
     * Bind the TCP listener used by run().
     *
     * @param int $port
     *
     * @throws \ValueError If the host contains null bytes or the port is outside TCP range.
     */
    public function listen(string $host, int $port): void {}

    /**
     * Configure server-supported WebSocket subprotocols.
     *
     * The HTTP Upgrade response selects the first client-offered token that is
     * present in this list. Invalid or duplicate tokens are rejected.
     *
     * @throws \ValueError If a protocol is not a valid RFC token or appears more than once.
     * @throws \Error If called while the server is running.
     */
    public function subprotocols(string ...$protocols): void {}

    /**
     * Register a callback called after a successful HTTP Upgrade.
     *
     * Returning false from the callback closes the accepted connection.
     *
     * @param \Closure(Connection):mixed $handler
     */
    public function onOpen(\Closure $handler): void {}

    /**
     * Register a callback called for complete text and binary messages.
     *
     * @param \Closure(Connection,string,MessageType):void $handler
     */
    public function onMessage(\Closure $handler): void {}

    /**
     * Register a callback called when an upgraded connection closes.
     *
     * @param \Closure(Connection):void $handler
     */
    public function onClose(\Closure $handler): void {}

    /**
     * Register a callback for runtime errors.
     *
     * @param \Closure(\Throwable):void $handler
     */
    public function onError(\Closure $handler): void {}

    /**
     * Start the native accept, HTTP Upgrade, and frame processing loop.
     *
     * @throws \Error If the server is already running, listen() was not called, or no I/O driver is available.
     */
    public function run(): void {}

    /**
     * Request graceful shutdown of the running server loop.
     */
    public function stop(): void {}

    /**
     * Return the active or best available I/O driver name.
     *
     * @return non-empty-string
     */
    public function getDriver(): string {}
}

/**
 * Explicit configuration for WebSocket\Server.
 */
final class ServerOptions
{
    /**
     * Maximum accepted text/binary message size in bytes.
     *
     * @var int
     */
    public readonly int $maxMessageSize;

    /**
     * Maximum queued outgoing bytes per connection.
     *
     * @var int
     */
    public readonly int $maxQueuedBytes;

    /**
     * Maximum concurrently accepted TCP connections.
     *
     * @var int
     */
    public readonly int $maxConnections;

    /**
     * Maximum idle time before HTTP Upgrade completes, in milliseconds.
     *
     * @var int
     */
    public readonly int $handshakeTimeoutMs;

    /**
     * Maximum idle time after HTTP Upgrade completes, in milliseconds.
     *
     * @var int
     */
    public readonly int $idleTimeoutMs;

    /**
     * @param int $maxMessageSize
     * @param int $maxQueuedBytes
     * @param int $maxConnections
     * @param int $handshakeTimeoutMs
     * @param int $idleTimeoutMs
     *
     * @throws \ValueError If a limit is less than 1.
     */
    public function __construct(
        int $maxMessageSize = 16 * 1024 * 1024,
        int $maxQueuedBytes = 16 * 1024 * 1024,
        int $maxConnections = 10000,
        int $handshakeTimeoutMs = 10000,
        int $idleTimeoutMs = 120000,
    ) {}
}

/**
 * Runtime connection accepted by WebSocket\Server.
 */
final class Connection
{
    /**
     * Runtime connection identifier.
     *
     * @var non-empty-string
     */
    public readonly string $id;

    /**
     * Remote peer address.
     *
     * @var non-empty-string
     */
    public readonly string $remoteAddress;

    /**
     * Selected WebSocket subprotocol, or null when none was negotiated.
     */
    public readonly ?string $subprotocol;

    /**
     * Send a text, binary, ping, pong, or close frame.
     *
     * MessageType::Continuation is not accepted for userland send().
     * Control frame payloads must be at most 125 bytes.
     *
     * @throws \ValueError If the message type or payload length is invalid.
     * @throws \Error If the connection is closed, not upgraded, or the frame cannot be sent.
     */
    public function send(string $payload, MessageType $type = MessageType::Text): void {}

    /**
     * Send a close frame and close the connection.
     *
     * @param int $code
     *
     * @throws \ValueError If the close code or reason length is invalid.
     */
    public function close(int $code = 1000, string $reason = ''): void {}

    /**
     * Check whether the underlying connection is still open.
     */
    public function isOpen(): bool {}
}

/**
 * WebSocket message and frame type.
 */
enum MessageType
{
    /** Continuation frame. */
    case Continuation;

    /** UTF-8 text message. */
    case Text;

    /** Binary message. */
    case Binary;

    /** Ping control frame. */
    case Ping;

    /** Pong control frame. */
    case Pong;

    /** Close control frame. */
    case Close;
}

/**
 * Decoded non-close WebSocket frame.
 */
final class Frame
{
    /**
     * Logical frame type derived from opcode.
     */
    public readonly MessageType $type;

    /**
     * Raw WebSocket opcode.
     */
    public readonly int $opcode;

    /**
     * Raw frame flags.
     *
     * @var int-mask-of<Protocol::FLAG_*>
     */
    public readonly int $flags;

    /**
     * Frame payload bytes.
     */
    public readonly string $payload;

    /**
     * Whether the FIN bit is set.
     */
    public readonly bool $final;

    /**
     * Number of bytes consumed by Protocol::decode() or Protocol::unpack().
     */
    public readonly int $bytesConsumed;

    /**
     * Create a frame value object for Protocol::pack().
     */
    public function __construct(MessageType $type, string $payload, bool $final = true) {}
}

/**
 * Decoded close frame.
 */
final class CloseFrame
{
    /**
     * WebSocket close status code.
     */
    public readonly int $code;

    /**
     * Close reason payload.
     */
    public readonly string $reason;

    /**
     * Raw frame flags.
     *
     * @var int-mask-of<Protocol::FLAG_*>
     */
    public readonly int $flags;

    /**
     * Number of bytes consumed by Protocol::decode() or Protocol::unpack().
     */
    public readonly int $bytesConsumed;

    /**
     * Create a close frame value object for Protocol::pack().
     *
     * @throws \ValueError If the reason is longer than 123 bytes.
     */
    public function __construct(int $code = Protocol::CLOSE_NORMAL, string $reason = '') {}
}

/**
 * RFC 6455 protocol helpers.
 */
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

    /**
     * Build Sec-WebSocket-Accept from Sec-WebSocket-Key.
     */
    public static function acceptKey(string $key): string {}

    /**
     * Encode one complete WebSocket frame from a payload and message type.
     */
    public static function encode(string $payload, MessageType $type = MessageType::Text, bool $masked = false): string {}

    /**
     * Decode one complete frame from a buffer.
     *
     * Returns null when the buffer does not yet contain a full frame.
     *
     * @throws \Error If the frame is structurally invalid.
     */
    public static function decode(string $buffer): Frame|CloseFrame|null {}

    /**
     * Encode a raw WebSocket frame.
     *
     * @param int $opcode
     * @param int $flags
     *
     * @throws \TypeError If $data is not a supported payload or frame value object.
     * @throws \ValueError If opcode, flags, or control payload length is invalid.
     */
    public static function pack(string|Frame|CloseFrame $data, int $opcode = \WebSocket\Protocol::OPCODE_TEXT, int $flags = \WebSocket\Protocol::FLAG_FIN): string {}

    /**
     * Decode one raw WebSocket frame from a buffer.
     *
     * Returns null when the buffer does not yet contain a full frame.
     *
     * @throws \Error If the frame is structurally invalid.
     */
    public static function unpack(string $buffer): Frame|CloseFrame|null {}
}
