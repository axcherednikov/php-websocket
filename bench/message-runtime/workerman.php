<?php

declare(strict_types=1);

ini_set('memory_limit', '512M');

require dirname(__DIR__) . '/vendor/autoload.php';
require __DIR__ . '/common.php';

$adapterVersion = Composer\InstalledVersions::getPrettyVersion('workerman/workerman') ?? 'installed';

runMessageRuntimeBenchmark(
	'workerman/workerman',
	$adapterVersion,
	__DIR__ . '/servers/workerman.php',
	['-n'],
	['start'],
);
