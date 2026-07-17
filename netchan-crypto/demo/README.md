# netchan-crypto demo: an encrypted echo over UDP

This demo runs the netchan reliable-datagram protocol over a real UDP socket,
wrapped in the `nc_crypto` encryption decorator, and driven by the `iox` event
loop. A server echoes back whatever a client sends it, over an encrypted
channel. It is the smallest thing that exercises every moving part: the X25519
handshake, per-packet AEAD, the reliable channel, and an event loop that owns
the socket, a retransmit timer, and the process signals.

## What is here

| File | Role |
|------|------|
| `netchan.c/.h` | the transport-agnostic protocol core (no socket headers) |
| `nc_udp.c/.h` | the UDP backend: the only code that knows `sockaddr` |
| `nc_crypto.c/.h` | the encryption decorator: X25519 + XChaCha20-Poly1305 |
| `secure_link.c/.h` | glue that drives netchan + nc_crypto over `iox` |
| `echo_server.c` | the echo server |
| `echo_client.c` | the interactive client (reads stdin, prints echoes) |
| `test_secure_link.c` | a self-contained loopback round-trip test, plus a check that the periodic tick keeps running |
| `iox/` | the vendored event loop (poll fds, one-shot timers, signals) |
| `third_party/` | vendored monocypher (see `VENDORING.md`) |

## Prerequisites

- A C compiler (gcc or clang) and GNU make.
- A checkout of [modular-make](https://github.com/) beside this tree. The
  `GNUmakefile` here is a one-line stub that includes it. By default it looks
  for `../../../DEVEL/modular-make`; override with
  `make MODULAR_MAKE=/path/to/modular-make`.

There are no other dependencies. The crypto primitives are the single vendored
`monocypher.c`; the event loop is vendored in `iox/`.

## Build

```sh
make                 # builds echo_server, echo_client, test_secure_link
make run-tests       # runs the loopback echo round-trip test
```

Binaries land in `_out/<triplet>/bin/`.

To build with the sanitizers the tests were checked under:

```sh
make CFLAGS="-fsanitize=address,undefined -g" LDFLAGS="-fsanitize=address,undefined"
```

## Run it

Open two terminals. Start the server first:

```sh
./_out/*/bin/echo_server 9000 --psk lan-party
```

Then connect a client and type lines at it:

```sh
./_out/*/bin/echo_client 127.0.0.1 9000 --psk lan-party
```

Every line you type is sent over the encrypted reliable channel and echoed
back, prefixed with `[echo]`. Press `Ctrl-D` to disconnect or `Ctrl-C` on the
server to shut it down.

### Options

- `--psk <phrase>` mixes a pre-shared key into the handshake. Both ends must
  pass the same phrase or the AEAD tag fails and no data flows. Omit it for an
  unauthenticated ephemeral handshake (encryption without a shared secret).
- `--plain` turns the crypto decorator off entirely, sending plaintext
  netchan datagrams. Useful for comparing the wire in a packet capture.

### What to expect

With `--psk`, launch order does not matter: the client repeats its handshake
`HELLO` until the server answers, so you can start either side first. With a
mismatched `--psk`, the client prints `connecting...` but never reaches
`session up`, because the server's sealed replies fail to authenticate.

Try `--plain` on one side only: the handshake never completes, since one end
is sealing packets the other cannot read.
