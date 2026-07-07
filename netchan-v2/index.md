---
title: "netchan-v2: Revisited — A Transport Seam for the Browser"
date: 2026-07-06
abstract: "Part two of the netchan story: cutting one clean seam through the protocol core so the same code can run over UDP on the desktop and, soon, over WebRTC or WebSockets in a browser."
category: networking
---

## Introduction

The [first netchan article](/netchan/) compared four reliable-UDP designs and
built one: a multiplexed channel protocol in about 1,500 lines of C, with
fragmentation, a 64-slot reorder buffer, credit-based flow control, and a fully
static memory model. It was a good desktop library and a self-contained
teaching example, but it made one assumption everywhere: that the network is a
POSIX UDP socket. `struct sockaddr` and `socklen_t` are stamped straight into
its public API.

That assumption is fine until you want to run the same protocol somewhere a
socket does not exist. A WebAssembly client in a browser cannot open a UDP
socket at all. It can, however, open a WebRTC data channel or a WebSocket, both
of which hand you a datagram-shaped pipe. The protocol logic above that pipe is
identical; only the plumbing below it differs.

The groundwork here is unglamorous: it replaces the baked-in socket address with
a single opaque type, and moves every line that knows what a socket is into one
small backend file. It is a short change. Once this seam exists, adding a browser
transport, or an encrypted one, stops being a rewrite and becomes a new file.

## Abstract

netchan-v2 introduces `nc_addr`, a transport-agnostic address the protocol core
copies and compares but never interprets, and `nc_udp`, a 98-line backend that
is the only code in the project aware of `sockaddr`. The core no longer includes
a single socket header. The change is behaviour-preserving: the existing ten
loopback tests still pass, and a new real-socket test exercises the UDP backend
directly, including IPv4/IPv6 address round-tripping and connection migration.
To show the core is now genuinely platform-independent, it also compiles to
WebAssembly and runs the same test suite under `node`. The first new backend
follows immediately: an encrypted UDP transport for the desktop, a
WireGuard/Noise-style decorator built on vendored public-domain crypto, added
without touching the core. WebRTC and WebSocket backends for a browser client
are the additive modules that come next.

## The Assumption, Made Explicit

The old public API named the operating system's address type directly:

```c
int    netchan_connect  (struct netchan_conn *c,
                         const struct sockaddr *addr, socklen_t addrlen);
int    netchan_feed     (struct netchan_conn *c, const void *pkt, size_t len,
                         const struct sockaddr *from, socklen_t fromlen);
size_t netchan_send_next(struct netchan_conn *c, void *buf, size_t buflen,
                         struct sockaddr *to, socklen_t *tolen);
```

Three functions, and through them the whole library, depend on `<sys/socket.h>`.
The core also stored the peer as a `sockaddr_storage`, compared addresses with
`memcmp` over a `socklen_t`, and even built a `sockaddr_in` by hand when
decoding a redirect frame. None of that logic cares about IP addressing. It
needs an address only as an opaque token: something to store, to copy into an
outgoing packet's destination, and to compare for equality when a peer roams to
a new address. That is the entire contract.

## nc_addr: An Address the Core Cannot Read

`nc_addr` is that contract and nothing more:

```c
#define NC_ADDR_MAX 20

struct nc_addr {
    uint8_t len;              /* bytes used in a[]; 0 means "unset" */
    uint8_t a[NC_ADDR_MAX];
};
```

The core treats `a[]` as bytes. Each transport decides how to fill them. The UDP
backend packs a family tag, an IPv4 or IPv6 address, and a port. A future WebRTC
backend can pack a channel handle, an integer index into a table of data
channels the browser gave it, because in a browser there is no IP address to
speak of, only "which peer." The core cannot tell the difference between an IP
and a handle, which is exactly why it can run on both.

The three signatures collapse to their essence:

```c
int    netchan_connect  (struct netchan_conn *c, const struct nc_addr *addr);
int    netchan_feed     (struct netchan_conn *c, const void *pkt, size_t len,
                         const struct nc_addr *from);
size_t netchan_send_next(struct netchan_conn *c, void *buf, size_t buflen,
                         struct nc_addr *to);
```

Connection migration, the feature that lets a player survive a NAT rebinding or
a Wi-Fi-to-cellular handoff, was a `memcmp` over socket bytes. It stays a
`memcmp`, now over `nc_addr` bytes, and works unchanged for any transport whose
addresses can differ:

```c
if (from->len != c->peer_addr.len ||
    memcmp(from->a, c->peer_addr.a, from->len) != 0) {
    if (validate_migration(c, ...))
        c->peer_addr = *from;
}
```

A small piece of luck made the redirect path especially clean. netchan's
`CONNECT_REDIRECT` frame already carries a compact `[type][address][port]`
layout on the wire. That is precisely the packing `nc_udp` uses, so decoding a
redirect is now a `memcpy` into an `nc_addr`, and the core dropped its last
`sockaddr_in`, its last `htons`, and its `<arpa/inet.h>` include along with it.

## nc_udp: Where sockaddr Lives, and Only There

Everything the operating system knows about addresses now sits in one file pair,
`nc_udp.h` and `nc_udp.c`, together under 100 lines. Two functions bridge the OS
and the core:

```c
int       nc_udp_from_sockaddr(struct nc_addr *a,
                               const struct sockaddr *sa, socklen_t salen);
socklen_t nc_udp_to_sockaddr  (const struct nc_addr *a,
                               struct sockaddr_storage *ss);
```

The application calls `from_sockaddr` on the address `recvfrom` gives it, then
feeds the packet in; it calls `to_sockaddr` on the address `send_next` reports,
then `sendto`. The chat example changed only at those two syscall boundaries.
The protocol core, the reliability layer, the fragmentation logic, the flow
control: none of it recompiles against a socket header any more.

| | Before | After |
|---|---|---|
| Address type in public API | `struct sockaddr` + `socklen_t` | `struct nc_addr` |
| Socket headers in core | `<sys/socket.h>`, `<arpa/inet.h>` | none |
| Code that knows about IP | scattered through the core | `nc_udp` only, ~98 lines |
| Adding a new transport | edit the core | add a backend file |

## Validation

The refactor is meant to change nothing an existing user can observe, so the
bar is that the old behaviour is still exactly the old behaviour.

- The original ten loopback tests pass unchanged. They pack an `nc_addr` by hand
  as an opaque token, since the loopback harness never touches a real socket.
- A new real-socket test (`nc_udp_test`) drives the actual `nc_udp` backend over
  two live loopback UDP sockets: it round-trips both an IPv4 and an IPv6 address
  through `from_sockaddr`/`to_sockaddr` and checks every field survives, then
  runs a full session and migrates the client to a new socket mid-connection,
  confirming the server's reliable acks follow it to the new address.
- Everything above builds clean under `-Wall -Wextra -Werror` and runs clean
  under AddressSanitizer and UndefinedBehaviorSanitizer.

## Proof: The Same Core, Native and in wasm

A transport seam is only convincing if the core actually crosses it. Because
`netchan.c` no longer includes a socket header, it now compiles to WebAssembly
unchanged, and the socketless loopback test suite runs identically in both
places.

The demo builds with a small modular build driver that produces native and wasm
targets from one tree:

```sh
make                          # native: core, tests, and the UDP chat example
make CC=emcc CXX=em++         # wasm: core and tests, example dropped automatically
```

The chat example, the one piece that calls `bind` and `recvfrom`, is excluded
on the emscripten target by a single guard in the build descriptor. Nothing in
the protocol core is conditional. The same `nc_addr` seam that lets a new
transport slot in is what lets the socket transport slot *out* for a build that
has no sockets.

To give the eventual game something to say, the demo also carries a wire schema.
Game messages, player input, per-entity snapshot state, and a join handshake,
are described in a short `.idl` and compiled to zero-allocation C encoders by a
vendored IDL generator ([microser](/serialization-formats/)). A round-trip test
for the generated code runs alongside the protocol tests.

Both test binaries, compiled to wasm, pass under `node`:

```
netchan tests:
  handshake                     OK
  ...
  stats                         OK
10/10 tests passed

proto round-trip OK (player_input, entity_state, welcome)
```

The identical suite passes as a native binary. Same protocol core, two platforms,
and a wire schema already waiting for the browser client to speak it.

## The First New Backend: Encryption as a Decorator

With the seam cut, the first thing worth adding is an encrypted transport for
the desktop, and it lands without touching the core at all. It is a
*decorator*: `netchan_send_next` produces a plaintext datagram, the crypto
layer seals it before the socket `sendto`, and incoming datagrams are opened
before `netchan_feed`. netchan never knows.

The shape is WireGuard/Noise, deliberately not QUIC. There is no TLS state
machine, no certificate chain, no PKI, and no per-stream keying (netchan already
multiplexes):

- **Handshake**: one X25519 ephemeral exchange per connection. Both sides send a
  `HELLO` carrying their ephemeral public key; each derives the session from the
  shared secret. An optional pre-shared key can be mixed in for a closed LAN
  game. The keys are *directional*, one for each direction, hashed out of the
  shared secret with BLAKE2b, so both sides can start their packet counters at 1
  with no risk of nonce reuse.
- **Per packet**: XChaCha20-Poly1305 AEAD. The 24-byte nonce is a 64-bit counter;
  the counter travels in the packet so the receiver can reconstruct the nonce and
  run a sliding replay window over it. Overhead is 25 bytes per datagram.

The primitives come from [Monocypher](https://monocypher.org), a single-file
public-domain (CC0) library, vendored in. There is no external package to
install. The backend itself is 200 lines.

It is desktop-only by construction. WebRTC data channels mandate DTLS and `wss`
is TLS, so a browser transport already encrypts; the crypto backend is simply how
the desktop UDP path earns the same guarantee. It is excluded from the wasm build
by the same one-line guard that drops the socket example.

A loopback test runs a full reliable netchan session end to end through the
cipher and confirms the payload arrives byte-exact, then checks that a tampered
packet and a replayed packet are both rejected. All three pass under
AddressSanitizer and UndefinedBehaviorSanitizer:

```
nc_crypto tests:
  encrypted reliable session      OK
  tampered packet rejected        OK
  replayed packet rejected        OK
3/3 tests passed
```

## What the Seam Unlocks Next

The crypto backend proved the pattern on hardware we control. The same seam
reaches the browser, where the transports are not sockets at all:

- **A WebRTC data channel**, configured unreliable and unordered, behaves like
  UDP; netchan's own reliability and reordering ride on top. Its address is a
  handle, not an IP, and `nc_addr` already accommodates that.
- **A WebSocket** is a reliable, ordered fallback for networks that block
  everything else. netchan's layer runs over it too, redundantly but correctly,
  which is the right trade when the alternative is not connecting at all.

Because none of this reaches the core, a single server can in principle accept a
UDP client and a WebRTC client at once: each connection carries whichever
`nc_addr` its transport produced, and the protocol logic above is identical. A
browser-based game demo talking to a Linux server, alongside native UDP clients,
is where this series is headed.

## Conclusion

This was intentionally a small part. The whole change is one new type, one new
backend file, and the deletion of every socket header from the core. But it
converts "port netchan to the browser" from a fork into an afternoon, and it
does so without giving up any of the protocol capability the first article
built. The groundwork is laid; the next part steps through the seam and out the
other side, into a transport that has no sockets at all.
