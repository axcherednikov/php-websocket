--TEST--
WebSocket handshake waits for complete headers and preserves a buffered frame
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
use WebSocket\Connection;
use WebSocket\Request;
use WebSocket\Server;
$root = dirname(__DIR__);
$extension = $root . '/modules/websocket.so';
$tmpDir = sys_get_temp_dir() . '/websocket-buffering-' . getmypid();
$eventsFile = $tmpDir . '/events.txt';
$serverFile = $tmpDir . '/server.php';
mkdir($tmpDir);
$probe = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
$port = (int) substr(strrchr(stream_socket_get_name($probe, false), ':'), 1);
fclose($probe);
$serverCode = <<<'PHP'
<?php
use WebSocket\Connection;
use WebSocket\Request;
use WebSocket\Server;
$server = new Server();
$server->listen('127.0.0.1', PORT_PLACEHOLDER);
$server->onHandshake(static function (Request $request): void { file_put_contents(EVENTS_PLACEHOLDER, "handshake\n", FILE_APPEND); });
$server->onMessage(static function (Connection $connection, string $message) use ($server): void {
    file_put_contents(EVENTS_PLACEHOLDER, "message:" . $message . "\n", FILE_APPEND);
    $server->stop();
});
$server->run();
PHP;
$serverCode = str_replace(['PORT_PLACEHOLDER', 'EVENTS_PLACEHOLDER'], [(string) $port, var_export($eventsFile, true)], $serverCode);
file_put_contents($serverFile, $serverCode);
$process = proc_open([PHP_BINARY, '-n', '-d', 'extension=' . $extension, $serverFile], [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
$client = false;
$deadline = microtime(true) + 5;
do { $client = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.1); if (!$client) usleep(10000); } while (!$client && microtime(true) < $deadline);
if ($client) {
    stream_set_timeout($client, 1);
    fwrite($client, "GET /buffered HTTP/1.1\r\nHost: 127.0.0.1:$port\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nX-Pending: part");
    $read = [$client]; $write = []; $except = [];
    $readable = stream_select($read, $write, $except, 0, 150000);
    $earlyResponse = $readable > 0 ? fread($client, 4096) : '';
    $before = file_exists($eventsFile) ? file_get_contents($eventsFile) : '';
    fwrite($client, "ial\r\n\r\n" . pack('C*', 0x81, 0x82, 1, 2, 3, 4, ord('h') ^ 1, ord('i') ^ 2));
    $response = fread($client, 4096);
    fclose($client);
} else { $before = ''; $earlyResponse = ''; $readable = false; $response = ''; }
$deadline = microtime(true) + 5;
do { $status = proc_get_status($process); if (!$status['running']) break; usleep(10000); } while (microtime(true) < $deadline);
if ($status['running']) proc_terminate($process);
$stdout = stream_get_contents($pipes[1]); $stderr = stream_get_contents($pipes[2]);
fclose($pipes[1]); fclose($pipes[2]); proc_close($process);
$events = file_exists($eventsFile) ? file($eventsFile, FILE_IGNORE_NEW_LINES) : [];
var_dump($client !== false);
var_dump($readable === 0 && $earlyResponse === '');
var_dump($before === '');
var_dump(str_contains($response, "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump($events);
var_dump($stdout === ''); var_dump($stderr === '');
@unlink($eventsFile); @unlink($serverFile); @rmdir($tmpDir);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
array(2) {
  [0]=>
  string(9) "handshake"
  [1]=>
  string(10) "message:hi"
}
bool(true)
bool(true)
