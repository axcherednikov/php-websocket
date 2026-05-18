<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

ini_set('memory_limit', '512M');

$serverClass = null;
foreach ([OpenSwoole\WebSocket\Server::class, Swoole\WebSocket\Server::class] as $class) {
    if (class_exists($class)) {
        $serverClass = $class;
        break;
    }
}

if ($serverClass === null) {
    fwrite(STDERR, "OpenSwoole WebSocket Server is not available.\n");
    fwrite(STDERR, "Install and enable ext-openswoole, then run this benchmark again.\n");
    fwrite(STDERR, "Example: pecl install openswoole && php -d extension=openswoole bench/protocol/openswoole.php\n");
    exit(1);
}

$opcodeText = websocketServerConstant($serverClass, 'WEBSOCKET_OPCODE_TEXT', 'WEBSOCKET_OPCODE_TEXT', 1);
$flagFin = websocketServerConstant($serverClass, 'WEBSOCKET_FLAG_FIN', 'WEBSOCKET_FLAG_FIN', 1);

$adapterName = $serverClass === OpenSwoole\WebSocket\Server::class ? 'openswoole' : 'swoole';
$extensionVersion = phpversion($adapterName);
$adapterVersion = $extensionVersion !== false ? $extensionVersion : 'loaded';
$encode = static fn (string $payload): string => $serverClass::pack($payload, $opcodeText, $flagFin);
$decode = static function (string $frame) use ($serverClass): string {
    $decoded = $serverClass::unpack($frame);
    if ($decoded === false) {
        throw new RuntimeException('OpenSwoole failed to unpack the WebSocket frame');
    }

    return $decoded->data;
};

require __DIR__ . '/common.php';
runBenchmarkSuite($adapterName, $adapterVersion, $encode, $decode);

function websocketServerConstant(string $class, string $classConstant, string $globalConstant, int $fallback): int
{
    $constant = $class . '::' . $classConstant;
    if (defined($constant)) {
        $value = constant($constant);
        if (is_int($value)) {
            return $value;
        }
    }

    if (defined($globalConstant)) {
        $value = constant($globalConstant);
        if (is_int($value)) {
            return $value;
        }
    }

    return $fallback;
}
