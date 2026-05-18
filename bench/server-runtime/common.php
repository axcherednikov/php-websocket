<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

/**
 * @param list<string> $phpArgs
 * @param list<string> $scriptArgs
 */
function runServerAcceptBenchmark(
	string $adapterName,
	string $adapterVersion,
	string $serverFile,
	array $phpArgs = [],
	array $scriptArgs = [],
	bool $clientUpgrade = false,
): void
{
	if (!function_exists('proc_open')) {
		fwrite(STDERR, "proc_open() is required for the server accept benchmark.\n");
		exit(1);
	}

	if (!function_exists('stream_socket_server') || !function_exists('stream_socket_client')) {
		fwrite(STDERR, "PHP stream sockets are required for the server accept benchmark.\n");
		exit(1);
	}

	if (!is_file($serverFile)) {
		fwrite(STDERR, "Benchmark server file not found: {$serverFile}\n");
		exit(1);
	}

	$arguments = $_SERVER['argv'] ?? [];
	if (!is_array($arguments)) {
		$arguments = [];
	}

	$connections = serverAcceptConnections($arguments);
	if ($connections <= 0) {
		fwrite(STDERR, "Connections must be a positive integer.\n");
		exit(1);
	}

	$rounds = serverAcceptRounds($arguments);
	if ($rounds <= 0) {
		fwrite(STDERR, "Rounds must be a positive integer.\n");
		exit(1);
	}

	$workDir = dirname(__DIR__) . '/var/server-accept-' . serverAcceptSlug($adapterName) . '-' . getmypid();
	if (!is_dir($workDir) && !mkdir($workDir, 0777, true) && !is_dir($workDir)) {
		fwrite(STDERR, "Unable to create benchmark directory {$workDir}.\n");
		exit(1);
	}

	$accepted = 0;
	$connected = 0;
	$serverElapsed = 0.0;
	$clientElapsed = 0.0;

	try {
		for ($round = 1; $round <= $rounds; $round++) {
			$result = serverAcceptRunOnce($workDir, $round, $connections, $serverFile, $phpArgs, $scriptArgs, $clientUpgrade);
			$accepted = $result['accepted'];
			$connected = $result['connected'];
			$serverElapsed += $result['serverElapsed'];
			$clientElapsed += $result['clientElapsed'];
		}
	} finally {
		serverAcceptRemoveWorkDir($workDir);
	}

	$serverElapsed /= $rounds;
	$clientElapsed /= $rounds;

	$serverBenchmark = $clientUpgrade ? 'websocket upgrade/close' : 'tcp accept/close';
	$clientBenchmark = $clientUpgrade ? 'client upgrade loop' : 'client connect loop';

	printf("Library: %s %s\n", $adapterName, $adapterVersion);
	printf("Connections: %d\n", $connections);
	printf("Rounds: %d\n\n", $rounds);
	printf(
		"%s: %d connections avg in %.4fs (%.0f connections/sec)\n",
		$serverBenchmark,
		$accepted,
		$serverElapsed,
		$accepted / max($serverElapsed, 0.000001)
	);
	printf(
		"%s: %d connections avg in %.4fs (%.0f connections/sec)\n",
		$clientBenchmark,
		$connected,
		$clientElapsed,
		$connected / max($clientElapsed, 0.000001)
	);
}

/**
 * @param array<array-key, mixed> $arguments
 */
function serverAcceptConnections(array $arguments): int
{
	if (!isset($arguments[1])) {
		return 1000;
	}

	if (!is_numeric($arguments[1])) {
		return 0;
	}

	return (int) $arguments[1];
}

/**
 * @param array<array-key, mixed> $arguments
 */
function serverAcceptRounds(array $arguments): int
{
	if (!isset($arguments[2])) {
		return 3;
	}

	if (!is_numeric($arguments[2])) {
		return 0;
	}

	return (int) $arguments[2];
}

/**
 * @param list<string> $phpArgs
 * @param list<string> $scriptArgs
 * @return array{accepted: int, connected: int, serverElapsed: float, clientElapsed: float}
 */
function serverAcceptRunOnce(
	string $workDir,
	int $round,
	int $connections,
	string $serverFile,
	array $phpArgs,
	array $scriptArgs,
	bool $clientUpgrade,
): array {
	$roundDir = $workDir . '/round-' . $round;
	if (!is_dir($roundDir) && !mkdir($roundDir, 0777, true) && !is_dir($roundDir)) {
		throw new RuntimeException("Unable to create benchmark round directory {$roundDir}");
	}

	$resultFile = $roundDir . '/result.json';
	$stdoutFile = $roundDir . '/stdout.log';
	$stderrFile = $roundDir . '/stderr.log';
	$port = serverAcceptAllocateTcpPort();
	$environment = [
		'WEBSOCKET_BENCH_PORT' => (string) $port,
		'WEBSOCKET_BENCH_CONNECTIONS' => (string) $connections,
		'WEBSOCKET_BENCH_RESULT_FILE' => $resultFile,
		'WEBSOCKET_BENCH_ROUND_DIR' => $roundDir,
	];

	$process = serverAcceptStartProcess($phpArgs, $scriptArgs, $serverFile, $stdoutFile, $stderrFile, $environment);
	$clientStart = hrtime(true);
	$connected = 0;

	try {
		for ($i = 0; $i < $connections; $i++) {
			serverAcceptConnectOnce($port, $i === 0 ? 5.0 : 1.0, $process, $clientUpgrade);
			$connected++;
		}

		serverAcceptWaitForResult($process, $resultFile, 10.0);
	} catch (Throwable $exception) {
		serverAcceptPrintProcessLogs($stdoutFile, $stderrFile);
		throw $exception;
	} finally {
		serverAcceptStopProcess($process);
	}

	$clientElapsed = (hrtime(true) - $clientStart) / 1e9;
	$result = serverAcceptReadResult($resultFile);

	return [
		'accepted' => $result['accepted'],
		'connected' => $connected,
		'serverElapsed' => $result['serverElapsed'],
		'clientElapsed' => $clientElapsed,
	];
}

function serverAcceptAllocateTcpPort(): int
{
	$errno = 0;
	$error = '';
	$server = stream_socket_server('tcp://127.0.0.1:0', $errno, $error);
	if ($server === false) {
		throw new RuntimeException("Unable to allocate TCP port: {$error}", is_int($errno) ? $errno : 0);
	}

	$name = stream_socket_get_name($server, false);
	fclose($server);

	if ($name === false) {
		throw new RuntimeException('Unable to read allocated TCP port');
	}

	$separator = strrpos($name, ':');
	if ($separator === false) {
		throw new RuntimeException("Unable to parse allocated TCP address {$name}");
	}

	return (int) substr($name, $separator + 1);
}

/**
 * @param list<string> $phpArgs
 * @param list<string> $scriptArgs
 * @param array<string, string> $environment
 * @return resource
 */
function serverAcceptStartProcess(array $phpArgs, array $scriptArgs, string $serverFile, string $stdoutFile, string $stderrFile, array $environment)
{
	$pipes = [];

	$process = proc_open(
		[PHP_BINARY, ...$phpArgs, $serverFile, ...$scriptArgs],
		[
			0 => ['file', '/dev/null', 'r'],
			1 => ['file', $stdoutFile, 'w'],
			2 => ['file', $stderrFile, 'w'],
		],
		$pipes,
		null,
		array_replace(getenv(), $environment),
	);

	if (!is_resource($process)) {
		throw new RuntimeException('Unable to start benchmark server process');
	}

	return $process;
}

/**
 * @param resource $process
 */
function serverAcceptConnectOnce(int $port, float $timeout, $process, bool $clientUpgrade): void
{
	$deadline = microtime(true) + $timeout;
	$errno = 0;
	$error = '';

	do {
		$client = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $error, 0.1);
		if ($client !== false) {
			if ($clientUpgrade) {
				$request = implode("\r\n", [
					'GET / HTTP/1.1',
					'Host: 127.0.0.1:' . $port,
					'Upgrade: websocket',
					'Connection: Upgrade',
					'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
					'Sec-WebSocket-Version: 13',
					'',
					'',
				]);

				fwrite($client, $request);
				stream_set_timeout($client, 1);
				$response = fread($client, 4096);
				if (!is_string($response) || !str_contains($response, "HTTP/1.1 101 Switching Protocols\r\n")) {
					fclose($client);
					throw new RuntimeException('Benchmark server did not complete WebSocket upgrade');
				}
			}

			fclose($client);
			return;
		}

		$status = proc_get_status($process);
		if (!$status['running']) {
			throw new RuntimeException('Benchmark server stopped before accepting all connections');
		}

		usleep(1000);
	} while (microtime(true) < $deadline);

	throw new RuntimeException("Unable to connect to benchmark server: {$error}", is_int($errno) ? $errno : 0);
}

/**
 * @param resource $process
 */
function serverAcceptWaitForResult($process, string $resultFile, float $timeout): void
{
	$deadline = microtime(true) + $timeout;

	do {
		if (is_file($resultFile)) {
			return;
		}

		usleep(10000);
	} while (microtime(true) < $deadline);

	$status = proc_get_status($process);
	if (!$status['running']) {
		throw new RuntimeException(sprintf(
			'Benchmark server stopped without writing a result file (exitcode=%d, signaled=%s, termsig=%d)',
			$status['exitcode'],
			$status['signaled'] ? 'yes' : 'no',
			$status['termsig'],
		));
	}

	throw new RuntimeException('Benchmark server did not stop before timeout');
}

function serverAcceptPrintProcessLogs(string $stdoutFile, string $stderrFile): void
{
	$stdout = is_file($stdoutFile) ? trim((string) file_get_contents($stdoutFile)) : '';
	$stderr = is_file($stderrFile) ? trim((string) file_get_contents($stderrFile)) : '';

	if ($stdout !== '') {
		fwrite(STDERR, "\n--- server stdout ---\n{$stdout}\n");
	}

	if ($stderr !== '') {
		fwrite(STDERR, "\n--- server stderr ---\n{$stderr}\n");
	}
}

/**
 * @param resource $process
 */
function serverAcceptStopProcess($process): void
{
	$status = proc_get_status($process);
	if ($status['running']) {
		proc_terminate($process);
	}

	proc_close($process);
}

/**
 * @return array{accepted: int, serverElapsed: float}
 */
function serverAcceptReadResult(string $resultFile): array
{
	if (!is_file($resultFile)) {
		throw new RuntimeException('Benchmark server did not write a result file');
	}

	$json = file_get_contents($resultFile);
	if ($json === false) {
		throw new RuntimeException('Unable to read benchmark result file');
	}

	$result = json_decode($json, true, flags: JSON_THROW_ON_ERROR);
	if (!is_array($result)) {
		throw new RuntimeException('Invalid benchmark result payload');
	}

	$accepted = $result['accepted'] ?? null;
	$serverElapsed = $result['serverElapsed'] ?? null;
	if (!is_int($accepted) || (!is_int($serverElapsed) && !is_float($serverElapsed))) {
		throw new RuntimeException('Invalid benchmark result shape');
	}

	return [
		'accepted' => $accepted,
		'serverElapsed' => (float) $serverElapsed,
	];
}

function serverAcceptSlug(string $name): string
{
	return trim((string) preg_replace('/[^a-z0-9]+/', '-', strtolower($name)), '-');
}

function serverAcceptRemoveWorkDir(string $workDir): void
{
	if (!is_dir($workDir)) {
		return;
	}

	$entries = scandir($workDir);
	if ($entries === false) {
		@rmdir($workDir);
		return;
	}

	foreach ($entries as $entry) {
		if ($entry === '.' || $entry === '..') {
			continue;
		}

		$path = $workDir . DIRECTORY_SEPARATOR . $entry;
		if (is_dir($path) && !is_link($path)) {
			serverAcceptRemoveWorkDir($path);
			continue;
		}

		@unlink($path);
	}

	@rmdir($workDir);
}
