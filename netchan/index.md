---
title: Reliable UDP for Games — QuakeWorld, ENet, QUIC, and Netchan Compared
date: 2026-05-09
abstract: "A comparison of four UDP multiplexing approaches for game networking, from the 1996 QuakeWorld NetChannel to a near-minimal modern implementation in 1,400 lines of C"
category: networking
---

## Introduction

Every multiplayer game faces the same transport problem. TCP guarantees delivery but adds latency through head-of-line blocking: one lost packet stalls everything behind it, even packets for unrelated game state. UDP avoids this but provides no delivery guarantees at all. The game needs both: reliable delivery for chat messages, inventory changes, and map loads; unreliable delivery for position updates that expire in milliseconds.

The solution, reinvented across three decades of game development, is a reliability layer over UDP. The implementations range from a few hundred lines of C (QuakeWorld, 1996) to hundreds of thousands (QUIC, 2021). Four points on that spectrum:

- **QuakeWorld NetChannel** — the 1996 protocol that proved client-side prediction could make internet FPS playable over dial-up
- **ENet** — the de facto library for indie game networking since 2004, used by Sauerbraten, Godot Engine, and others
- **QUIC** — the IETF-standardized protocol (RFC 9000) that powers HTTP/3, with mandatory TLS 1.3 encryption
- **Netchan** — a near-minimal multiplexed channel protocol in 1,400 lines of C, designed as a practical baseline for small multiplayer games

## Abstract

We analyze the protocol design, packet overhead, reliability mechanisms, and implementation complexity of four UDP-based transport approaches. QuakeWorld's NetChannel introduced the reliable/unreliable split that became the industry template but limited itself to one reliable message in flight. ENet generalized this into a channel-based library with sliding windows and congestion control. QUIC brought stream multiplexing and mandatory encryption to the IETF standards track but at a complexity cost that dwarfs what most games need. Netchan demonstrates that a modern multiplexed channel protocol covering these requirements fits in a single C file with no external dependencies.

## The QuakeWorld NetChannel

QuakeWorld (1996) solved a problem that seemed impossible: making a fast-paced FPS playable over 200ms dial-up connections. The key insight was client-side prediction, but the networking protocol that enabled it was equally important.

### Packet Format

The QuakeWorld packet header is 10 bytes:

| Offset | Size | Field | Purpose |
|--------|------|-------|---------|
| 0 | 4 | Sequence | Outgoing packet counter; MSB = "reliable data" flag |
| 4 | 4 | ACK Sequence | Last received sequence from peer; MSB = reliable ACK |
| 8 | 2 | QPort | Random client ID to work around NAT port remapping |

Commands follow the header as variable-length messages. A single UDP packet mixes reliable and unreliable data freely.

### Reliability Model

QuakeWorld permits exactly **one reliable message in flight** at a time. The sender flags the reliable bit in the sequence number and waits for acknowledgment before queuing the next reliable message. ACKs piggyback on regular data packets, avoiding dedicated acknowledgment traffic during gameplay. If the ACK does not arrive, the reliable message is retransmitted with the next outgoing packet.

This is the simplest possible reliability scheme that works. At 20 server frames per second, a lost reliable message retransmits within 50ms. The single-message-in-flight constraint rarely matters because reliable traffic (map changes, connect/disconnect) is infrequent compared to unreliable traffic (player positions, inputs).

### What It Gets Right

- The unreliable/reliable split maps directly to game requirements
- Implicit ACKs eliminate dedicated acknowledgment packets
- Circular buffer indexed by bitwise AND avoids modulo operations
- The entire implementation fits in ~400 lines of C

### Limitations

- One reliable message in flight serializes all reliable operations
- No flow control or congestion detection
- No encryption
- No channel multiplexing; all data shares one stream
- No fragmentation support for large messages

The QuakeWorld model works because games produce a constant stream of small unreliable packets that carry the ACKs for free. Quake 2 and Quake 3 Arena inherited this design, adding delta compression and Huffman coding but keeping the single-reliable-message constraint.

## ENet

ENet, written by Lee Salzman for the Cube 2 engine (Sauerbraten), has been the go-to reliable UDP library for indie games since 2004. It generalizes the QuakeWorld model into a proper library with multiple peers, channels, and congestion control.

### Architecture

ENet manages a `host` that communicates with multiple `peers`, each supporting up to 255 independent channels. The application drives I/O through `enet_host_service()`, which returns events (connect, receive, disconnect) in a polling loop. The API surface is roughly 50 functions.

### Channel System

Each channel maintains independent sequence counters and supports three delivery modes:

| Mode | Ordered | Guaranteed | Use Case |
|------|---------|------------|----------|
| Reliable sequenced | Yes | Yes | Chat, inventory, RPC |
| Unreliable sequenced | Yes | No | Position updates |
| Unsequenced | No | No | Fire-and-forget effects |

Channels are independently buffered and drained in numerical order, giving lower-numbered channels implicit priority.

### Packet Format

ENet uses a 4-byte packet header (peer ID + send time) plus a 4-byte command header per command (type, channel ID, reliable sequence number). With UDP/IP overhead of 28 bytes, a minimal single-command packet is 36 bytes.

Multiple commands aggregate into a single UDP packet, amortizing the header cost. Messages exceeding the MTU (default 1,392 bytes, configurable 576–4,096) are fragmented transparently, with bitmap tracking for reassembly.

### Congestion Control

ENet tracks round-trip time per peer and implements a probabilistic throttle (0–32 scale) that adjusts based on RTT deviation. Static bandwidth limits (bytes/second) can be configured per host. This is more sophisticated than QuakeWorld's non-existent congestion handling but simpler than TCP's algorithms.

### Strengths

- Battle-tested in real games over two decades
- Clean event-driven API, no threading required
- Aggregation of multiple commands per packet reduces overhead
- Congestion control prevents overwhelming slow connections

### Limitations

- No IPv6 in the original library (the ENet6 fork adds dual-stack support)
- No encryption; application must layer DTLS or equivalent
- Community maintains several forks alongside the original
- No connection migration
- Fixed MTU assumption; no path MTU discovery
- Single-threaded; no built-in thread safety

## QUIC

QUIC (RFC 9000, 2021) is the most capable of the four protocols. It was designed by Google for HTTP/3 and standardized by the IETF, combining transport, encryption, and multiplexing into a single protocol.

### What QUIC Solves

QUIC eliminates TCP's head-of-line blocking through independent streams. A lost packet on stream A blocks only stream A; streams B and C continue unaffected. Connection establishment takes 1 RTT (versus 2–3 for TCP + TLS 1.3), and returning clients can send data immediately with 0-RTT resumption.

Connection migration allows sessions to survive network changes (Wi-Fi to cellular) by identifying connections with IDs rather than IP/port tuples.

### Packet Format

QUIC uses two header formats:

| Header | Use | Size |
|--------|-----|------|
| Long header | Handshake (Initial, Handshake, 0-RTT) | ~20 bytes |
| Short header | Established connection data | ~12 bytes |

Every packet carries a 16-byte AEAD authentication tag for mandatory encryption. Total per-packet overhead on IPv4 is approximately 64 bytes (20 IP + 8 UDP + 12 QUIC + 16 AEAD + frame headers).

### Why Games Avoid It

QUIC is feature-rich but mismatched to game networking requirements:

**Mandatory encryption.** Every packet is encrypted with TLS 1.3. This consumes CPU cycles that games would rather spend on simulation. The per-packet AEAD operations add measurable CPU overhead compared to unencrypted UDP protocols.

**Reliability by default.** QUIC streams are reliable. RFC 9221 adds an unreliable datagram extension, but it is not universally implemented. Games need unreliable delivery as the default, not an afterthought.

**Implementation complexity.** Production QUIC implementations measure in the tens to hundreds of thousands of lines of code:

| Implementation | Language | Approximate Scale |
|----------------|----------|-------------------|
| quiche (Cloudflare) | Rust | Large codebase |
| ngtcp2 | C | Large codebase |
| MsQuic (Microsoft) | C | Production-grade, ships in Windows |

Compare this with ENet (~5,000 LOC) or netchan (1,400 LOC).

**Latency tradeoffs.** On fast networks, measurements show UDP+QUIC+HTTP/3 suffering up to 45% data rate reduction versus TCP+TLS+HTTP/2. The per-packet crypto and loss detection overhead isn't free.

**What games do not need.** Mandatory TLS certificates, sophisticated congestion control tuned for web traffic, stream-level flow control with credit-based limits, and connection migration across network interfaces. These are valuable for web browsers; they are dead weight for a 16-player game server.

QUIC's unreliable datagram extension (RFC 9221) narrows the gap, but the fundamental mismatch remains: QUIC optimizes for reliable, encrypted byte streams, while games optimize for low-latency, lossy state snapshots.

## Netchan

Netchan is a multiplexed UDP channel protocol implemented in 1,398 lines of C (139-line header, 1,398-line implementation) with no external dependencies beyond POSIX sockets. The companion source (`netchan.h`, `netchan.c`, `netchan_test.c`) builds with a single `make` invocation and runs nine protocol-level tests. It doesn't require tens of thousands of lines of code to provide per-channel reliability, flow control, fragmentation, and connection migration.

### Design Goals

1. **Multiplexed channels** over a single UDP connection, each with independent reliability
2. **Application-owned sockets** — the library never calls `sendto()` or `recvfrom()`
3. **No mandatory encryption** — the application can layer DTLS if needed
4. **Near-minimal implementation** — small enough to audit, embed, and modify

### Connection Lifecycle

Connections follow a five-state machine: NEW, CONNECTING, CONNECTED, CLOSING, CLOSED. The handshake is a two-frame exchange:

1. Client sends CONNECT_INIT (client ID + protocol version)
2. Server responds with CONNECT_ACCEPT (server ID + version + idle timeout)

Both sides generate random 32-bit connection IDs from `/dev/urandom`. The handshake retries up to 5 times at 500ms intervals. Keepalive PING/PONG frames maintain the connection and measure RTT at 5-second intervals, with a 30-second idle timeout.

The progression from QuakeWorld's 4-round challenge/response to netchan's 2-frame exchange reflects the shift from connectionless UDP (where the server cannot trust the source address without a challenge) to connection-ID-based protocols (where the random ID serves as a lightweight authentication token):

<img src="handshake-compare.svg" alt="Handshake comparison: QuakeWorld (4 rounds), ENet (3 commands), Netchan (2 frames)">

### Packet Format

Netchan uses two packet header formats:

| Header | Size | Fields |
|--------|------|--------|
| INIT | 5 bytes | flags (1) + connection ID (4) |
| Data | 7 bytes | flags (1) + remote connection ID (4) + packet sequence (2) |

Frames are packed sequentially after the header. Multiple frames share a single UDP packet, padded to the configured MTU (default 1,200 bytes). A typical data packet packs a DATA frame and an ACK frame into the same UDP datagram:

<img src="packet-layout.svg" alt="Netchan data packet layout: 7-byte header, DATA frame, ACK frame, padding to MTU">

The frame types:

| Frame | ID | Size | Purpose |
|-------|-----|------|---------|
| PADDING | 0x00 | 1 | Null fill |
| CONNECT_INIT | 0x01 | 6 | Client handshake |
| CONNECT_ACCEPT | 0x02 | 10 | Server handshake |
| CONNECT_REDIRECT | 0x03 | 12–24 | Server redirect |
| DISCONNECT | 0x04 | 3 | Graceful close |
| PING | 0x05 | 5 | Keepalive probe |
| PONG | 0x06 | 5 | Keepalive response |
| CHANNEL_OPEN | 0x07 | 5+ | Open a channel |
| CHANNEL_CLOSE | 0x08 | 4 | Close a channel |
| DATA | 0x09 | 8+ | Channel payload |
| ACK | 0x0A | 4 | Delivery confirmation |
| WINDOW_UPDATE | 0x0B | 6 | Flow control credit |

### Channel Model

Channels are the core abstraction. Each connection supports up to 256 channels, opened dynamically with a type, direction, and optional content-type string. Server-allocated channels get even IDs; client-allocated channels get odd IDs, preventing collisions.

Two channel types are implemented:

**Unreliable channels** deliver best-effort datagrams with no sequencing. Fragments are reassembled on arrival, but a lost fragment means a lost message. This is the right mode for position updates, input snapshots, and other data that expires quickly.

**Reliable channels** guarantee ordered delivery through:
- Per-channel 16-bit sequence numbers
- Explicit ACK frames
- A 64-slot reorder buffer for out-of-order packets
- Exponential backoff retransmission (100ms initial, doubling to 1,000ms max, 5 attempts)
- Sliding window flow control with WINDOW_UPDATE frames

### Fragmentation

Messages exceeding the per-packet payload (MTU minus headers, approximately 1,185 bytes at default MTU) are split into up to 32 fragments. Each fragment carries a sequence number, fragment index, and fragment total. The receiver tracks reassembly with a 32-bit bitmask across 4 concurrent fragment slots per channel.

### Flow Control

Reliable channels implement credit-based flow control:
- The receiver advertises a window size (default 64 KB) via WINDOW_UPDATE frames
- The sender tracks bytes in flight and blocks with `NETCHAN_ERR_FLOW` when the window is exhausted
- The receiver sends window updates as buffered data is consumed

This prevents a fast sender from overwhelming a slow receiver without requiring global bandwidth limits.

### Connection Migration

When a packet arrives from a different address than expected, netchan validates the migration by checking for DATA or ACK frames targeting known channels. Spoofed packets without valid channel context are rejected. On successful validation, the peer address is updated transparently.

### API Design

The API separates concerns cleanly. The application owns the UDP socket and drives timing:

```c
/* connection lifecycle */
struct netchan_conn *netchan_open(int server);
int netchan_connect(conn, addr, addrlen);
int netchan_accept(conn);
void netchan_close(conn);

/* packet I/O — application owns the socket */
uint32_t netchan_peek_id(pkt, len);          /* demux without parsing */
int netchan_feed(conn, pkt, len, from, fromlen);
size_t netchan_send_next(conn, buf, buflen, to, tolen);
int netchan_service(conn, now_ms);           /* service timers */

/* channels */
struct netchan_chan *netchan_chan_open(conn, type, dir, content_type);
int netchan_chan_write(ch, data, len);
int netchan_chan_read(ch, buf, buflen);

/* events */
int netchan_poll(conn, &ev);
```

The `netchan_peek_id()` function extracts the connection ID from a raw packet without full parsing, enabling the application to demultiplex packets across multiple connections using a single socket.

The library never allocates a socket, never calls `sendto()`, and never uses global state. The application calls `netchan_send_next()` in a loop to drain outgoing packets and `sendto()` them on its own socket. This makes netchan embeddable in any event loop.

### Test Coverage

Nine tests exercise the protocol end-to-end using loopback helpers that shuttle packets between two in-process connections:

| Test | Coverage |
|------|----------|
| Handshake | 3-way connection establishment |
| Unreliable datagram | Best-effort send/receive |
| Reliable datagram | ACK-confirmed delivery |
| Bidirectional channels | Simultaneous send/receive channels |
| Multiple messages | 10 reliable messages, ordered delivery |
| Connection migration | Address change mid-connection |
| Channel close | Graceful channel teardown |
| Peek ID | Packet demultiplexing utility |
| Graceful disconnect | Clean shutdown, no leaks |

## Comparison

### Protocol Features

| Feature | QuakeWorld | ENet | QUIC | Netchan |
|---------|-----------|------|------|---------|
| Year | 1996 | 2004 | 2021 | 2026 |
| Unreliable delivery | Yes | Yes | Extension (RFC 9221) | Yes |
| Reliable delivery | Yes (1 in flight) | Yes (sliding window) | Yes (per-stream) | Yes (per-channel) |
| Channels/streams | 1 | Up to 255 | Unlimited | Up to 256 |
| Fragmentation | No | Yes | Yes | Yes (32 frags) |
| Flow control | No | Throttle + bandwidth limit | Per-stream + connection | Per-channel window |
| Congestion control | No | RTT-based throttle | Pluggable (NewReno/BBR) | No |
| Encryption | No | No | Mandatory TLS 1.3 | No (layer externally) |
| Connection migration | No | No | Yes | Yes |
| IPv6 | No | Fork only (ENet6) | Yes | Socket-agnostic |

### Packet Overhead

| Protocol | Min Header | Per-payload overhead | Crypto overhead |
|----------|-----------|---------------------|-----------------|
| QuakeWorld | 10 bytes | None | None |
| ENet | 8 bytes (4 pkt + 4 cmd) | 4 bytes per command | None |
| QUIC (short header) | ~12 bytes | Frame headers | 16-byte AEAD tag |
| Netchan | 7 bytes | Frame type + length | None |

All figures exclude the 28-byte UDP/IPv4 (or 48-byte UDP/IPv6) header that every protocol shares.

### Implementation Complexity

| Metric | QuakeWorld | ENet | QUIC (quiche) | Netchan |
|--------|-----------|------|---------------|---------|
| Implementation LOC | ~400 | ~5,000 | ~100,000+ | 1,398 |
| Header LOC | Inline | ~400 | Thousands | 139 |
| External dependencies | None | None | TLS library | None |
| API functions | ~5 | ~50 | ~100+ | 20 |
| Build complexity | Part of engine | autotools/CMake | Cargo + system TLS | Single `cc` invocation |

### Reliability Mechanisms

| Aspect | QuakeWorld | ENet | QUIC | Netchan |
|--------|-----------|------|------|---------|
| In-flight reliable messages | 1 | Window-based | Per-stream window | 64 per channel |
| ACK mechanism | Piggybacked on data | Aggregated ACK commands | Dedicated ACK frames | Per-channel ACK frames |
| Retransmission | Next outgoing packet | Exponential backoff | PTO + loss detection | Exponential backoff (100ms–1s) |
| Reorder handling | Drop | Per-channel buffer | Per-stream | 64-slot reorder buffer |
| Max retries | Continuous | Configurable | Continuous (PTO) | 5 attempts, then channel dies |

### Suitability by Game Type

| Game Type | QuakeWorld | ENet | QUIC | Netchan |
|-----------|-----------|------|------|---------|
| Twitch FPS (2–16 players) | Good | Good | Overkill | Good |
| Cooperative PvE (2–8 players) | Limited | Good | Overkill | Good |
| Turn-based/strategy | Poor | Good | Viable | Good |
| MMO (1,000+ connections) | N/A | Possible | Possible | Possible |
| Web-based game | N/A | N/A | Natural fit | N/A |

## When to Use What

**Use QuakeWorld-style netchan** if the game is a fast-paced shooter where nearly all data is unreliable position/input snapshots, reliable messages are rare (connect, disconnect, map change), and the server is authoritative. The simplicity is the feature.

**Use ENet** if the game needs a proven library with an active community, multiple peer management built in, and configurable congestion control. ENet handles the common case well and has been shipped in real games for over twenty years.

**Use QUIC** if the game runs in a browser (via WebTransport), requires encryption for compliance or anti-cheat, or connects through corporate firewalls that block non-HTTPS UDP traffic. Accept the complexity and CPU cost as the price of those requirements.

**Use netchan** (or something like it) if the game needs multiplexed channels with per-channel reliability, the codebase must remain small enough to audit and modify, no external dependencies are acceptable, and the application wants full control over socket I/O and event loops. Netchan is a protocol you understand, not a dependency you install.

## Conclusion

The spectrum from QuakeWorld's 400-line netchan to QUIC's six-figure codebase reflects a fundamental tension in protocol design: generality costs complexity. QuakeWorld proved that a reliable UDP layer can be trivially simple when the application's needs are narrow; QUIC proved it can replace TCP+TLS when they're broad. ENet and netchan occupy the practical middle ground where most games live.

Netchan fits the features described above into 1,400 lines of C. It's not the right choice for every game, but it establishes a useful lower bound: if the networking layer is more complex than this and the game's requirements are simpler than QUIC's, something may have gone wrong.

The full source (`netchan.h`, `netchan.c`, `netchan_test.c`, and `Makefile`) is available as a companion download. Build with `make` and run `./netchan_test` to exercise all nine protocol tests.
