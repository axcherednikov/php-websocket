<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

ini_set('memory_limit', '512M');

require dirname(__DIR__) . '/vendor/autoload.php';
require __DIR__ . '/common.php';

$adapterVersion = Composer\InstalledVersions::getPrettyVersion('amphp/websocket-server') ?? 'installed';

runMessageRuntimeBenchmark(
	'amphp/websocket-server',
	$adapterVersion,
	__DIR__ . '/servers/amphp.php',
	['-n'],
);
