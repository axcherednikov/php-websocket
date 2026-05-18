<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

use WebSocket\Connection;
use WebSocket\Server;

require __DIR__ . '/bootstrap.php';

$scenario = messageRuntimeBenchmarkScenario();
$connectionsExpected = messageRuntimeBenchmarkConnections();
$messagesExpected = messageRuntimeBenchmarkMessages();
$connections = [];
$opened = 0;
$messages = 0;
$idleStartedAt = null;
$messageStartedAt = null;

$server = new Server(['maxQueuedBytes' => 64 * 1024 * 1024]);
$server->listen('127.0.0.1', messageRuntimeBenchmarkInternetPort());

$server->onOpen(static function (Connection $connection) use ($scenario, $connectionsExpected, &$connections, &$opened, &$idleStartedAt): void {
	if ($idleStartedAt === null) {
		$idleStartedAt = hrtime(true);
	}

	$connections[$connection->id] = $connection;
	$opened++;

	if ($scenario === 'idle' && $opened >= $connectionsExpected) {
		messageRuntimeBenchmarkWriteResult('idle', $opened, messageRuntimeBenchmarkElapsed($idleStartedAt));
	}
});

$server->onMessage(static function (Connection $connection, string $message) use ($scenario, $connectionsExpected, $messagesExpected, &$connections, &$messages, &$messageStartedAt): void {
	if ($messageStartedAt === null) {
		$messageStartedAt = hrtime(true);
	}

	if ($scenario === 'broadcast') {
		foreach ($connections as $client) {
			if ($client->isOpen()) {
				$client->send($message);
			}
		}

		$messages++;
		if ($messages >= $messagesExpected) {
			messageRuntimeBenchmarkWriteResult('broadcast', $messages * $connectionsExpected, messageRuntimeBenchmarkElapsed($messageStartedAt));
		}

		return;
	}

	$connection->send($message);
	$messages++;

	if ($scenario === 'echo' && $messages >= $messagesExpected) {
		messageRuntimeBenchmarkWriteResult('echo', $messages, messageRuntimeBenchmarkElapsed($messageStartedAt));
	}
});

$server->onClose(static function (Connection $connection) use (&$connections): void {
	unset($connections[$connection->id]);
});

$server->run();
