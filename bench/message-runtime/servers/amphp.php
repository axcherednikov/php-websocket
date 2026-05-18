<?php

/*
 * Copyright (c) 2026 Aleksandr Cherednikov
 * Licensed under the MIT License. See LICENSE for details.
 */


declare(strict_types=1);

use Amp\Http\Server\DefaultErrorHandler;
use Amp\Http\Server\Request;
use Amp\Http\Server\Response;
use Amp\Http\Server\SocketHttpServer;
use Amp\Socket\InternetAddress;
use Amp\Websocket\Parser\Rfc6455ParserFactory;
use Amp\Websocket\Server\Rfc6455Acceptor;
use Amp\Websocket\Server\Rfc6455ClientFactory;
use Amp\Websocket\Server\Websocket;
use Amp\Websocket\Server\WebsocketClientHandler;
use Amp\Websocket\WebsocketClient;
use Psr\Log\NullLogger;

require dirname(__DIR__, 2) . '/vendor/autoload.php';
require __DIR__ . '/bootstrap.php';

$logger = new NullLogger();
$server = SocketHttpServer::createForDirectAccess($logger);
$scenario = messageRuntimeBenchmarkScenario();
$connectionsExpected = messageRuntimeBenchmarkConnections();
$messagesExpected = messageRuntimeBenchmarkMessages();

$handler = new class($scenario, $connectionsExpected, $messagesExpected) implements WebsocketClientHandler {
	/** @var array<int, WebsocketClient> */
	private array $clients = [];

	private int $opened = 0;

	private int $messages = 0;

	private int|float|null $idleStartedAt = null;

	private int|float|null $messageStartedAt = null;

	public function __construct(
		private readonly string $scenario,
		private readonly int $connectionsExpected,
		private readonly int $messagesExpected,
	) {
	}

	public function handleClient(WebsocketClient $client, Request $request, Response $response): void
	{
		if ($this->idleStartedAt === null) {
			$this->idleStartedAt = hrtime(true);
		}

		$this->clients[$client->getId()] = $client;
		$this->opened++;

		if ($this->scenario === 'idle' && $this->opened >= $this->connectionsExpected) {
			messageRuntimeBenchmarkWriteResult('idle', $this->opened, messageRuntimeBenchmarkElapsed($this->idleStartedAt));
		}

		while ($message = $client->receive()) {
			$payload = $message->buffer();

			if ($this->messageStartedAt === null) {
				$this->messageStartedAt = hrtime(true);
			}

			if ($this->scenario === 'broadcast') {
				foreach ($this->clients as $peer) {
					$peer->sendText($payload);
				}

				$this->messages++;
				if ($this->messages >= $this->messagesExpected) {
					messageRuntimeBenchmarkWriteResult('broadcast', $this->messages * $this->connectionsExpected, messageRuntimeBenchmarkElapsed($this->messageStartedAt));
				}

				continue;
			}

			$client->sendText($payload);
			$this->messages++;

			if ($this->scenario === 'echo' && $this->messages >= $this->messagesExpected) {
				messageRuntimeBenchmarkWriteResult('echo', $this->messages, messageRuntimeBenchmarkElapsed($this->messageStartedAt));
			}
		}

		unset($this->clients[$client->getId()]);
	}
};

$websocket = new Websocket(
	httpServer: $server,
	logger: $logger,
	acceptor: new Rfc6455Acceptor(),
	clientHandler: $handler,
	clientFactory: new Rfc6455ClientFactory(
		heartbeatQueue: null,
		rateLimit: null,
		parserFactory: new Rfc6455ParserFactory(
			validateUtf8: true,
			messageSizeLimit: PHP_INT_MAX,
			frameSizeLimit: PHP_INT_MAX,
		),
	),
);

$server->expose(new InternetAddress('127.0.0.1', messageRuntimeBenchmarkInternetPort()));
$server->start($websocket, new DefaultErrorHandler());

Amp\trapSignal([SIGTERM, SIGINT]);
$server->stop();
