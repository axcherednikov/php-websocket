--TEST--
WebSocket\Server supports zero-argument fast reject callbacks
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
$tmpDir = sys_get_temp_dir() . '/websocket-server-fast-reject-test-' . getmypid();
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

use WebSocket\Server;

$server = new Server();
$server->listen('127.0.0.1', PORT_PLACEHOLDER);

$accepted = 0;

$server->onOpen(static function () use ($server, &$accepted): bool {
    $accepted++;
    $server->stop();

    return false;
});

$server->run();
file_put_contents(EVENTS_PLACEHOLDER, $accepted . "\nreturned\n");
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
    fclose($client);
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
var_dump($events);
var_dump($stdout === '');
var_dump($stderr === '');

@unlink($eventsFile);
@unlink($serverFile);
@rmdir($tmpDir);
?>
--EXPECT--
bool(true)
array(2) {
  [0]=>
  string(1) "1"
  [1]=>
  string(8) "returned"
}
bool(true)
bool(true)
