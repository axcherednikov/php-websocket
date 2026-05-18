<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

function serverAcceptBenchmarkEnv(string $name): string
{
	$value = getenv($name);
	if ($value === false || $value === '') {
		fwrite(STDERR, "Missing {$name} environment variable.\n");
		exit(1);
	}

	return $value;
}

function serverAcceptBenchmarkEnvInt(string $name): int
{
	$value = serverAcceptBenchmarkEnv($name);
	if (!is_numeric($value) || (int) $value <= 0) {
		fwrite(STDERR, "{$name} must be a positive integer.\n");
		exit(1);
	}

	return (int) $value;
}

function serverAcceptBenchmarkPort(): int
{
	return serverAcceptBenchmarkEnvInt('WEBSOCKET_BENCH_PORT');
}

function serverAcceptBenchmarkConnections(): int
{
	return serverAcceptBenchmarkEnvInt('WEBSOCKET_BENCH_CONNECTIONS');
}

function serverAcceptBenchmarkResultFile(): string
{
	return serverAcceptBenchmarkEnv('WEBSOCKET_BENCH_RESULT_FILE');
}

function serverAcceptBenchmarkRoundDir(): string
{
	return serverAcceptBenchmarkEnv('WEBSOCKET_BENCH_ROUND_DIR');
}

function serverAcceptBenchmarkWriteResult(int $accepted, float $serverElapsed): void
{
	file_put_contents(serverAcceptBenchmarkResultFile(), json_encode([
		'accepted' => $accepted,
		'serverElapsed' => $serverElapsed,
	], JSON_THROW_ON_ERROR));
}

function serverAcceptBenchmarkWriteFallbackResult(int $accepted, ?int $startedAt): void
{
	$resultFile = serverAcceptBenchmarkResultFile();
	if (is_file($resultFile)) {
		return;
	}

	$elapsed = $startedAt === null ? 0.0 : (hrtime(true) - $startedAt) / 1e9;
	serverAcceptBenchmarkWriteResult($accepted, $elapsed);
}
