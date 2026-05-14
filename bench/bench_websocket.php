<?php

declare(strict_types=1);

use WebSocket\MessageType;
use WebSocket\Protocol;

ini_set('memory_limit', '512M');

if (!extension_loaded('websocket')) {
    fwrite(STDERR, "The websocket extension is not loaded.\n");
    fwrite(STDERR, "Run with: php -d extension=\"\$PWD/modules/websocket.so\" bench/bench_websocket.php\n");
    exit(1);
}

$adapterName = 'ext-websocket';
$websocketVersion = phpversion('websocket');
$adapterVersion = $websocketVersion !== false ? $websocketVersion : 'loaded';
$encode = static fn (string $payload): string => Protocol::encode($payload, MessageType::Text);
$decode = static function (string $frame): string {
    $decoded = Protocol::decode($frame);
    if (!$decoded instanceof WebSocket\Frame) {
        throw new RuntimeException('The websocket extension did not decode a data frame');
    }

    return $decoded->payload;
};

require __DIR__ . '/bench.php';
runBenchmarkSuite($adapterName, $adapterVersion, $encode, $decode);
