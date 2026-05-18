<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

use WebSocket\Connection;
use WebSocket\MessageType;
use WebSocket\Protocol;
use WebSocket\Server;

if (!extension_loaded('websocket')) {
    fwrite(STDERR, "The websocket extension is not loaded.\n");
    fwrite(STDERR, "Run with: php -d extension=modules/websocket.so examples/run_server.php\n");
    exit(1);
}

$host = $argv[1] ?? '127.0.0.1';
$port = isset($argv[2]) ? (int) $argv[2] : 8080;

if ($port < 1 || $port > 65535) {
    fwrite(STDERR, "Port must be between 1 and 65535.\n");
    exit(1);
}

$server = new Server([
    'maxMessageSize' => 1024 * 1024,
]);

$server->listen($host, $port);

$server->onOpen(static function (Connection $connection): void {
    echo "open {$connection->id} from {$connection->remoteAddress}\n";

    $connection->send('Welcome to ext-websocket', MessageType::Text);
});

$server->onMessage(static function (Connection $connection, string $message) use ($server): void {
    echo "message {$connection->id}: {$message}\n";

    if ($message === 'quit') {
        $connection->close(Protocol::CLOSE_NORMAL, 'bye');
        return;
    }

    if ($message === 'shutdown') {
        $connection->send('Server is stopping', MessageType::Text);
        $server->stop();
        return;
    }

    $connection->send('echo: ' . $message, MessageType::Text);
});

$server->onClose(static function (Connection $connection): void {
    echo "close {$connection->id}\n";
});

$server->onError(static function (Throwable $error): void {
    fwrite(STDERR, "error: {$error->getMessage()}\n");
});

echo "listening on ws://{$host}:{$port}\n";
$server->run();
echo "server stopped\n";
