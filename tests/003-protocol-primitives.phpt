--TEST--
WebSocket\Protocol supports production frame primitives
--EXTENSIONS--
websocket
--FILE--
<?php

use WebSocket\CloseFrame;
use WebSocket\Frame;
use WebSocket\MessageType;
use WebSocket\Protocol;

var_dump(Protocol::OPCODE_CONTINUATION);
var_dump(Protocol::OPCODE_CLOSE);
var_dump(Protocol::FLAG_FIN | Protocol::FLAG_MASK);
var_dump(Protocol::CLOSE_MESSAGE_TOO_BIG);

$partial = new Frame(MessageType::Text, 'hel', false);
var_dump($partial->opcode);
var_dump($partial->flags);
var_dump(bin2hex(Protocol::pack($partial)));

$frame = Protocol::unpack(Protocol::pack('hi', Protocol::OPCODE_BINARY, Protocol::FLAG_FIN | Protocol::FLAG_MASK));
var_dump($frame instanceof Frame);
var_dump($frame->type === MessageType::Binary);
var_dump($frame->opcode);
var_dump($frame->flags);
var_dump($frame->payload);

$close = new CloseFrame(1009, 'too big');
$encodedClose = Protocol::pack($close);
var_dump(bin2hex($encodedClose));

$decodedClose = Protocol::unpack($encodedClose);
var_dump($decodedClose instanceof CloseFrame);
var_dump($decodedClose->code);
var_dump($decodedClose->reason);
var_dump($decodedClose->flags);

try {
    Protocol::pack('x', 0x3);
} catch (\ValueError $e) {
    var_dump($e instanceof \ValueError);
}

try {
    Protocol::pack('x', Protocol::OPCODE_PING, 0);
} catch (\ValueError $e) {
    var_dump($e instanceof \ValueError);
}

try {
    Protocol::unpack(hex2bin('8801ff'));
} catch (\Error $e) {
    var_dump($e instanceof \Error);
}
?>
--EXPECT--
int(0)
int(8)
int(33)
int(1009)
int(1)
int(0)
string(10) "010368656c"
bool(true)
bool(true)
int(2)
int(33)
string(2) "hi"
string(22) "880903f1746f6f20626967"
bool(true)
int(1009)
string(7) "too big"
int(1)
bool(true)
bool(true)
bool(true)
