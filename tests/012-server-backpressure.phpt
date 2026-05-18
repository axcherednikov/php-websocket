--TEST--
WebSocket\Server enforces outgoing write queue limits
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
use WebSocket\Connection;
use WebSocket\Server;

$root = dirname(__DIR__);
$extension = $root . '/modules/websocket.so';
$tmpDir = sys_get_temp_dir() . '/websocket-server-backpressure-test-' . getmypid();
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

$server = new Server(['maxQueuedBytes' => 8]);
$server->listen('127.0.0.1', PORT_PLACEHOLDER);

$server->onOpen(static function (Connection $connection): void {
    file_put_contents(EVENTS_PLACEHOLDER, "open\n");

    try {
        $connection->send(str_repeat('x', 16));
    } catch (Throwable $exception) {
        file_put_contents(EVENTS_PLACEHOLDER, 'error:' . $exception->getMessage() . "\n", FILE_APPEND);
        $connection->close();
    }
});

$server->onClose(static function (Connection $connection) use ($server): void {
    file_put_contents(EVENTS_PLACEHOLDER, "close\n", FILE_APPEND);
    $server->stop();
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

$client = false;
$deadline = microtime(true) + 5.0;

do {
    $client = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.1);
    if ($client !== false) {
        break;
    }

    $status = proc_get_status($process);
    if (!$status['running']) {
        break;
    }

    usleep(10000);
} while (microtime(true) < $deadline);

if ($client !== false) {
    fwrite($client, implode("\r\n", [
        'GET / HTTP/1.1',
        'Host: 127.0.0.1:' . $port,
        'Upgrade: websocket',
        'Connection: Upgrade',
        'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
        'Sec-WebSocket-Version: 13',
        '',
        '',
    ]));

    stream_set_timeout($client, 1);
    $responseBuffer = '';
    $deadline = microtime(true) + 5.0;
    do {
        $chunk = fread($client, 4096);
        if (is_string($chunk) && $chunk !== '') {
            $responseBuffer .= $chunk;
            if (str_contains($responseBuffer, "\r\n\r\n")) {
                break;
            }
        }
        usleep(10000);
    } while (microtime(true) < $deadline);

    $headerEnd = strpos($responseBuffer, "\r\n\r\n");
    $response = $headerEnd === false ? $responseBuffer : substr($responseBuffer, 0, $headerEnd + 4);
    fclose($client);
} else {
    $response = '';
}

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

var_dump($client !== false);
var_dump(str_contains($response, "HTTP/1.1 101 Switching Protocols\r\n"));
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
array(4) {
  [0]=>
  string(4) "open"
  [1]=>
  string(36) "error:Failed to send WebSocket frame"
  [2]=>
  string(5) "close"
  [3]=>
  string(8) "returned"
}
bool(true)
bool(true)
