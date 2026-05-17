<?php

declare(strict_types=1);

use Amp\Socket\Socket;

use function Amp\Socket\listen;

require dirname(__DIR__, 2) . '/vendor/autoload.php';
require __DIR__ . '/bootstrap.php';

$server = listen('127.0.0.1:' . serverAcceptBenchmarkPort());
$connections = serverAcceptBenchmarkConnections();
$accepted = 0;
$startedAt = null;

$onOpen = static function (Socket $socket) use ($connections, &$accepted, &$startedAt): void {
	if ($startedAt === null) {
		$startedAt = hrtime(true);
	}

	$accepted++;
	$socket->close();

	if ($accepted >= $connections) {
		serverAcceptBenchmarkWriteResult($accepted, (hrtime(true) - $startedAt) / 1e9);
	}
};

while ($accepted < $connections) {
	$socket = $server->accept();
	if ($socket === null) {
		break;
	}

	$onOpen($socket);
}

$server->close();

serverAcceptBenchmarkWriteFallbackResult($accepted, $startedAt);
