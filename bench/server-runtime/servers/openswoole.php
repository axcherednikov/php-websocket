<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

use OpenSwoole\Server;

require __DIR__ . '/bootstrap.php';

$connections = serverAcceptBenchmarkConnections();

$server = new Server('127.0.0.1', serverAcceptBenchmarkPort(), Server::SIMPLE_MODE);
$server->set([
	'worker_num' => 1,
	'log_file' => '/dev/null',
]);

$accepted = 0;
$startedAt = null;

$server->on('Receive', static function (Server $server, int $fd, int $reactorId, string $data): void {
});

$server->on('Connect', static function (Server $server, int $fd) use ($connections, &$accepted, &$startedAt): void {
	if ($startedAt === null) {
		$startedAt = hrtime(true);
	}

	$accepted++;
	$server->close($fd);

	if ($accepted >= $connections) {
		serverAcceptBenchmarkWriteResult($accepted, (hrtime(true) - $startedAt) / 1e9);
		$server->shutdown();
	}
});

$server->start();
