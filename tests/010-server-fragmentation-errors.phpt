--TEST--
WebSocket\Server rejects invalid fragmented message sequences
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
use WebSocket\Protocol;
use WebSocket\Server;

function runInvalidFragmentCase(array $frames, string $caseName): array
{
    $root = dirname(__DIR__);
    $extension = $root . '/modules/websocket.so';
    $tmpDir = sys_get_temp_dir() . '/websocket-server-fragment-error-test-' . getmypid() . '-' . $caseName;
    $eventsFile = $tmpDir . '/events.txt';
    $serverFile = $tmpDir . '/server.php';

    mkdir($tmpDir);

    $probe = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
    if (!$probe) {
        return ['client' => false, 'response' => '', 'code' => null, 'events' => [], 'stdout' => '', 'stderr' => 'cannot allocate port'];
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

$server->onOpen(static function (Connection $connection): void {
    file_put_contents(EVENTS_PLACEHOLDER, "open\n");
});

$server->onMessage(static function (Connection $connection, string $message): void {
    file_put_contents(EVENTS_PLACEHOLDER, "message\n", FILE_APPEND);
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
        return ['client' => false, 'response' => '', 'code' => null, 'events' => [], 'stdout' => '', 'stderr' => 'cannot start server'];
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

    $response = '';
    $close = null;

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
        $response = fread($client, 4096);

        foreach ($frames as $frame) {
            fwrite($client, $frame);
        }

        $buffer = '';
        $deadline = microtime(true) + 5.0;
        do {
            $chunk = fread($client, 4096);
            if (is_string($chunk) && $chunk !== '') {
                $buffer .= $chunk;
                $frame = Protocol::unpack($buffer);
                if ($frame instanceof CloseFrame) {
                    $close = $frame;
                    break;
                }
            }

            usleep(10000);
        } while (microtime(true) < $deadline);

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

    @unlink($eventsFile);
    @unlink($serverFile);
    @rmdir($tmpDir);

    return [
        'client' => $client !== false,
        'response' => $response,
        'code' => $close?->code,
        'events' => $events,
        'stdout' => $stdout,
        'stderr' => $stderr,
    ];
}

$continuationWithoutStart = runInvalidFragmentCase([
    Protocol::pack('orphan', Protocol::OPCODE_CONTINUATION, Protocol::FLAG_FIN | Protocol::FLAG_MASK),
], 'orphan');

$newDataBeforeContinuation = runInvalidFragmentCase([
    Protocol::pack('first', Protocol::OPCODE_TEXT, Protocol::FLAG_MASK),
    Protocol::pack('second', Protocol::OPCODE_BINARY, Protocol::FLAG_FIN | Protocol::FLAG_MASK),
], 'nested');

var_dump($continuationWithoutStart['client']);
var_dump(str_contains($continuationWithoutStart['response'], "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump($continuationWithoutStart['code']);
var_dump($continuationWithoutStart['events']);
var_dump($continuationWithoutStart['stdout'] === '');
var_dump($continuationWithoutStart['stderr'] === '');

var_dump($newDataBeforeContinuation['client']);
var_dump(str_contains($newDataBeforeContinuation['response'], "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump($newDataBeforeContinuation['code']);
var_dump($newDataBeforeContinuation['events']);
var_dump($newDataBeforeContinuation['stdout'] === '');
var_dump($newDataBeforeContinuation['stderr'] === '');
?>
--EXPECT--
bool(true)
bool(true)
int(1002)
array(3) {
  [0]=>
  string(4) "open"
  [1]=>
  string(5) "close"
  [2]=>
  string(8) "returned"
}
bool(true)
bool(true)
bool(true)
bool(true)
int(1002)
array(3) {
  [0]=>
  string(4) "open"
  [1]=>
  string(5) "close"
  [2]=>
  string(8) "returned"
}
bool(true)
bool(true)
