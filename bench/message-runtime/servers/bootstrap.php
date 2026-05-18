<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

function messageRuntimeBenchmarkEnv(string $name): string
{
	$value = getenv($name);
	if ($value === false || $value === '') {
		fwrite(STDERR, "Missing {$name} environment variable.\n");
		exit(1);
	}

	return $value;
}

function messageRuntimeBenchmarkEnvInt(string $name): int
{
	$value = messageRuntimeBenchmarkEnv($name);
	if (!is_numeric($value) || (int) $value <= 0) {
		fwrite(STDERR, "{$name} must be a positive integer.\n");
		exit(1);
	}

	return (int) $value;
}

function messageRuntimeBenchmarkPort(): int
{
	return messageRuntimeBenchmarkEnvInt('WEBSOCKET_BENCH_PORT');
}

/**
 * @return int<1, 65535>
 */
function messageRuntimeBenchmarkInternetPort(): int
{
	$port = messageRuntimeBenchmarkPort();
	if ($port < 1 || $port > 65535) {
		fwrite(STDERR, "WEBSOCKET_BENCH_PORT must be between 1 and 65535.\n");
		exit(1);
	}

	return $port;
}

function messageRuntimeBenchmarkConnections(): int
{
	return messageRuntimeBenchmarkEnvInt('WEBSOCKET_BENCH_CONNECTIONS');
}

function messageRuntimeBenchmarkMessages(): int
{
	return messageRuntimeBenchmarkEnvInt('WEBSOCKET_BENCH_MESSAGES');
}

function messageRuntimeBenchmarkScenario(): string
{
	$scenario = messageRuntimeBenchmarkEnv('WEBSOCKET_BENCH_SCENARIO');
	if (!in_array($scenario, ['idle', 'echo', 'broadcast'], true)) {
		fwrite(STDERR, "Unsupported WEBSOCKET_BENCH_SCENARIO {$scenario}.\n");
		exit(1);
	}

	return $scenario;
}

function messageRuntimeBenchmarkResultFile(): string
{
	return messageRuntimeBenchmarkEnv('WEBSOCKET_BENCH_RESULT_FILE');
}

function messageRuntimeBenchmarkRoundDir(): string
{
	return messageRuntimeBenchmarkEnv('WEBSOCKET_BENCH_ROUND_DIR');
}

function messageRuntimeBenchmarkWriteResult(string $scenario, int $operations, float $serverElapsed): void
{
	$resultFile = messageRuntimeBenchmarkResultFile();
	if (is_file($resultFile) && filesize($resultFile) !== 0) {
		return;
	}

	$tmpFile = $resultFile . '.' . getmypid() . '.tmp';
	file_put_contents($tmpFile, json_encode([
		'scenario' => $scenario,
		'operations' => $operations,
		'serverElapsed' => $serverElapsed,
	], JSON_THROW_ON_ERROR));
	rename($tmpFile, $resultFile);
}

function messageRuntimeBenchmarkElapsed(int|float|null $startedAt): float
{
	return $startedAt === null ? 0.0 : (hrtime(true) - $startedAt) / 1e9;
}
