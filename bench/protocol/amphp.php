<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

use Amp\Websocket\Parser\Rfc6455FrameCompiler;
use Amp\Websocket\Parser\Rfc6455Parser;
use Amp\Websocket\Parser\WebsocketFrameHandler;
use Amp\Websocket\Parser\WebsocketFrameType;

ini_set('memory_limit', '512M');

require dirname(__DIR__) . '/vendor/autoload.php';

$compiler = new Rfc6455FrameCompiler(masked: false);
$handler = new class implements WebsocketFrameHandler {
    public string $payload = '';

    public function handleFrame(WebsocketFrameType $type, string $data, bool $final): void
    {
        if ($type === WebsocketFrameType::Text) {
            $this->payload = $data;
        }
    }
};
$parser = new Rfc6455Parser(
    frameHandler: $handler,
    masked: false,
    validateUtf8: false,
);

$adapterName = 'amphp/websocket-server';
$adapterVersion = Composer\InstalledVersions::getPrettyVersion('amphp/websocket-server') ?? 'installed';
$encode = static fn (string $payload): string => $compiler->compileFrame(WebsocketFrameType::Text, $payload, true);
$decode = static function (string $frame) use ($parser, $handler): string {
    $parser->push($frame);

    return $handler->payload;
};

require __DIR__ . '/common.php';
runBenchmarkSuite($adapterName, $adapterVersion, $encode, $decode);
