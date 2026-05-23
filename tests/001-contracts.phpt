--TEST--
websocket extension exposes focused public contracts
--EXTENSIONS--
websocket
--FILE--
<?php
var_dump(class_exists(WebSocket\Server::class));
var_dump(class_exists(WebSocket\ServerOptions::class));
var_dump(class_exists(WebSocket\Request::class));
var_dump(class_exists(WebSocket\HandshakeResponse::class));
var_dump(class_exists(WebSocket\HandshakeException::class));
var_dump(class_exists(WebSocket\Connection::class));
var_dump(enum_exists(WebSocket\MessageType::class));
var_dump(class_exists(WebSocket\Frame::class));
var_dump(class_exists(WebSocket\CloseFrame::class));
var_dump(class_exists(WebSocket\Protocol::class));
var_dump(method_exists(WebSocket\Server::class, 'send'));
var_dump(method_exists(WebSocket\Server::class, 'close'));
var_dump(method_exists(WebSocket\Server::class, 'subprotocols'));
var_dump(method_exists(WebSocket\Server::class, 'onHandshake'));
var_dump((new ReflectionMethod(WebSocket\Server::class, 'subprotocols'))->isVariadic());
var_dump((new ReflectionMethod(WebSocket\Request::class, 'header'))->getReturnType()->allowsNull());
var_dump((new ReflectionMethod(WebSocket\HandshakeResponse::class, '__construct'))->getNumberOfParameters());
var_dump((new ReflectionMethod(WebSocket\HandshakeException::class, '__construct'))->getNumberOfParameters());
var_dump((new ReflectionMethod(WebSocket\Connection::class, 'send'))->getNumberOfParameters());
var_dump((new ReflectionProperty(WebSocket\Connection::class, 'subprotocol'))->getType()->allowsNull());
var_dump((new ReflectionMethod(WebSocket\ServerOptions::class, '__construct'))->getNumberOfParameters());
$options = new WebSocket\ServerOptions(maxMessageSize: 1024, maxQueuedBytes: 2048, maxConnections: 16, handshakeTimeoutMs: 250, idleTimeoutMs: 500);
var_dump($options->maxMessageSize);
var_dump($options->maxQueuedBytes);
var_dump($options->maxConnections);
var_dump($options->handshakeTimeoutMs);
var_dump($options->idleTimeoutMs);
try {
    new WebSocket\ServerOptions(maxMessageSize: 0);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
var_dump((new ReflectionMethod(WebSocket\Frame::class, '__construct'))->getNumberOfParameters());
var_dump((new ReflectionMethod(WebSocket\CloseFrame::class, '__construct'))->getNumberOfParameters());

$server = new WebSocket\Server();
$server->listen('127.0.0.1', 8080);
$server->subprotocols('chat.v1', 'superchat');
try {
    $server->subprotocols('bad protocol');
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
$server->onOpen(static function () {});
$server->onHandshake(static function () {});
$server->onMessage(static function () {});
$server->onClose(static function () {});
$server->onError(static function () {});
$response = new WebSocket\HandshakeResponse(401, ['X-Test' => 'ok'], 'nope');
var_dump($response->status);
var_dump($response->headers);
var_dump($response->body);
$exception = new WebSocket\HandshakeException($response);
var_dump($exception->response === $response);
var_dump(in_array($server->getDriver(), ['kqueue', 'epoll', 'poll', 'select'], true));
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
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
int(3)
int(1)
int(2)
bool(true)
int(5)
int(1024)
int(2048)
int(16)
int(250)
int(500)
WebSocket\ServerOptions::__construct(): Argument #1 ($maxMessageSize) must be at least 1
int(3)
int(2)
WebSocket\Server::subprotocols(): Argument #1 must be a valid WebSocket subprotocol token
int(401)
array(1) {
  ["X-Test"]=>
  string(2) "ok"
}
string(4) "nope"
bool(true)
bool(true)
