--TEST--
WebSocket\Server negotiates Sec-WebSocket-Protocol
--EXTENSIONS--
websocket
--SKIPIF--
<?php
if (!function_exists('proc_open')) {
    die('skip proc_open is unavailable');
}
if (!function_exists('stream_socket_server') || !function_exists('stream_socket_client')) {
    die('skip stream sockets are unavailable');
}
?>
--FILE--
<?php
$root = dirname(__DIR__);
$extension = $root . '/modules/websocket.so';
$tmpDir = sys_get_temp_dir() . '/websocket-server-subprotocol-test-' . getmypid();
$eventsFile = $tmpDir . '/events.txt';
$serverFile = $tmpDir . '/server.php';

mkdir($tmpDir);

$probe = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
if (!$probe) {
    echo "cannot allocate port\n";
    exit;
}

$name = stream_socket_get_name($probe, false);
fclose($probe);
$port = (int) substr(strrchr($name, ':'), 1);

$serverCode = <<<'PHP'
<?php

use WebSocket\Connection;
use WebSocket\Server;

$server = new Server();
$server->listen('127.0.0.1', PORT_PLACEHOLDER);
$server->subprotocols('chat.v1', 'superchat');

$opened = 0;
$server->onOpen(static function (Connection $connection) use ($server, &$opened): void {
    $opened++;
    file_put_contents(EVENTS_PLACEHOLDER, ($connection->subprotocol ?? '(none)') . "\n", FILE_APPEND);

    if ($opened >= 2) {
        $server->stop();
    }
});

$server->run();
file_put_contents(EVENTS_PLACEHOLDER, "returned\n", FILE_APPEND);
PHP;

$serverCode = str_replace(
    ['PORT_PLACEHOLDER', 'EVENTS_PLACEHOLDER'],
    [(string) $port, var_export($eventsFile, true)],
    $serverCode,
);
file_put_contents($serverFile, $serverCode);

$process = proc_open(
    [PHP_BINARY, '-n', '-d', 'extension=' . $extension, $serverFile],
    [
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ],
    $pipes,
);

if (!is_resource($process)) {
    echo "cannot start server\n";
    exit;
}

$connect = static function () use ($port, $process): mixed {
    $client = false;
    $deadline = microtime(true) + 5.0;

    do {
        $client = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.1);
        if ($client !== false) {
            return $client;
        }

        $status = proc_get_status($process);
        if (!$status['running']) {
            break;
        }

        usleep(10000);
    } while (microtime(true) < $deadline);

    return false;
};

$handshake = static function (?string $protocolHeader) use ($connect, $port): string {
    $client = $connect();
    if ($client === false) {
        return '';
    }

    $lines = [
        'GET /chat HTTP/1.1',
        'Host: 127.0.0.1:' . $port,
        'Upgrade: websocket',
        'Connection: Upgrade',
        'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
        'Sec-WebSocket-Version: 13',
    ];

    if ($protocolHeader !== null) {
        $lines[] = 'Sec-WebSocket-Protocol: ' . $protocolHeader;
    }

    $lines[] = '';
    $lines[] = '';

    fwrite($client, implode("\r\n", $lines));
    stream_set_timeout($client, 1);
    $response = fread($client, 4096);
    fclose($client);

    return $response;
};

$invalidResponse = $handshake('chat.v1, ');
$unmatchedResponse = $handshake('video');
$matchedResponse = $handshake('video, superchat, chat.v1');

$deadline = microtime(true) + 5.0;
do {
    $status = proc_get_status($process);
    if (!$status['running']) {
        break;
    }

    usleep(10000);
} while (microtime(true) < $deadline);

$status = proc_get_status($process);
if ($status['running']) {
    proc_terminate($process);
}

$stdout = stream_get_contents($pipes[1]);
$stderr = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
proc_close($process);

$events = file_exists($eventsFile) ? file($eventsFile, FILE_IGNORE_NEW_LINES) : [];

var_dump(str_contains($invalidResponse, "HTTP/1.1 400 Bad Request\r\n"));
var_dump(str_contains($unmatchedResponse, "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump(! str_contains($unmatchedResponse, "Sec-WebSocket-Protocol:"));
var_dump(str_contains($matchedResponse, "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump(str_contains($matchedResponse, "Sec-WebSocket-Protocol: superchat\r\n"));
var_dump($events);
var_dump($stdout === '');
var_dump($stderr === '');

@unlink($eventsFile);
@unlink($serverFile);
@rmdir($tmpDir);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
array(3) {
  [0]=>
  string(6) "(none)"
  [1]=>
  string(9) "superchat"
  [2]=>
  string(8) "returned"
}
bool(true)
bool(true)
