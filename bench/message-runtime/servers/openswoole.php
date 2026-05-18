<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

use OpenSwoole\WebSocket\Frame;
use OpenSwoole\WebSocket\Server;
use OpenSwoole\Http\Request;

require __DIR__ . '/bootstrap.php';

$scenario = messageRuntimeBenchmarkScenario();
$connectionsExpected = messageRuntimeBenchmarkConnections();
$messagesExpected = messageRuntimeBenchmarkMessages();
$connections = [];
$opened = 0;
$messages = 0;
$idleStartedAt = null;
$messageStartedAt = null;

$server = new Server('127.0.0.1', messageRuntimeBenchmarkInternetPort(), Server::SIMPLE_MODE);
$server->set([
	'worker_num' => 1,
	'log_file' => '/dev/null',
]);

$server->on('Open', static function (Server $server, Request $request) use ($scenario, $connectionsExpected, &$connections, &$opened, &$idleStartedAt): void {
	if ($idleStartedAt === null) {
		$idleStartedAt = hrtime(true);
	}

	$connections[$request->fd] = $request->fd;
	$opened++;

	if ($scenario === 'idle' && $opened >= $connectionsExpected) {
		messageRuntimeBenchmarkWriteResult('idle', $opened, messageRuntimeBenchmarkElapsed($idleStartedAt));
	}
});

$server->on('Message', static function (Server $server, Frame $frame) use ($scenario, $connectionsExpected, $messagesExpected, &$connections, &$messages, &$messageStartedAt): void {
	if ($messageStartedAt === null) {
		$messageStartedAt = hrtime(true);
	}

	if ($scenario === 'broadcast') {
		foreach ($connections as $fd) {
			$server->push($fd, $frame->data);
		}

		$messages++;
		if ($messages >= $messagesExpected) {
			messageRuntimeBenchmarkWriteResult('broadcast', $messages * $connectionsExpected, messageRuntimeBenchmarkElapsed($messageStartedAt));
		}

		return;
	}

	$server->push($frame->fd, $frame->data);
	$messages++;

	if ($scenario === 'echo' && $messages >= $messagesExpected) {
		messageRuntimeBenchmarkWriteResult('echo', $messages, messageRuntimeBenchmarkElapsed($messageStartedAt));
	}
});

$server->on('Close', static function (Server $server, int $fd) use (&$connections): void {
	unset($connections[$fd]);
});

$server->start();
