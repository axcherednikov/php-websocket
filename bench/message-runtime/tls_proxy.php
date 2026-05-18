<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

ini_set('memory_limit', '512M');

/**
 * @return never
 */
function tlsProxyFail(string $message): void
{
	fwrite(STDERR, $message . "\n");
	exit(1);
}

function tlsProxyEnv(string $name): string
{
	$value = getenv($name);
	if ($value === false || $value === '') {
		tlsProxyFail("Missing {$name} environment variable.");
	}

	return $value;
}

function tlsProxyEnvInt(string $name): int
{
	$value = tlsProxyEnv($name);
	if (!is_numeric($value) || (int) $value <= 0) {
		tlsProxyFail("{$name} must be a positive integer.");
	}

	return (int) $value;
}

/**
 * @param resource $stream
 */
function tlsProxyStreamId($stream): int
{
	return (int) $stream;
}

/**
 * @param resource $stream
 * @param array<int, resource> $streams
 * @param array<int, int> $peers
 * @param array<int, string> $writeBuffers
 */
function tlsProxyClose($stream, array &$streams, array &$peers, array &$writeBuffers): void
{
	$id = tlsProxyStreamId($stream);
	$peerId = $peers[$id] ?? null;

	unset($streams[$id], $peers[$id], $writeBuffers[$id]);
	@fclose($stream);

	if ($peerId !== null && isset($streams[$peerId])) {
		$peer = $streams[$peerId];
		unset($streams[$peerId], $peers[$peerId], $writeBuffers[$peerId]);
		@fclose($peer);
	}
}

/**
 * @param array<int, resource> $streams
 * @param array<int, int> $peers
 * @param array<int, string> $writeBuffers
 */
function tlsProxyFlushWritable(array &$streams, array &$peers, array &$writeBuffers): void
{
	$write = [];
	foreach ($writeBuffers as $id => $buffer) {
		if ($buffer !== '' && isset($streams[$id])) {
			$write[] = $streams[$id];
		}
	}

	if ($write === []) {
		return;
	}

	$read = null;
	$except = null;
	$ready = @stream_select($read, $write, $except, 0, 0);
	if ($ready === false || $ready <= 0) {
		return;
	}

	foreach ($write as $stream) {
		$id = tlsProxyStreamId($stream);
		$buffer = $writeBuffers[$id] ?? '';
		if ($buffer === '') {
			continue;
		}

		$written = @fwrite($stream, $buffer);
		if ($written === false || $written === 0) {
			tlsProxyClose($stream, $streams, $peers, $writeBuffers);
			continue;
		}

		$writeBuffers[$id] = substr($buffer, $written);
	}
}

$frontendPort = tlsProxyEnvInt('WEBSOCKET_BENCH_TLS_PORT');
$backendPort = tlsProxyEnvInt('WEBSOCKET_BENCH_BACKEND_PORT');
$certificateFile = tlsProxyEnv('WEBSOCKET_BENCH_CERT_FILE');
$keyFile = tlsProxyEnv('WEBSOCKET_BENCH_KEY_FILE');

$context = stream_context_create([
	'ssl' => [
		'local_cert' => $certificateFile,
		'local_pk' => $keyFile,
		'allow_self_signed' => true,
		'verify_peer' => false,
		'verify_peer_name' => false,
	],
]);

$errno = 0;
$error = '';
$server = @stream_socket_server(
	'tcp://127.0.0.1:' . $frontendPort,
	$errno,
	$error,
	STREAM_SERVER_BIND | STREAM_SERVER_LISTEN,
	$context,
);
if ($server === false) {
	tlsProxyFail("Unable to start TLS benchmark proxy: {$error}");
}

stream_set_blocking($server, false);

/** @var array<int, resource> $streams */
$streams = [];
/** @var array<int, int> $peers */
$peers = [];
/** @var array<int, string> $writeBuffers */
$writeBuffers = [];

while (getenv('WEBSOCKET_BENCH_TLS_PROXY_STOP') !== '1') {
	$read = [$server, ...array_values($streams)];
	$write = null;
	$except = null;
	$ready = @stream_select($read, $write, $except, 0, 10000);
	if ($ready === false) {
		continue;
	}

	if ($ready > 0) {
		foreach ($read as $stream) {
			if ($stream === $server) {
				$client = @stream_socket_accept($server, 0);
				if ($client === false) {
					continue;
				}
				stream_set_blocking($client, true);
				if (@stream_socket_enable_crypto($client, true, STREAM_CRYPTO_METHOD_TLS_SERVER) !== true) {
					@fclose($client);
					continue;
				}

				$backend = @stream_socket_client('tcp://127.0.0.1:' . $backendPort, $errno, $error, 1.0);
				if ($backend === false) {
					@fclose($client);
					continue;
				}

				stream_set_blocking($client, false);
				stream_set_blocking($backend, false);

				$clientId = tlsProxyStreamId($client);
				$backendId = tlsProxyStreamId($backend);
				$streams[$clientId] = $client;
				$streams[$backendId] = $backend;
				$peers[$clientId] = $backendId;
				$peers[$backendId] = $clientId;
				$writeBuffers[$clientId] = '';
				$writeBuffers[$backendId] = '';
				continue;
			}

			$id = tlsProxyStreamId($stream);
			if (!isset($streams[$id])) {
				continue;
			}

			$chunk = @fread($stream, 16384);
			if ($chunk === false || $chunk === '') {
				if (feof($stream)) {
					tlsProxyClose($stream, $streams, $peers, $writeBuffers);
				}
				continue;
			}

			$peerId = $peers[$id] ?? null;
			if ($peerId === null || !isset($streams[$peerId])) {
				tlsProxyClose($stream, $streams, $peers, $writeBuffers);
				continue;
			}

			$writeBuffers[$peerId] .= $chunk;
		}
	}

	tlsProxyFlushWritable($streams, $peers, $writeBuffers);
}
