--TEST--
WebSocket\Server sends heartbeat pings and closes missing pongs
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
use WebSocket\CloseFrame;
use WebSocket\Connection;
use WebSocket\Frame;
use WebSocket\MessageType;
use WebSocket\Protocol;
use WebSocket\Server;
use WebSocket\ServerOptions;

$root = dirname(__DIR__);
$extension = $root . '/modules/websocket.so';
$tmpDir = sys_get_temp_dir() . '/websocket-server-heartbeat-test-' . getmypid();
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
use WebSocket\ServerOptions;

$server = new Server(new ServerOptions(
    idleTimeoutMs: 3000,
    pingIntervalMs: 100,
    pongTimeoutMs: 250,
));
$server->listen('127.0.0.1', PORT_PLACEHOLDER);

$closed = 0;

$server->onOpen(static function (Connection $connection): void {
    file_put_contents(EVENTS_PLACEHOLDER, "open\n", FILE_APPEND);
});

$server->onClose(static function (Connection $connection) use ($server, &$closed): void {
    $closed++;
    file_put_contents(EVENTS_PLACEHOLDER, "close\n", FILE_APPEND);

    if ($closed >= 2) {
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
            stream_set_timeout($client, 1);
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

$readHeaders = static function ($client) use ($port): string {
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

    $response = '';
    $deadline = microtime(true) + 5.0;

    do {
        $chunk = fread($client, 4096);
        if (is_string($chunk) && $chunk !== '') {
            $response .= $chunk;
            if (str_contains($response, "\r\n\r\n")) {
                break;
            }
        }

        usleep(10000);
    } while (microtime(true) < $deadline);

    return $response;
};

$readFrame = static function ($client, string &$buffer): Frame|CloseFrame|null {
    $deadline = microtime(true) + 5.0;

    do {
        $frame = Protocol::unpack($buffer);
        if ($frame !== null) {
            $buffer = substr($buffer, $frame->bytesConsumed);
            return $frame;
        }

        $chunk = fread($client, 4096);
        if (is_string($chunk) && $chunk !== '') {
            $buffer .= $chunk;
        }

        usleep(10000);
    } while (microtime(true) < $deadline);

    return null;
};

$client1 = $connect();
$response1 = '';
$buffer1 = '';
$ping1 = null;
$close1 = null;

if ($client1 !== false) {
    $response1 = $readHeaders($client1);
    $headerEnd = strpos($response1, "\r\n\r\n");
    $buffer1 = $headerEnd === false ? '' : substr($response1, $headerEnd + 4);

    $ping1 = $readFrame($client1, $buffer1);

    if ($ping1 instanceof Frame) {
        fwrite($client1, Protocol::pack($ping1->payload, Protocol::OPCODE_PONG, Protocol::FLAG_FIN | Protocol::FLAG_MASK));
    }

    fwrite($client1, Protocol::pack(pack('n', Protocol::CLOSE_NORMAL), Protocol::OPCODE_CLOSE, Protocol::FLAG_FIN | Protocol::FLAG_MASK));
    $close1 = $readFrame($client1, $buffer1);
    fclose($client1);
}

$client2 = $connect();
$response2 = '';
$buffer2 = '';
$ping2 = null;
$close2 = null;

if ($client2 !== false) {
    $response2 = $readHeaders($client2);
    $headerEnd = strpos($response2, "\r\n\r\n");
    $buffer2 = $headerEnd === false ? '' : substr($response2, $headerEnd + 4);

    $ping2 = $readFrame($client2, $buffer2);
    $close2 = $readFrame($client2, $buffer2);

    fclose($client2);
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

var_dump($client1 !== false);
var_dump(str_contains($response1, "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump($ping1 instanceof Frame && $ping1->type === MessageType::Ping && $ping1->payload === '');
var_dump($close1 instanceof CloseFrame && $close1->code === Protocol::CLOSE_NORMAL);
var_dump($client2 !== false);
var_dump(str_contains($response2, "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump($ping2 instanceof Frame && $ping2->type === MessageType::Ping && $ping2->payload === '');
var_dump($close2 instanceof CloseFrame && $close2->code === Protocol::CLOSE_NORMAL && $close2->reason === 'pong timeout');
var_dump(array_count_values($events));
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
bool(true)
bool(true)
bool(true)
array(3) {
  ["open"]=>
  int(2)
  ["close"]=>
  int(2)
  ["returned"]=>
  int(1)
}
bool(true)
bool(true)
