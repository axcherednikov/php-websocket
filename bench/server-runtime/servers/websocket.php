<?php

declare(strict_types=1);

use WebSocket\Server;

require __DIR__ . '/bootstrap.php';

$server = new Server();
$server->listen('127.0.0.1', serverAcceptBenchmarkPort());

$connections = serverAcceptBenchmarkConnections();
$accepted = 0;
$startedAt = null;

$server->onOpen(static function () use ($server, $connections, &$accepted, &$startedAt): bool {
	if ($startedAt === null) {
		$startedAt = hrtime(true);
	}

	$accepted++;

	if ($accepted >= $connections) {
		serverAcceptBenchmarkWriteResult($accepted, (hrtime(true) - $startedAt) / 1e9);
		$server->stop();
	}

	return false;
});

$server->run();

serverAcceptBenchmarkWriteFallbackResult($accepted, $startedAt);
