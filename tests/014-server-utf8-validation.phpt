--TEST--
WebSocket\Server rejects invalid UTF-8 text and close reason payloads
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

$root = dirname(__DIR__);
$extension = $root . '/modules/websocket.so';

$runCase = static function (string $caseName, array $frames): array {
    global $extension;

    $tmpDir = sys_get_temp_dir() . '/websocket-server-utf8-test-' . getmypid() . '-' . $caseName;
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

$server->onOpen(static function (Connection $connection): void {
    file_put_contents(EVENTS_PLACEHOLDER, "open\n");
});

$server->onMessage(static function (Connection $connection, string $message): void {
    file_put_contents(EVENTS_PLACEHOLDER, "message:" . bin2hex($message) . "\n", FILE_APPEND);
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

    $readFrame = static function ($client): CloseFrame|null {
        $buffer = '';
        $deadline = microtime(true) + 5.0;

        do {
            $chunk = fread($client, 4096);
            if (is_string($chunk) && $chunk !== '') {
                $buffer .= $chunk;
                $frame = Protocol::unpack($buffer);
                if ($frame instanceof CloseFrame) {
                    return $frame;
                }
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
        fread($client, 4096);

        foreach ($frames as $frame) {
            fwrite($client, $frame);
        }

        $close = $readFrame($client);
        fclose($client);
    } else {
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

    @unlink($eventsFile);
    @unlink($serverFile);
    @rmdir($tmpDir);

    return [$close, $events, $stdout, $stderr];
};

$packRawMasked = static function (string $payload, int $opcode, bool $final = true): string {
    $mask = "\x12\x34\x56\x78";
    $header = chr(($final ? 0x80 : 0) | $opcode);
    $len = strlen($payload);

    if ($len < 126) {
        $header .= chr(0x80 | $len);
    } elseif ($len <= 0xffff) {
        $header .= chr(0x80 | 126) . pack('n', $len);
    } else {
        $header .= chr(0x80 | 127) . pack('NN', 0, $len);
    }

    $masked = '';
    for ($i = 0; $i < $len; $i++) {
        $masked .= $payload[$i] ^ $mask[$i & 3];
    }

    return $header . $mask . $masked;
};

$invalidText = $runCase('text', [
    $packRawMasked("\xff", Protocol::OPCODE_TEXT),
]);

$invalidFragmentedText = $runCase('fragmented', [
    $packRawMasked("\xf0\x9f", Protocol::OPCODE_TEXT, false),
    $packRawMasked("\xff", Protocol::OPCODE_CONTINUATION),
]);

$invalidCloseReason = $runCase('close', [
    $packRawMasked(pack('n', Protocol::CLOSE_NORMAL) . "\xff", Protocol::OPCODE_CLOSE),
]);

foreach ([$invalidText, $invalidFragmentedText, $invalidCloseReason] as [$close, $events, $stdout, $stderr]) {
    var_dump($close instanceof CloseFrame);
    var_dump($close?->code);
    var_dump(in_array('message:ff', $events, true));
    var_dump($events);
    var_dump($stdout === '');
    var_dump($stderr === '');
}
?>
--EXPECT--
bool(true)
int(1007)
bool(false)
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
int(1007)
bool(false)
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
int(1007)
bool(false)
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
