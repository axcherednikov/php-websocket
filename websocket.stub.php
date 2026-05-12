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

    public function send(Connection $connection, string $payload, MessageType $type = MessageType::Text): void {}

    public function close(Connection $connection, int $code = 1000, string $reason = ''): void {}

    public function run(): void {}

    public function stop(): void {}

    public function getDriver(): string {}
}

final class Connection
{
    public readonly string $id;

    public readonly string $remoteAddress;

    public function send(string $payload): void {}

    public function close(int $code = 1000, string $reason = ''): void {}

    public function isOpen(): bool {}
}

enum MessageType
{
    case Text;
    case Binary;
    case Ping;
    case Pong;
    case Close;
}

namespace Channels;

final class Server
{
    public function __construct(array $apps, array $options = []) {}

    public function listen(string $host, int $port): void {}

    public function onConnection(\Closure $handler): void {}

    public function onSubscribe(\Closure $handler): void {}

    public function onUnsubscribe(\Closure $handler): void {}

    public function onClientEvent(\Closure $handler): void {}

    public function trigger(string|array $channels, string $event, mixed $data, array $params = [], bool $alreadyEncoded = false): object {}

    public function triggerBatch(array $batch = [], bool $alreadyEncoded = false): object {}

    public function getChannelInfo(string $channel, array $params = []): object {}

    public function getChannels(array $params = []): object {}

    public function getPresenceUsers(string $channel): object {}

    public function terminateUserConnections(string $userId): object {}

    public function run(): void {}

    public function stop(): void {}
}

final class App
{
    public function __construct(string $key, string $secret, string $id, array $options = []) {}
}
