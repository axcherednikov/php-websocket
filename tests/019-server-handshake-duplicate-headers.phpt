--TEST--
WebSocket handshake validation preserves ordered duplicate header rules with and without callbacks
--EXTENSIONS--
websocket
--SKIPIF--
<?php
if (!function_exists('proc_open') || !function_exists('stream_socket_server') || !function_exists('stream_socket_client')) {
    die('skip process and stream socket functions are required');
}
?>
--FILE--
<?php
$root = dirname(__DIR__);
$extension = $root . '/modules/websocket.so';
$tmpDir = sys_get_temp_dir() . '/websocket-duplicate-headers-' . getmypid();
mkdir($tmpDir);
$probe = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
$port = (int) substr(strrchr(stream_socket_get_name($probe, false), ':'), 1);
fclose($probe);
$runMode = static function (bool $withCallback, int $port, string $tmpDir, string $extension): array {
    $eventsFile = $tmpDir . '/' . ($withCallback ? 'callback' : 'no-callback') . '.txt';
    $serverFile = $tmpDir . '/' . ($withCallback ? 'callback' : 'no-callback') . '.php';
    $callbackRegistration = $withCallback ? <<<'PHP'
$server->onHandshake(static function (WebSocket\Request $request): void {
    file_put_contents(EVENTS_PLACEHOLDER, $request->target . '|' . $request->header('sec-websocket-protocol') . '|' . $request->header('upgrade') . '|' . $request->header('connection') . '|' . $request->header('sec-websocket-version') . "\n", FILE_APPEND);
});
PHP : '';
    $serverCode = <<<'PHP'
<?php
use WebSocket\Connection;
use WebSocket\Server;
$server = new Server();
$server->listen('127.0.0.1', PORT_PLACEHOLDER);
$server->subprotocols('chat.v1', 'superchat');
CALLBACK_PLACEHOLDER
$server->onOpen(static function (Connection $connection) use ($server): void {
    file_put_contents(EVENTS_PLACEHOLDER, 'open:' . ($connection->subprotocol ?? '(none)') . "\n", FILE_APPEND);
    $server->stop();
});
$server->run();
PHP;
    $serverCode = str_replace(['PORT_PLACEHOLDER', 'CALLBACK_PLACEHOLDER', 'EVENTS_PLACEHOLDER'], [(string) $port, $callbackRegistration, var_export($eventsFile, true)], $serverCode);
    file_put_contents($serverFile, $serverCode);
    $process = proc_open([PHP_BINARY, '-n', '-d', 'extension=' . $extension, $serverFile], [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    $connect = static function () use ($port, $process) {
        $deadline = microtime(true) + 5;
        do {
            $client = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.1);
            if ($client) return $client;
            if (!proc_get_status($process)['running']) break;
            usleep(10000);
        } while (microtime(true) < $deadline);
        return false;
    };
    $handshake = static function (array $extra) use ($connect, $port): string {
        $client = $connect();
        if (!$client) return '';
        $lines = ['GET /case HTTP/1.1', 'Host: 127.0.0.1:' . $port, 'Upgrade: websocket', 'Connection: Upgrade', 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==', 'Sec-WebSocket-Version: 13'];
        array_push($lines, ...$extra);
        $lines[] = ''; $lines[] = '';
        fwrite($client, implode("\r\n", $lines));
        stream_set_timeout($client, 1);
        $response = fread($client, 4096);
        fclose($client);
        return $response;
    };
    $duplicateKey = $handshake(['Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==']);
    $badLaterToken = $handshake(['Sec-WebSocket-Protocol: video, chat.v1', 'Sec-WebSocket-Protocol: superchat, ']);
    $upgradeValidThenInvalid = $handshake(['Upgrade: websocket', 'Upgrade: h2c']);
    $connectionValidThenInvalid = $handshake(['Connection: Upgrade', 'Connection: keep-alive']);
    $versionValidThenInvalid = $handshake(['Sec-WebSocket-Version: 13', 'Sec-WebSocket-Version: 12']);
    $valid = $handshake([
        'Upgrade: h2c', 'Upgrade: websocket',
        'Connection: keep-alive', 'Connection: upgrade',
        'Sec-WebSocket-Version: 12', 'Sec-WebSocket-Version: 13',
        'Sec-WebSocket-Protocol: video, chat.v1', 'Sec-WebSocket-Protocol: superchat',
    ]);
    $deadline = microtime(true) + 5;
    do { $status = proc_get_status($process); if (!$status['running']) break; usleep(10000); } while (microtime(true) < $deadline);
    if ($status['running']) proc_terminate($process);
    $stdout = stream_get_contents($pipes[1]); $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]); fclose($pipes[2]); proc_close($process);
    $events = file_exists($eventsFile) ? file($eventsFile, FILE_IGNORE_NEW_LINES) : [];
    return [[$duplicateKey, $badLaterToken, $upgradeValidThenInvalid, $connectionValidThenInvalid, $versionValidThenInvalid, $valid], $events, $stdout, $stderr];
};
[$callbackResponses, $callbackEvents, $callbackStdout, $callbackStderr] = $runMode(true, $port, $tmpDir, $extension);
[$plainResponses, $plainEvents, $plainStdout, $plainStderr] = $runMode(false, $port, $tmpDir, $extension);
$badStatuses = static function (array $responses): bool {
    foreach (array_slice($responses, 0, 5) as $response) if (!str_contains($response, "HTTP/1.1 400 Bad Request\r\n")) return false;
    return true;
};
$validResponse = static fn(array $responses): string => $responses[5] ?? '';
$expectedCallbackEvents = ['/case|video, chat.v1, superchat|websocket, h2c, websocket|Upgrade, keep-alive, upgrade|13, 12, 13', 'open:chat.v1'];
var_dump($badStatuses($callbackResponses));
var_dump(str_contains($validResponse($callbackResponses), "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump(str_contains($validResponse($callbackResponses), "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"));
var_dump(str_contains($validResponse($callbackResponses), "Sec-WebSocket-Protocol: chat.v1\r\n"));
var_dump($callbackEvents === $expectedCallbackEvents);
var_dump($badStatuses($plainResponses));
var_dump(str_contains($validResponse($plainResponses), "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump(str_contains($validResponse($plainResponses), "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"));
var_dump(str_contains($validResponse($plainResponses), "Sec-WebSocket-Protocol: chat.v1\r\n"));
var_dump($plainEvents === ['open:chat.v1']);
var_dump($callbackStdout === '' && $callbackStderr === '');
var_dump($plainStdout === '' && $plainStderr === '');
@unlink($tmpDir . '/callback.txt'); @unlink($tmpDir . '/no-callback.txt');
@unlink($tmpDir . '/callback.php'); @unlink($tmpDir . '/no-callback.php'); @rmdir($tmpDir);
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
bool(true)
bool(true)
bool(true)
bool(true)
