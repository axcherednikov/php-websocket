<?php

declare(strict_types=1);

use Workerman\Connection\TcpConnection;
use Workerman\Worker;

require dirname(__DIR__, 2) . '/vendor/autoload.php';
require __DIR__ . '/bootstrap.php';

$roundDir = serverAcceptBenchmarkRoundDir();
$connections = serverAcceptBenchmarkConnections();

Worker::$daemonize = false;
Worker::$pidFile = $roundDir . '/workerman.pid';
Worker::$logFile = $roundDir . '/workerman.log';
Worker::$stdoutFile = '/dev/null';

$worker = new Worker('tcp://127.0.0.1:' . serverAcceptBenchmarkPort());
$worker->count = 1;

$accepted = 0;
$startedAt = null;

$worker->onConnect = static function (TcpConnection $connection) use ($connections, &$accepted, &$startedAt): void {
	if ($startedAt === null) {
		$startedAt = hrtime(true);
	}

	$accepted++;
	$connection->close();

	if ($accepted >= $connections) {
		serverAcceptBenchmarkWriteResult($accepted, (hrtime(true) - $startedAt) / 1e9);
		Worker::stopAll();
	}
};

Worker::runAll();
