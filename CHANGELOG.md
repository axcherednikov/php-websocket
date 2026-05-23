# Changelog

All notable changes to ext-websocket are documented here.

## 1.1.0 - 2026-05-23

### Added

- Added `maxConnections`, `handshakeTimeoutMs`, and `idleTimeoutMs` server options to limit idle and slowloris-style connection pressure.
- Added UTF-8 validation for text messages and close reason payloads.
- Added validation for reserved WebSocket close codes on outgoing close frames.

### Changed

- Changed masked frame generation to use random mask keys instead of a deterministic mask.

### Fixed

- Fixed compatibility with PHP 8.1 and 8.2 when compiling generated class constants.
- Removed dependency on newer `ext/random` C headers when generating WebSocket mask keys.

## 1.0.0 - 2026-05-18

### Added

- Added `WebSocket\ServerOptions` for typed server configuration.

### Changed

- Updated `WebSocket\Server` to accept `ServerOptions` while keeping array options supported.
- Updated examples and production documentation to prefer `ServerOptions`.

## 0.9.1 - 2026-05-18

### Added

- Added the MIT license file.
- Added MIT license headers to source, benchmark, configuration, and workflow files.
- Added this changelog.
- Added production deployment notes.
- Added PIE package metadata.
- Added IDE and static-analysis contracts to `websocket.stub.php`.

## 0.8.0 - 2026-05-18

### Added

- Added real WebSocket message-runtime benchmarks.
- Added comparison runners for ext-websocket, AMPHP WebSocket Server, Workerman, and OpenSwoole.
- Added `ws://` and `wss://` benchmark modes with a shared local TLS terminator.
- Added benchmark PHPStan coverage and OpenSwoole stubs for the benchmark suite.

### Changed

- Simplified the root README and moved detailed benchmark output to `bench/README.md`.

## 0.7.0 - 2026-05-18

### Added

- Added bounded outgoing write queues for WebSocket connections.
- Added backpressure behavior for slow clients.
- Added runtime tests for queued writes and connection cleanup under backpressure.

## 0.6.0 - 2026-05-18

### Added

- Added fragmented text and binary message support.
- Added continuation frame handling and validation.
- Added total message size checks across fragmented frames.
- Added tests for valid fragmentation, invalid continuation sequences, and size-limit failures.

## 0.5.0 - 2026-05-18

### Added

- Added runtime handling for binary frames.
- Added ping, pong, and close frame handling in the native server runtime.
- Added protocol-error close behavior for invalid frames.
- Added runtime tests for frame processing and protocol errors.

## 0.4.0 - 2026-05-18

### Added

- Added HTTP Upgrade parsing for the native WebSocket server.
- Added `101 Switching Protocols` response generation.
- Added invalid-handshake rejection paths.
- Added integration tests for successful and rejected handshakes.

## 0.3.0-dev - 2026-05-17

### Added

- Added the first native `WebSocket\Server::run()` runtime.
- Added TCP listen/accept, non-blocking sockets, and the main server loop.
- Added native I/O drivers for `select`, `poll`, `epoll`, and `kqueue`.
- Added server runtime benchmarks for TCP accept and upgrade paths.
- Added `examples/run_server.php`.

## 0.2.1 - 2026-05-14

### Added

- Added GitHub Actions matrix builds for PHP 8.1 through PHP 8.5.
- Added Linux and macOS CI coverage.

### Fixed

- Fixed PHP 8.1 Zend API compatibility.

## 0.2.0 - 2026-05-14

### Changed

- Simplified the public API around focused `Server`, `Connection`, `Protocol`, `Frame`, and `CloseFrame` contracts.
- Reduced API surface before the native server runtime work continued.

### Documentation

- Improved the README for the early public API.

## 0.1.0 - 2026-05-13

### Added

- Added WebSocket protocol primitives.
- Added `WebSocket\MessageType`, `WebSocket\Frame`, `WebSocket\CloseFrame`, and `WebSocket\Protocol`.
- Added frame encode/decode helpers, raw pack/unpack helpers, close code constants, opcodes, and flags.
- Added tests for public contracts and protocol primitives.
