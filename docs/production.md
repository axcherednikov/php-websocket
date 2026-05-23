# Production Notes

`ext-websocket` provides a native WebSocket protocol core and a small native server runtime. It is usable for real services, but it is still a `0.x` release, so treat the API as close to stable but not final.

## TLS and `wss://`

Terminate TLS outside the extension with an established proxy:

- nginx
- Caddy
- Traefik
- HAProxy
- stunnel

Run `WebSocket\Server` behind that proxy over local `ws://` traffic. This keeps certificate renewal, ALPN, HTTP routing, access logs, and TLS hardening in infrastructure built for that job.

## Process Model

The native server runtime is currently a single PHP process with one event loop. For multi-core deployments, run several PHP server processes behind a load balancer or process manager.

Recommended process managers:

- systemd
- supervisord
- Docker / Kubernetes
- launchd on macOS development machines

## Limits

Set explicit limits for your workload:

```php
$server = new WebSocket\Server(new WebSocket\ServerOptions(
    maxMessageSize: 1024 * 1024,
    maxQueuedBytes: 8 * 1024 * 1024,
    maxConnections: 1000,
    handshakeTimeoutMs: 5000,
    idleTimeoutMs: 60000,
));
```

`maxMessageSize` protects incoming frames and fragmented messages. `maxQueuedBytes` protects memory when a client reads slowly and outgoing writes need to be queued. `maxConnections`, `handshakeTimeoutMs`, and `idleTimeoutMs` protect file descriptors and event-loop work from slowloris-style or idle-connection pressure.

## Slow Clients

Slow clients are handled with a bounded write queue. When broadcasting to many clients, always expect some clients to lag or disconnect. Keep `maxQueuedBytes` conservative and test with realistic client behavior before raising it.

## Graceful Shutdown

Use `Server::stop()` from your own signal handling or lifecycle code when possible. Active connections are closed during shutdown.

## Observability

Application callbacks should record at least:

- accepted connections
- closed connections
- message count
- broadcast fanout size
- callback exceptions
- rejected handshakes
- protocol-error closes

The extension intentionally keeps the public API small, so production metrics should live in userland code around your callbacks.

## Deployment Checklist

- Run CI on every supported PHP version.
- Run `make test` against the built extension.
- Run application-level load tests with realistic payloads and fanout.
- Disable Xdebug for benchmarks and production.
- Put `wss://` behind a TLS proxy.
- Start with conservative `maxMessageSize` and `maxQueuedBytes`.
- Set `maxConnections`, `handshakeTimeoutMs`, and `idleTimeoutMs` for the deployment envelope.
- Keep the extension and your PHP runtime on the same PHP minor version used during build.
