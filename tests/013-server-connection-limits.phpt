--TEST--
WebSocket\Server enforces connection limits and handshake timeouts
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
$tmpDir = sys_get_temp_dir() . '/websocket-server-limits-test-' . getmypid();
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

use WebSocket\Server;
use WebSocket\ServerOptions;

$server = new Server(new ServerOptions(
    maxMessageSize: 1024,
    maxQueuedBytes: 1024,
    maxConnections: 1,
    handshakeTimeoutMs: 1000,
    idleTimeoutMs: 10000,
));
$server->listen('127.0.0.1', PORT_PLACEHOLDER);
$server->run();
PHP;

$serverCode = str_replace('PORT_PLACEHOLDER', (string) $port, $serverCode);
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

$first = false;
$deadline = microtime(true) + 5.0;
do {
    $first = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.1);
    if ($first !== false) {
        break;
    }

    $status = proc_get_status($process);
    if (!$status['running']) {
        break;
    }

    usleep(10000);
} while (microtime(true) < $deadline);

if ($first !== false) {
    fwrite($first, "GET /slow HTTP/1.1\r\n");
}

usleep(200000);

$second = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.5);
$secondResponse = '';
if ($second !== false) {
    stream_set_timeout($second, 1);
    $secondResponse = fread($second, 4096);
    fclose($second);
}

if ($first !== false) {
    stream_set_timeout($first, 2);
    usleep(1200000);
    $firstResponse = fread($first, 4096);
    $firstMeta = stream_get_meta_data($first);
    fclose($first);
} else {
    $firstResponse = null;
    $firstMeta = ['eof' => false];
}

$status = proc_get_status($process);
if ($status['running']) {
    proc_terminate($process);
}

$stdout = stream_get_contents($pipes[1]);
$stderr = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
proc_close($process);

var_dump($first !== false);
var_dump($second !== false);
var_dump(str_contains($secondResponse, "HTTP/1.1 503 Service Unavailable\r\n"));
var_dump($firstResponse === '' && $firstMeta['eof']);
var_dump($stdout === '');
var_dump($stderr === '');

@unlink($serverFile);
@rmdir($tmpDir);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
