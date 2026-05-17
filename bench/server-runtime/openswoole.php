<?php

declare(strict_types=1);

ini_set('memory_limit', '512M');

if (!extension_loaded('openswoole')) {
    fwrite(STDERR, "The openswoole extension is not loaded.\n");
    exit(1);
}

require __DIR__ . '/common.php';

$adapterVersion = phpversion('openswoole');

runServerAcceptBenchmark(
	'openswoole',
	$adapterVersion !== false ? $adapterVersion : 'loaded',
	__DIR__ . '/servers/openswoole.php',
);
