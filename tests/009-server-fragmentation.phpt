--TEST--
WebSocket\Server reassembles fragmented text and binary messages
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

$root = dirname(__DIR__);
$extension = $root . '/modules/websocket.so';
$tmpDir = sys_get_temp_dir() . '/websocket-server-fragmentation-test-' . getmypid();
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
use WebSocket\MessageType;
use WebSocket\Server;

$server = new Server();
$server->listen('127.0.0.1', PORT_PLACEHOLDER);

$server->onOpen(static function (Connection $connection): void {
    file_put_contents(EVENTS_PLACEHOLDER, "open\n");
});

$server->onMessage(static function (Connection $connection, string $message, MessageType $type): void {
    file_put_contents(EVENTS_PLACEHOLDER, $type->name . ':' . bin2hex($message) . "\n", FILE_APPEND);

    if ($type === MessageType::Binary) {
        $connection->send($message, MessageType::Binary);
        return;
    }

    $connection->send('text:' . $message, MessageType::Text);
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

$buffer = '';
$readFrame = static function ($client) use (&$buffer): Frame|CloseFrame|null {
    $deadline = microtime(true) + 5.0;

    do {
        if ($buffer !== '') {
            $frame = Protocol::unpack($buffer);
            if ($frame !== null) {
                $buffer = substr($buffer, $frame->bytesConsumed);
                return $frame;
            }
        }

        $chunk = fread($client, 4096);
        if (is_string($chunk) && $chunk !== '') {
            $buffer .= $chunk;
            continue;
        }

        usleep(10000);
    } while (microtime(true) < $deadline);

    return null;
};

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
    $buffer = $headerEnd === false ? '' : substr($responseBuffer, $headerEnd + 4);

    fwrite($client, Protocol::pack('hello ', Protocol::OPCODE_TEXT, Protocol::FLAG_MASK));
    fwrite($client, Protocol::pack('mid', Protocol::OPCODE_PING, Protocol::FLAG_FIN | Protocol::FLAG_MASK));
    $pong = $readFrame($client);
    fwrite($client, Protocol::pack('world', Protocol::OPCODE_CONTINUATION, Protocol::FLAG_FIN | Protocol::FLAG_MASK));
    $textEcho = $readFrame($client);

    fwrite($client, Protocol::pack("\x00\x01", Protocol::OPCODE_BINARY, Protocol::FLAG_MASK));
    fwrite($client, Protocol::pack("\x02\x03", Protocol::OPCODE_CONTINUATION, Protocol::FLAG_FIN | Protocol::FLAG_MASK));
    $binaryEcho = $readFrame($client);

    fwrite($client, Protocol::pack(pack('n', Protocol::CLOSE_NORMAL), Protocol::OPCODE_CLOSE, Protocol::FLAG_FIN | Protocol::FLAG_MASK));
    $close = $readFrame($client);

    fclose($client);
} else {
    $response = '';
    $pong = null;
    $textEcho = null;
    $binaryEcho = null;
    $close = null;
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
var_dump($pong instanceof Frame && $pong->type === MessageType::Pong && $pong->payload === 'mid');
var_dump($textEcho instanceof Frame && $textEcho->type === MessageType::Text && $textEcho->payload === 'text:hello world');
var_dump($binaryEcho instanceof Frame && $binaryEcho->type === MessageType::Binary && $binaryEcho->payload === "\x00\x01\x02\x03");
var_dump($close instanceof CloseFrame && $close->code === Protocol::CLOSE_NORMAL);
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
bool(true)
array(5) {
  [0]=>
  string(4) "open"
  [1]=>
  string(27) "Text:68656c6c6f20776f726c64"
  [2]=>
  string(15) "Binary:00010203"
  [3]=>
  string(5) "close"
  [4]=>
  string(8) "returned"
}
bool(true)
bool(true)
