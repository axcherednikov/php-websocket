<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

use Workerman\Connection\TcpConnection;
use Workerman\Worker;

require dirname(__DIR__, 2) . '/vendor/autoload.php';
require __DIR__ . '/bootstrap.php';

$scenario = messageRuntimeBenchmarkScenario();
$connectionsExpected = messageRuntimeBenchmarkConnections();
$messagesExpected = messageRuntimeBenchmarkMessages();
$roundDir = messageRuntimeBenchmarkRoundDir();
$connections = [];
$opened = 0;
$messages = 0;
$idleStartedAt = null;
$messageStartedAt = null;

Worker::$daemonize = false;
Worker::$pidFile = $roundDir . '/workerman.pid';
Worker::$logFile = $roundDir . '/workerman.log';
Worker::$stdoutFile = '/dev/null';

$worker = new Worker('websocket://127.0.0.1:' . messageRuntimeBenchmarkInternetPort());
$worker->count = 1;

$worker->onWebSocketConnected = static function (TcpConnection $connection) use ($scenario, $connectionsExpected, &$connections, &$opened, &$idleStartedAt): void {
	if ($idleStartedAt === null) {
		$idleStartedAt = hrtime(true);
	}

	$connections[$connection->id] = $connection;
	$opened++;

	if ($scenario === 'idle' && $opened >= $connectionsExpected) {
		messageRuntimeBenchmarkWriteResult('idle', $opened, messageRuntimeBenchmarkElapsed($idleStartedAt));
	}
};

$worker->onMessage = static function (TcpConnection $connection, string $message) use ($scenario, $connectionsExpected, $messagesExpected, &$connections, &$messages, &$messageStartedAt): void {
	if ($messageStartedAt === null) {
		$messageStartedAt = hrtime(true);
	}

	if ($scenario === 'broadcast') {
		foreach ($connections as $client) {
			$client->send($message);
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
};

$worker->onClose = static function (TcpConnection $connection) use (&$connections): void {
	unset($connections[$connection->id]);
};

Worker::runAll();
