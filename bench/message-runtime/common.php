<?php

declare(strict_types=1);

/**
 * @param list<string> $phpArgs
 * @param list<string> $scriptArgs
 */
function runMessageRuntimeBenchmark(
	string $adapterName,
	string $adapterVersion,
	string $serverFile,
	array $phpArgs = [],
	array $scriptArgs = [],
): void {
	if (!function_exists('proc_open')) {
		fwrite(STDERR, "proc_open() is required for the message runtime benchmark.\n");
		exit(1);
	}

	if (!function_exists('stream_socket_server') || !function_exists('stream_socket_client')) {
		fwrite(STDERR, "PHP stream sockets are required for the message runtime benchmark.\n");
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

	$connections = messageRuntimeArgumentInt($arguments, 1, 50);
	$messages = messageRuntimeArgumentInt($arguments, 2, 1000);
	$rounds = messageRuntimeArgumentInt($arguments, 3, 3);
	$transport = messageRuntimeArgumentString($arguments, 4, 'ws');
	$payloadBytes = messageRuntimeArgumentInt($arguments, 5, 64);
	if ($connections <= 0 || $messages <= 0 || $rounds <= 0 || $payloadBytes <= 0) {
		fwrite(STDERR, "Connections, messages, rounds, and payload bytes must be positive integers.\n");
		exit(1);
	}

	$transports = messageRuntimeTransports($transport);
	if ($transports === []) {
		fwrite(STDERR, "Transport must be ws, wss, or both.\n");
		exit(1);
	}

	$workDir = dirname(__DIR__) . '/var/message-runtime-' . messageRuntimeSlug($adapterName) . '-' . getmypid();
	if (!is_dir($workDir) && !mkdir($workDir, 0777, true) && !is_dir($workDir)) {
		fwrite(STDERR, "Unable to create benchmark directory {$workDir}.\n");
		exit(1);
	}

	try {
		$results = [];
		foreach ($transports as $currentTransport) {
			$results[$currentTransport] = [
				'idle' => messageRuntimeAverageScenario($workDir, $currentTransport, 'idle', $rounds, $connections, $messages, $payloadBytes, $serverFile, $phpArgs, $scriptArgs),
				'echo' => messageRuntimeAverageScenario($workDir, $currentTransport, 'echo', $rounds, $connections, $messages, $payloadBytes, $serverFile, $phpArgs, $scriptArgs),
				'broadcast' => messageRuntimeAverageScenario($workDir, $currentTransport, 'broadcast', $rounds, $connections, $messages, $payloadBytes, $serverFile, $phpArgs, $scriptArgs),
			];
		}
	} finally {
		messageRuntimeRemoveWorkDir($workDir);
	}

	printf("Library: %s %s\n", $adapterName, $adapterVersion);
	printf("Connections: %d\n", $connections);
	printf("Messages: %d\n", $messages);
	printf("Payload: %d bytes\n", $payloadBytes);
	printf("Transport: %s\n", implode(',', $transports));
	printf("Rounds: %d\n\n", $rounds);

	foreach ($transports as $currentTransport) {
		if (count($transports) > 1) {
			printf("[%s]\n", $currentTransport);
		}

		if (!isset($results[$currentTransport])) {
			throw new RuntimeException("Missing benchmark results for {$currentTransport}");
		}

		$transportResults = $results[$currentTransport];
		messageRuntimePrintMetric('idle upgraded connections', $transportResults['idle'], 'connections');
		messageRuntimePrintMetric('echo pipelined messages', $transportResults['echo'], 'messages');
		messageRuntimePrintMetric('broadcast fanout deliveries', $transportResults['broadcast'], 'deliveries');

		if (count($transports) > 1) {
			echo "\n";
		}
	}
}

/**
 * @param array<array-key, mixed> $arguments
 */
function messageRuntimeArgumentInt(array $arguments, int $index, int $default): int
{
	if (!isset($arguments[$index])) {
		return $default;
	}

	if (!is_numeric($arguments[$index])) {
		return 0;
	}

	return (int) $arguments[$index];
}

/**
 * @param array<array-key, mixed> $arguments
 */
function messageRuntimeArgumentString(array $arguments, int $index, string $default): string
{
	if (!isset($arguments[$index])) {
		return $default;
	}

	if (!is_scalar($arguments[$index])) {
		return '';
	}

	return strtolower((string) $arguments[$index]);
}

/**
 * @return list<'ws'|'wss'>
 */
function messageRuntimeTransports(string $transport): array
{
	return match ($transport) {
		'ws' => ['ws'],
		'wss' => ['wss'],
		'both' => ['ws', 'wss'],
		default => [],
	};
}

/**
 * @param list<string> $phpArgs
 * @param list<string> $scriptArgs
 * @return array{operations: int, serverElapsed: float, clientElapsed: float}
 */
function messageRuntimeAverageScenario(
	string $workDir,
	string $transport,
	string $scenario,
	int $rounds,
	int $connections,
	int $messages,
	int $payloadBytes,
	string $serverFile,
	array $phpArgs,
	array $scriptArgs,
): array {
	$operations = 0;
	$serverElapsed = 0.0;
	$clientElapsed = 0.0;

	for ($round = 1; $round <= $rounds; $round++) {
		$result = messageRuntimeRunOnce($workDir, $transport, $scenario, $round, $connections, $messages, $payloadBytes, $serverFile, $phpArgs, $scriptArgs);
		$operations = $result['operations'];
		$serverElapsed += $result['serverElapsed'];
		$clientElapsed += $result['clientElapsed'];
	}

	return [
		'operations' => $operations,
		'serverElapsed' => $serverElapsed / $rounds,
		'clientElapsed' => $clientElapsed / $rounds,
	];
}

/**
 * @param list<string> $phpArgs
 * @param list<string> $scriptArgs
 * @return array{operations: int, serverElapsed: float, clientElapsed: float}
 */
function messageRuntimeRunOnce(
	string $workDir,
	string $transport,
	string $scenario,
	int $round,
	int $connections,
	int $messages,
	int $payloadBytes,
	string $serverFile,
	array $phpArgs,
	array $scriptArgs,
): array {
	$roundDir = $workDir . '/' . $transport . '-' . $scenario . '-round-' . $round;
	if (!is_dir($roundDir) && !mkdir($roundDir, 0777, true) && !is_dir($roundDir)) {
		throw new RuntimeException("Unable to create benchmark round directory {$roundDir}");
	}

	$resultFile = $roundDir . '/result.json';
	$stdoutFile = $roundDir . '/stdout.log';
	$stderrFile = $roundDir . '/stderr.log';
	$backendPort = messageRuntimeAllocateTcpPort();
	$clientPort = $backendPort;
	$environment = [
		'WEBSOCKET_BENCH_PORT' => (string) $backendPort,
		'WEBSOCKET_BENCH_CONNECTIONS' => (string) $connections,
		'WEBSOCKET_BENCH_MESSAGES' => (string) $messages,
		'WEBSOCKET_BENCH_PAYLOAD_BYTES' => (string) $payloadBytes,
		'WEBSOCKET_BENCH_SCENARIO' => $scenario,
		'WEBSOCKET_BENCH_RESULT_FILE' => $resultFile,
		'WEBSOCKET_BENCH_ROUND_DIR' => $roundDir,
	];

	$process = messageRuntimeStartProcess($phpArgs, $scriptArgs, $serverFile, $stdoutFile, $stderrFile, $environment);
	$proxyProcess = null;
	$clients = [];

	try {
		if ($transport === 'wss') {
			$clientPort = messageRuntimeAllocateTcpPort();
			$proxyProcess = messageRuntimeStartTlsProxy($roundDir, $backendPort, $clientPort, $stdoutFile . '.tls', $stderrFile . '.tls');
		}

		$clients = messageRuntimeConnectClients($clientPort, $connections, $process, $transport);
		$clientStart = hrtime(true);

		$operations = match ($scenario) {
			'idle' => messageRuntimeRunIdle($clients),
			'echo' => messageRuntimeRunEcho($clients, $messages, $payloadBytes),
			'broadcast' => messageRuntimeRunBroadcast($clients, $messages, $payloadBytes),
			default => throw new RuntimeException("Unsupported message runtime scenario {$scenario}"),
		};

		$clientElapsed = (hrtime(true) - $clientStart) / 1e9;
		messageRuntimeWaitForResult($process, $resultFile, 15.0);
		$result = messageRuntimeReadResult($resultFile, $scenario);

		return [
			'operations' => $result['operations'] > 0 ? $result['operations'] : $operations,
			'serverElapsed' => $result['serverElapsed'],
			'clientElapsed' => $clientElapsed,
		];
	} catch (Throwable $exception) {
		messageRuntimePrintProcessLogs($stdoutFile, $stderrFile);
		if ($proxyProcess !== null) {
			messageRuntimePrintProcessLogs($stdoutFile . '.tls', $stderrFile . '.tls');
		}
		throw $exception;
	} finally {
		messageRuntimeCloseClients($clients);
		if ($proxyProcess !== null) {
			messageRuntimeStopProcess($proxyProcess);
		}
		messageRuntimeStopProcess($process);
	}
}

function messageRuntimeAllocateTcpPort(): int
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
 * @return resource
 */
function messageRuntimeStartTlsProxy(string $roundDir, int $backendPort, int $frontendPort, string $stdoutFile, string $stderrFile)
{
	[$certificateFile, $keyFile] = messageRuntimeCreateTlsCertificate($roundDir);

	return messageRuntimeStartProcess(
		['-d', 'xdebug.mode=off'],
		[],
		__DIR__ . '/tls_proxy.php',
		$stdoutFile,
		$stderrFile,
		[
			'WEBSOCKET_BENCH_TLS_PORT' => (string) $frontendPort,
			'WEBSOCKET_BENCH_BACKEND_PORT' => (string) $backendPort,
			'WEBSOCKET_BENCH_CERT_FILE' => $certificateFile,
			'WEBSOCKET_BENCH_KEY_FILE' => $keyFile,
		],
	);
}

/**
 * @return array{0: string, 1: string}
 */
function messageRuntimeCreateTlsCertificate(string $roundDir): array
{
	if (!function_exists('openssl_pkey_new') || !function_exists('openssl_csr_new') || !function_exists('openssl_csr_sign')) {
		throw new RuntimeException('The openssl extension is required for the wss message runtime benchmark.');
	}

	$key = openssl_pkey_new([
		'private_key_bits' => 2048,
		'private_key_type' => OPENSSL_KEYTYPE_RSA,
	]);
	if ($key === false) {
		throw new RuntimeException('Unable to create benchmark TLS key.');
	}

	$csr = openssl_csr_new([
		'commonName' => '127.0.0.1',
	], $key, [
		'digest_alg' => 'sha256',
	]);
	if ($csr === false || $csr === true) {
		throw new RuntimeException('Unable to create benchmark TLS CSR.');
	}

	$certificate = openssl_csr_sign($csr, null, $key, 1, [
		'digest_alg' => 'sha256',
	]);
	if ($certificate === false) {
		throw new RuntimeException('Unable to sign benchmark TLS certificate.');
	}

	$certificatePem = '';
	$keyPem = '';
	if (!openssl_x509_export($certificate, $certificatePem) || !openssl_pkey_export($key, $keyPem)) {
		throw new RuntimeException('Unable to export benchmark TLS certificate.');
	}

	$certificateFile = $roundDir . '/tls.crt';
	$keyFile = $roundDir . '/tls.key';
	if (file_put_contents($certificateFile, $certificatePem) === false || file_put_contents($keyFile, $keyPem) === false) {
		throw new RuntimeException('Unable to write benchmark TLS certificate.');
	}

	return [$certificateFile, $keyFile];
}

/**
 * @param list<string> $phpArgs
 * @param list<string> $scriptArgs
 * @param array<string, string> $environment
 * @return resource
 */
function messageRuntimeStartProcess(array $phpArgs, array $scriptArgs, string $serverFile, string $stdoutFile, string $stderrFile, array $environment)
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
 * @return list<array{stream: resource, buffer: string}>
 */
function messageRuntimeConnectClients(int $port, int $connections, $process, string $transport): array
{
	$clients = [];

	for ($i = 0; $i < $connections; $i++) {
		$clients[] = [
			'stream' => messageRuntimeConnectOnce($port, $i === 0 ? 5.0 : 1.0, $process, $transport),
			'buffer' => '',
		];
	}

	return $clients;
}

/**
 * @param resource $process
 * @return resource
 */
function messageRuntimeConnectOnce(int $port, float $timeout, $process, string $transport)
{
	$deadline = microtime(true) + $timeout;
	$errno = 0;
	$error = '';

	do {
		$context = null;
		$socketUrl = 'tcp://127.0.0.1:' . $port;
		if ($transport === 'wss') {
			$socketUrl = 'tls://127.0.0.1:' . $port;
			$context = stream_context_create([
				'ssl' => [
					'allow_self_signed' => true,
					'verify_peer' => false,
					'verify_peer_name' => false,
				],
			]);
		}

		$client = @stream_socket_client($socketUrl, $errno, $error, 0.1, STREAM_CLIENT_CONNECT, $context);
		if ($client !== false) {
			stream_set_timeout($client, 2);
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
			messageRuntimeReadHandshake($client);

			return $client;
		}

		$status = proc_get_status($process);
		if (!$status['running']) {
			throw new RuntimeException('Benchmark server stopped before accepting all message runtime connections');
		}

		usleep(1000);
	} while (microtime(true) < $deadline);

	throw new RuntimeException("Unable to connect to benchmark server: {$error}", is_int($errno) ? $errno : 0);
}

/**
 * @param resource $client
 */
function messageRuntimeReadHandshake($client): void
{
	$buffer = '';
	$deadline = microtime(true) + 5.0;

	do {
		$chunk = fread($client, 4096);
		if (is_string($chunk) && $chunk !== '') {
			$buffer .= $chunk;
			if (str_contains($buffer, "\r\n\r\n")) {
				if (!str_starts_with($buffer, 'HTTP/1.1 101')) {
					throw new RuntimeException('Benchmark server did not complete WebSocket upgrade');
				}

				return;
			}
		}

		usleep(1000);
	} while (microtime(true) < $deadline);

	throw new RuntimeException('Timed out waiting for WebSocket upgrade response');
}

/**
 * @param list<array{stream: resource, buffer: string}> $clients
 */
function messageRuntimeRunIdle(array $clients): int
{
	usleep(100000);

	return count($clients);
}

/**
 * @param list<array{stream: resource, buffer: string}> $clients
 */
function messageRuntimeRunEcho(array &$clients, int $messages, int $payloadBytes): int
{
	return messageRuntimeRunPipelinedMessages($clients, $messages, $payloadBytes, 1);
}

/**
 * @param list<array{stream: resource, buffer: string}> $clients
 */
function messageRuntimeRunBroadcast(array &$clients, int $messages, int $payloadBytes): int
{
	return messageRuntimeRunPipelinedMessages($clients, $messages, $payloadBytes, count($clients));
}

/**
 * @param list<array{stream: resource, buffer: string}> $clients
 */
function messageRuntimeRunPipelinedMessages(array &$clients, int $messages, int $payloadBytes, int $deliveriesPerMessage): int
{
	$count = count($clients);
	$payload = messageRuntimePayload($payloadBytes);
	$frame = messageRuntimeClientFrame($payload);
	$expectedDeliveries = $messages * $deliveriesPerMessage;
	$maxOutstandingDeliveries = max($deliveriesPerMessage, min($expectedDeliveries, $deliveriesPerMessage * 64));
	$sentMessages = 0;
	$receivedDeliveries = 0;
	$writeBuffers = array_fill(0, $count, '');
	$streamIndexes = [];
	$deadline = microtime(true) + max(15.0, $expectedDeliveries / 1000.0);

	for ($i = 0; $i < $count; $i++) {
		stream_set_blocking($clients[$i]['stream'], false);
		$streamIndexes[messageRuntimeStreamId($clients[$i]['stream'])] = $i;
	}

	while ($receivedDeliveries < $expectedDeliveries) {
		while ($sentMessages < $messages && ($sentMessages * $deliveriesPerMessage) - $receivedDeliveries < $maxOutstandingDeliveries) {
			$sender = $sentMessages % $count;
			$writeBuffers[$sender] .= $frame;
			$sentMessages++;
		}

		$readStreams = [];
		foreach ($clients as $client) {
			$readStreams[] = $client['stream'];
		}

		$writeStreams = [];
		foreach ($writeBuffers as $index => $buffer) {
			if ($buffer !== '') {
				$writeStreams[] = $clients[$index]['stream'];
			}
		}

		$readReady = $readStreams;
		$writeReady = $writeStreams !== [] ? $writeStreams : null;
		$exceptReady = null;
		$ready = @stream_select($readReady, $writeReady, $exceptReady, 0, 10000);
		if ($ready === false) {
			throw new RuntimeException('Unable to wait for benchmark client sockets');
		}

		if ($ready === 0) {
			if (microtime(true) > $deadline) {
				throw new RuntimeException('Timed out waiting for pipelined WebSocket benchmark frames');
			}
			continue;
		}

		if ($writeReady !== null) {
			foreach ($writeReady as $stream) {
				$index = $streamIndexes[messageRuntimeStreamId($stream)] ?? null;
				if ($index === null || $writeBuffers[$index] === '') {
					continue;
				}

				$written = @fwrite($stream, $writeBuffers[$index]);
				if ($written === false) {
					throw new RuntimeException('Unable to write WebSocket benchmark frame');
				}

				if ($written > 0) {
					$writeBuffers[$index] = substr($writeBuffers[$index], $written);
				}
			}
		}

		foreach ($readReady as $stream) {
			$index = $streamIndexes[messageRuntimeStreamId($stream)] ?? null;
			if ($index === null) {
				continue;
			}

			$chunk = @fread($stream, 65536);
			if ($chunk === false) {
				throw new RuntimeException('Unable to read WebSocket benchmark frame');
			}

			if ($chunk === '') {
				if (feof($stream)) {
					throw new RuntimeException('Benchmark connection closed while reading pipelined frames');
				}
				continue;
			}

			$buffer = $clients[$index]['buffer'] . $chunk;
			while (($response = messageRuntimeTryReadFrame($buffer)) !== null) {
				if ($response !== $payload) {
					throw new RuntimeException('Unexpected pipelined benchmark payload');
				}

				$receivedDeliveries++;
				if ($receivedDeliveries > $expectedDeliveries) {
					throw new RuntimeException('Benchmark received more frames than expected');
				}
			}

			$clients[$index] = [
				'stream' => $clients[$index]['stream'],
				'buffer' => $buffer,
			];
		}
	}

	return $expectedDeliveries;
}

/**
 * @param resource $stream
 */
function messageRuntimeStreamId($stream): int
{
	return (int) $stream;
}

function messageRuntimeClientFrame(string $payload): string
{
	$length = strlen($payload);
	$mask = "\x12\x34\x56\x78";
	$header = "\x81";

	if ($length < 126) {
		$header .= chr(0x80 | $length);
	} elseif ($length <= 0xffff) {
		$header .= chr(0x80 | 126) . pack('n', $length);
	} else {
		$header .= chr(0x80 | 127) . pack('NN', 0, $length);
	}

	return $header . $mask . messageRuntimeMaskPayload($payload, $mask);
}

function messageRuntimeMaskPayload(string $payload, string $mask): string
{
	$masked = '';
	$length = strlen($payload);

	for ($i = 0; $i < $length; $i++) {
		$masked .= $payload[$i] ^ $mask[$i % 4];
	}

	return $masked;
}

/**
 * @param resource $client
 */
function messageRuntimeReadFrame($client, string &$buffer): string
{
	$deadline = microtime(true) + 5.0;

	do {
		$payload = messageRuntimeTryReadFrame($buffer);
		if ($payload !== null) {
			return $payload;
		}

		$chunk = fread($client, 4096);
		if (is_string($chunk) && $chunk !== '') {
			$buffer .= $chunk;
			continue;
		}

		usleep(1000);
	} while (microtime(true) < $deadline);

	throw new RuntimeException('Timed out waiting for WebSocket benchmark frame');
}

function messageRuntimeTryReadFrame(string &$buffer): ?string
{
	$length = strlen($buffer);
	if ($length < 2) {
		return null;
	}

	$pos = 2;
	$opcode = ord($buffer[0]) & 0x0f;
	$masked = (ord($buffer[1]) & 0x80) !== 0;
	$payloadLength = ord($buffer[1]) & 0x7f;

	if ($payloadLength === 126) {
		if ($length < 4) {
			return null;
		}
		$unpacked = unpack('nlength', substr($buffer, 2, 2));
		if ($unpacked === false) {
			throw new RuntimeException('Unable to parse WebSocket benchmark frame length');
		}
		$payloadLength = $unpacked['length'];
		$pos = 4;
	} elseif ($payloadLength === 127) {
		if ($length < 10) {
			return null;
		}
		$parts = unpack('Nhigh/Nlow', substr($buffer, 2, 8));
		if ($parts === false) {
			throw new RuntimeException('Unable to parse WebSocket benchmark frame length');
		}
		$payloadLength = ((int) $parts['high'] << 32) | (int) $parts['low'];
		$pos = 10;
	}

	$mask = '';
	if ($masked) {
		if ($length < $pos + 4) {
			return null;
		}
		$mask = substr($buffer, $pos, 4);
		$pos += 4;
	}

	if ($length < $pos + $payloadLength) {
		return null;
	}

	$payload = substr($buffer, $pos, $payloadLength);
	$buffer = substr($buffer, $pos + $payloadLength);

	if ($opcode === 0x8) {
		throw new RuntimeException('Benchmark connection closed by server');
	}

	if ($masked) {
		$payload = messageRuntimeMaskPayload($payload, $mask);
	}

	return $payload;
}

function messageRuntimePayload(int $size): string
{
	return substr(str_repeat('abcdefghijklmnopqrstuvwxyz0123456789', intdiv($size, 36) + 1), 0, $size);
}

/**
 * @param resource $process
 */
function messageRuntimeWaitForResult($process, string $resultFile, float $timeout): void
{
	$deadline = microtime(true) + $timeout;

	do {
		if (is_file($resultFile) && filesize($resultFile) !== 0) {
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

	throw new RuntimeException('Benchmark server did not write result before timeout');
}

/**
 * @return array{operations: int, serverElapsed: float}
 */
function messageRuntimeReadResult(string $resultFile, string $expectedScenario): array
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

	$scenario = $result['scenario'] ?? null;
	$operations = $result['operations'] ?? null;
	$serverElapsed = $result['serverElapsed'] ?? null;
	if ($scenario !== $expectedScenario || !is_int($operations) || (!is_int($serverElapsed) && !is_float($serverElapsed))) {
		throw new RuntimeException('Invalid benchmark result shape');
	}

	return [
		'operations' => $operations,
		'serverElapsed' => (float) $serverElapsed,
	];
}

function messageRuntimePrintProcessLogs(string $stdoutFile, string $stderrFile): void
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
 * @param list<array{stream: resource, buffer: string}> $clients
 */
function messageRuntimeCloseClients(array $clients): void
{
	foreach ($clients as $client) {
		fclose($client['stream']);
	}
}

/**
 * @param resource $process
 */
function messageRuntimeStopProcess($process): void
{
	$status = proc_get_status($process);
	if ($status['running']) {
		proc_terminate($process);
	}

	proc_close($process);
}

/**
 * @param array{operations: int, serverElapsed: float, clientElapsed: float} $result
 */
function messageRuntimePrintMetric(string $name, array $result, string $unit): void
{
	printf(
		"%s: %d %s avg in %.4fs server / %.4fs client (%.0f %s/sec server, %.0f %s/sec client)\n",
		$name,
		$result['operations'],
		$unit,
		$result['serverElapsed'],
		$result['clientElapsed'],
		$result['operations'] / max($result['serverElapsed'], 0.000001),
		$unit,
		$result['operations'] / max($result['clientElapsed'], 0.000001),
		$unit,
	);
}

function messageRuntimeSlug(string $name): string
{
	return trim((string) preg_replace('/[^a-z0-9]+/', '-', strtolower($name)), '-');
}

function messageRuntimeRemoveWorkDir(string $workDir): void
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
			messageRuntimeRemoveWorkDir($path);
			continue;
		}

		@unlink($path);
	}

	@rmdir($workDir);
}
