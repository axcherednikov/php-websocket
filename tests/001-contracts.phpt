--TEST--
websocket extension exposes initial public contracts
--EXTENSIONS--
websocket
--FILE--
<?php
var_dump(class_exists(WebSocket\Server::class));
var_dump(class_exists(WebSocket\Connection::class));
var_dump(enum_exists(WebSocket\MessageType::class));
var_dump(class_exists(WebSocket\Frame::class));
var_dump(class_exists(WebSocket\Protocol::class));
var_dump(class_exists(Channels\Server::class));
var_dump(class_exists(Channels\App::class));

$server = new WebSocket\Server();
$server->listen('127.0.0.1', 8080);
$server->onOpen(static function () {});
$server->onMessage(static function () {});
$server->onClose(static function () {});
$server->onError(static function () {});
var_dump(in_array($server->getDriver(), ['kqueue', 'epoll', 'poll', 'select'], true));

$channels = new Channels\Server([new Channels\App('key', 'secret', '1')]);
$channels->listen('127.0.0.1', 8081);
$channels->onConnection(static function () {});
$channels->onSubscribe(static function () {});
$channels->onUnsubscribe(static function () {});
$channels->onClientEvent(static function () {});
var_dump($channels->trigger('public-chat', 'message', ['ok' => true]) instanceof stdClass);
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
