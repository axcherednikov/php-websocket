<?php

declare(strict_types=1);

/**
 * @param Closure(string): string $encode
 * @param Closure(string): string $decode
 */
function runBenchmarkSuite(string $adapterName, string $adapterVersion, Closure $encode, Closure $decode): void
{
    $arguments = $_SERVER['argv'] ?? [];
    if (!is_array($arguments)) {
        $arguments = [];
    }

    $iterations = benchmarkIterations($arguments);
    if ($iterations <= 0) {
        fwrite(STDERR, "Iterations must be a positive integer.\n");
        exit(1);
    }

    $smallSize = 64;
    $largeSize = 1024;
    $smallPayload = makePayload($smallSize);
    $largePayload = makePayload($largeSize);
    $largeIterations = max(1, intdiv($iterations, 5));

    printf("Library: %s %s\n", $adapterName, $adapterVersion);
    printf("Iterations: %d\n\n", $iterations);

    runBenchmark("encode text {$smallSize}B", $smallPayload, $iterations, $encode);
    runBenchmark("decode masked text {$smallSize}B", clientFrame($smallPayload), $iterations, $decode);
    runBenchmark("encode text {$largeSize}B", $largePayload, $largeIterations, $encode);
    runBenchmark("decode masked text {$largeSize}B", clientFrame($largePayload), $largeIterations, $decode);
}

/**
 * @param array<array-key, mixed> $arguments
 */
function benchmarkIterations(array $arguments): int
{
    if (!isset($arguments[1])) {
        return 100000;
    }

    if (!is_numeric($arguments[1])) {
        return 0;
    }

    return (int) $arguments[1];
}

/**
 * @param Closure(string): string $callback
 */
function runBenchmark(string $name, string $subject, int $iterations, Closure $callback): void
{
    $warmup = min(10000, max(100, intdiv($iterations, 20)));
    $sink = 0;

    for ($i = 0; $i < $warmup; $i++) {
        $sink += strlen($callback($subject));
    }

    $start = hrtime(true);
    for ($i = 0; $i < $iterations; $i++) {
        $sink += strlen($callback($subject));
    }
    $elapsed = (hrtime(true) - $start) / 1e9;

    printf(
        "%s: %d ops in %.4fs (%.0f ops/sec)\n",
        $name,
        $iterations,
        $elapsed,
        $iterations / $elapsed
    );

    if ($sink === 0) {
        fwrite(STDERR, "Unexpected empty benchmark result.\n");
    }
}

function makePayload(int $size): string
{
    return substr(str_repeat('abcdefghijklmnopqrstuvwxyz0123456789', intdiv($size, 36) + 1), 0, $size);
}

function clientFrame(string $payload): string
{
    $length = strlen($payload);
    $mask = "\x12\x34\x56\x78";
    $header = "\x81";

    if ($length < 126) {
        $header .= chr(0x80 | $length);
    } elseif ($length <= 0xffff) {
        $header .= chr(0x80 | 126) . pack('n', $length);
    } else {
        $header .= chr(0x80 | 127) . pack('NN', 0, $length);
    }

    return $header . $mask . maskPayload($payload, $mask);
}

function maskPayload(string $payload, string $mask): string
{
    $masked = '';
    $length = strlen($payload);

    for ($i = 0; $i < $length; $i++) {
        $masked .= $payload[$i] ^ $mask[$i % 4];
    }

    return $masked;
}
