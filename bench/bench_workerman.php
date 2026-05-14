<?php

declare(strict_types=1);

use Workerman\Connection\TcpConnection;
use Workerman\Events\EventInterface;
use Workerman\Protocols\Websocket;

ini_set('memory_limit', '512M');

require __DIR__ . '/vendor/autoload.php';

$connection = makeWorkermanConnection();
$adapterName = 'workerman/workerman';
$adapterVersion = Composer\InstalledVersions::getPrettyVersion('workerman/workerman') ?? 'installed';
$encode = static fn (string $payload): string => Websocket::encode($payload, $connection);
$decode = static function (string $frame) use ($connection): string {
    $context = workermanContext($connection);
    $context->websocketCurrentFrameLength = 0;
    $context->websocketDataBuffer = '';

    return Websocket::decode($frame, $connection);
};

require __DIR__ . '/bench.php';
runBenchmarkSuite($adapterName, $adapterVersion, $encode, $decode);

function makeWorkermanConnection(): TcpConnection
{
    $loop = new class implements EventInterface {
        /**
         * @param callable(mixed...): void $func
         * @param array<array-key, mixed> $args
         */
        public function delay(float $delay, callable $func, array $args = []): int { return 0; }
        public function offDelay(int $timerId): bool { return true; }
        /**
         * @param callable(mixed...): void $func
         * @param array<array-key, mixed> $args
         */
        public function repeat(float $interval, callable $func, array $args = []): int { return 0; }
        public function offRepeat(int $timerId): bool { return true; }
        /**
         * @param resource $stream
         * @param callable(resource): void $func
         */
        public function onReadable($stream, callable $func): void {}
        /**
         * @param resource $stream
         */
        public function offReadable($stream): bool { return true; }
        /**
         * @param resource $stream
         * @param callable(resource): void $func
         */
        public function onWritable($stream, callable $func): void {}
        /**
         * @param resource $stream
         */
        public function offWritable($stream): bool { return true; }
        /**
         * @param callable(int): void $func
         */
        public function onSignal(int $signal, callable $func): void {}
        public function offSignal(int $signal): bool { return true; }
        public function deleteAllTimer(): void {}
        public function run(): void {}
        public function stop(): void {}
        public function getTimerCount(): int { return 0; }
        /**
         * @param callable(\Throwable): void $errorHandler
         */
        public function setErrorHandler(callable $errorHandler): void {}
    };

    $stream = fopen('php://temp', 'r+');
    if ($stream === false) {
        throw new RuntimeException('Unable to open temporary stream for Workerman benchmark');
    }

    $connection = new TcpConnection($loop, $stream);
    $context = workermanContext($connection);
    $context->websocketHandshake = true;
    $context->websocketCurrentFrameLength = 0;
    $context->websocketDataBuffer = '';
    $connection->websocketType = Websocket::BINARY_TYPE_BLOB;

    return $connection;
}

function workermanContext(TcpConnection $connection): \stdClass
{
    if (!$connection->context instanceof \stdClass) {
        $connection->context = new \stdClass();
    }

    return $connection->context;
}
