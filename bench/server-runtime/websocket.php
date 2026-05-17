<?php

declare(strict_types=1);

ini_set('memory_limit', '512M');

if (!extension_loaded('websocket')) {
    fwrite(STDERR, "The websocket extension is not loaded.\n");
    fwrite(STDERR, "Run with: php -d extension=\"\$PWD/modules/websocket.so\" bench/server-runtime/websocket.php\n");
    exit(1);
}

$extension = dirname(__DIR__, 2) . '/modules/websocket.so';
if (!is_file($extension)) {
    fwrite(STDERR, "Built extension not found at {$extension}.\n");
    exit(1);
}

require __DIR__ . '/common.php';

$adapterVersion = phpversion('websocket');

runServerAcceptBenchmark(
	'ext-websocket',
	$adapterVersion !== false ? $adapterVersion : 'loaded',
	__DIR__ . '/servers/websocket.php',
	['-n', '-d', 'extension=' . $extension],
);
