--TEST--
WebSocket\Request preserves parsed handshake fields and retained values
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
$tmpDir = sys_get_temp_dir() . '/websocket-request-data-' . getmypid();
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
$retained = null;
$server->onHandshake(static function (Request $request) use (&$retained): void { $retained = $request; });
$server->onOpen(static function (Connection $connection) use ($server): void { $server->stop(); });
$server->run();
echo json_encode([$retained->method, $retained->target, $retained->headers, $retained->header('x-mIx'), $retained->header('x-empty'), $retained->header('missing')], JSON_UNESCAPED_SLASHES), "\n";
PHP;
$serverCode = str_replace('PORT_PLACEHOLDER', (string) $port, $serverCode);
file_put_contents($serverFile, $serverCode);
$process = proc_open([PHP_BINARY, '-n', '-d', 'extension=' . $extension, $serverFile], [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
$client = false;
$deadline = microtime(true) + 5;
do { $client = @stream_socket_client('tcp://127.0.0.1:' . $port, $errno, $errstr, 0.1); if (!$client) usleep(10000); } while (!$client && microtime(true) < $deadline);
if ($client) {
    fwrite($client, implode("\r\n", ['gEt /MiXeD?q=1 HTTP/1.1', 'Host: 127.0.0.1:' . $port, 'Upgrade: websocket', 'Connection: Upgrade', 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==', 'Sec-WebSocket-Version: 13', "X-MiX: \tone \t", 'x-mix: two', "X-Empty:\t", '', '']));
    stream_set_timeout($client, 1);
    $response = fread($client, 4096);
    fclose($client);
} else { $response = ''; }
$deadline = microtime(true) + 5;
do { $status = proc_get_status($process); if (!$status['running']) break; usleep(10000); } while (microtime(true) < $deadline);
if ($status['running']) proc_terminate($process);
$stdout = stream_get_contents($pipes[1]);
$stderr = stream_get_contents($pipes[2]);
fclose($pipes[1]); fclose($pipes[2]); proc_close($process);
var_dump(str_contains($response, "HTTP/1.1 101 Switching Protocols\r\n"));
var_dump($client !== false);
echo str_replace((string) $port, 'PORT', $stdout);
var_dump($stderr === '');
@unlink($serverFile); @rmdir($tmpDir);
?>
--EXPECT--
bool(true)
bool(true)
["gEt","/MiXeD?q=1",{"host":"127.0.0.1:PORT","upgrade":"websocket","connection":"Upgrade","sec-websocket-key":"dGhlIHNhbXBsZSBub25jZQ==","sec-websocket-version":"13","x-mix":"one, two","x-empty":""},"one, two","",null]
bool(true)
