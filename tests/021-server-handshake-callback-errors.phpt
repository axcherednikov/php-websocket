--TEST--
WebSocket handshake callback exceptions and invalid returns retain failure behavior
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
$tmpDir = sys_get_temp_dir() . '/websocket-callback-errors-' . getmypid();
$serverFile = $tmpDir . '/server.php';
mkdir($tmpDir);
$probe = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
$port = (int) substr(strrchr(stream_socket_get_name($probe, false), ':'), 1);
fclose($probe);
$runCase = static function (string $callbackCode, int $port, string $serverFile, string $extension): array {
    $code = <<<'PHP'
<?php
use WebSocket\Connection;
use WebSocket\Server;
$server = new Server();
$server->listen('127.0.0.1', PORT_PLACEHOLDER);
$server->onHandshake(static function (): mixed { CALLBACK_PLACEHOLDER });
$server->onOpen(static function (Connection $connection) use ($server): void { file_put_contents(EVENTS_PLACEHOLDER, "open\n", FILE_APPEND); $server->stop(); });
try { $server->run(); } catch (Throwable $error) { file_put_contents(EVENTS_PLACEHOLDER, get_class($error) . ':' . $error->getMessage() . "\n", FILE_APPEND); }
PHP;
    $eventsFile = $serverFile . '.events';
    $code = str_replace(['PORT_PLACEHOLDER', 'CALLBACK_PLACEHOLDER', 'EVENTS_PLACEHOLDER'], [(string) $port, $callbackCode, var_export($eventsFile, true)], $code);
    file_put_contents($serverFile, $code);
    $process = proc_open([PHP_BINARY, '-n', '-d', 'extension=' . $extension, $serverFile], [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    $client = false; $deadline = microtime(true) + 5;
    do { $client = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.1); if (!$client) usleep(10000); } while (!$client && microtime(true) < $deadline);
    if ($client) {
        fwrite($client, implode("\r\n", ['GET / HTTP/1.1', 'Host: 127.0.0.1:' . $port, 'Upgrade: websocket', 'Connection: Upgrade', 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==', 'Sec-WebSocket-Version: 13', '', '']));
        stream_set_timeout($client, 1); $response = fread($client, 4096); fclose($client);
    } else { $response = ''; }
    $deadline = microtime(true) + 5;
    do { $status = proc_get_status($process); if (!$status['running']) break; usleep(10000); } while (microtime(true) < $deadline);
    if ($status['running']) proc_terminate($process);
    $stdout = stream_get_contents($pipes[1]); $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]); fclose($pipes[2]); proc_close($process);
    $events = file_exists($eventsFile) ? file_get_contents($eventsFile) : '';
    @unlink($eventsFile);
    return [$response, $events, $stdout, $stderr];
};
[$exceptionResponse, $exceptionEvents, $exceptionStdout, $exceptionStderr] = $runCase("throw new RuntimeException('ordinary');", $port, $serverFile, $extension);
[$returnResponse, $returnEvents, $returnStdout, $returnStderr] = $runCase("return 'invalid';", $port, $serverFile, $extension);
var_dump($exceptionResponse === '');
var_dump(str_contains($exceptionEvents, 'RuntimeException:ordinary'));
var_dump(! str_contains($exceptionEvents, 'open'));
var_dump($returnResponse === '');
var_dump(str_contains($returnEvents, 'TypeError:WebSocket handshake handler must return void, string returned'));
var_dump(! str_contains($returnEvents, 'open'));
var_dump($exceptionStdout === '' && $exceptionStderr === '');
var_dump($returnStdout === '' && $returnStderr === '');
@unlink($serverFile); @rmdir($tmpDir);
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
