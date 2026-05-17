<?php

declare(strict_types=1);

ini_set('memory_limit', '512M');

require dirname(__DIR__) . '/vendor/autoload.php';

$adapterName = 'ratchet/rfc6455';
$adapterVersion = Composer\InstalledVersions::getPrettyVersion('ratchet/rfc6455') ?? 'installed';
$encode = static function (string $payload): string {
    $frame = new Ratchet\RFC6455\Messaging\Frame(
        $payload,
        true,
        Ratchet\RFC6455\Messaging\Frame::OP_TEXT
    );

    return $frame->getContents();
};
$decode = static function (string $frame): string {
    $decoded = new Ratchet\RFC6455\Messaging\Frame();
    $decoded->addBuffer($frame);

    return $decoded->getPayload();
};

require __DIR__ . '/common.php';
runBenchmarkSuite($adapterName, $adapterVersion, $encode, $decode);
