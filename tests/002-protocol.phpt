--TEST--
WebSocket\Protocol exposes embeddable RFC6455 helpers
--EXTENSIONS--
websocket
--FILE--
<?php

use WebSocket\Frame;
use WebSocket\MessageType;
use WebSocket\Protocol;

var_dump(Protocol::acceptKey('dGhlIHNhbXBsZSBub25jZQ=='));

$frame = Protocol::encode('hello', MessageType::Text);
var_dump(bin2hex($frame));

$decoded = Protocol::decode($frame);
var_dump($decoded instanceof Frame);
var_dump($decoded->type === MessageType::Text);
var_dump($decoded->payload);
var_dump($decoded->final);
var_dump($decoded->bytesConsumed);

var_dump(Protocol::decode(substr($frame, 0, 1)));

$masked = Protocol::encode('hi', MessageType::Text, true);
var_dump(bin2hex($masked));
var_dump(Protocol::decode($masked)->payload);
?>
--EXPECT--
string(28) "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
string(14) "810568656c6c6f"
bool(true)
bool(true)
string(5) "hello"
bool(true)
int(7)
NULL
string(16) "8182123456787a5d"
string(2) "hi"
