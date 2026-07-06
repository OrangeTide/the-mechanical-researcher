# netchan v2 — Folding netchan-ipx Gains Back Into netchan

Goal: update **netchan** (the POSIX/UDP library) so it is a **superset of
netchan-ipx's features minus the IPX protocol support**. That is: keep all of
netchan's richer protocol capability, add the portability/embeddability wins the
DOS/IPX work produced, and ship only a UDP transport backend (no IPX).

## The two projects are different in kind

| | netchan (2026-05-09) | netchan-ipx (2026-06-03) |
|---|---|---|
| Article | Comparison piece (QuakeWorld / ENet / QUIC / Netchan) | Build story: porting to 16-bit DOS / IPX |
| API prefix | `netchan_*`, 22 functions | `nc_*`, ~15 functions |
| Memory | `malloc` on demand | Static; one `calloc` at open, none during play |
| Transport | Hardwired `struct sockaddr` / POSIX UDP | Abstracted via `nc_addr`; pluggable IPX + UDP backends |
| Demo | `netchan_test.c` + `netchan_example.c` (protocol tests) | Full playable game (`thor.c`, Caves of Thor, 4-player) |

Code size (demo dirs):

- netchan: `netchan.c` 1437, `netchan.h` 160, `netchan_test.c` 668, `netchan_example.c` 409.
- netchan-ipx: `netchan.c` 664, `netchan.h` 156, plus `nc_addr.h`, `plat.h`,
  `nc_udp.c`, `nc_ipx.c`, and a whole game (`thor.c`, `game*.c`, `render.c`,
  `rng.c`, `shot.c`).

## The subtlety: netchan-ipx is mostly a *subset*, not a superset

On the wire protocol, netchan-ipx **deliberately drops** capability that netchan
already has (netchan-ipx's own article table states this):

| Dimension | netchan | netchan-ipx |
|---|---|---|
| Allocation | malloc on demand | static / one calloc at open |
| Framing | variable 5/7 B header + 12 TLV frame types | fixed 8 B header + homogeneous 6 B records |
| Reliability | dedicated ACK frames + 64-slot reorder buffer | cumulative ack piggybacked, Go-Back-N |
| Fragmentation | yes (up to 32 frags, 1200 B MTU) | none; max message ≈ one packet (~532 B) |
| Flow control | credit-based byte windows | fixed message-count window |
| Channels | up to 256, negotiated content type | fixed small set, no negotiation |
| Channel types | reliable / unreliable / stream | reliable / unreliable |
| Endianness | big-endian | big-endian (kept) |

So netchan already contains most of netchan-ipx's protocol capability, and more.
"Superset" cannot mean adopting netchan-ipx's protocol simplifications, those are
regressions. It means adopting the handful of things netchan-ipx has that netchan
genuinely **lacks**.

## What netchan-ipx has that netchan lacks (the actual delta)

1. **Transport abstraction.** `nc_addr` is an opaque, transport-agnostic address
   (`uint8_t len; uint8_t a[12]`) the core never interprets; each transport packs
   its own bytes. netchan instead bakes `struct sockaddr` / `socklen_t` straight
   into its public API (`netchan_connect`, `netchan_feed`, `netchan_send_next`).
   This is exactly the seam "minus IPX" applies to: keep the pluggable-transport
   design, ship only the UDP backend (`nc_udp.c`), drop the IPX backend
   (`nc_ipx.c`).

2. **Optional static / no-heap memory model.** Compile-time tunables
   (`NC_MTU`, `NC_WINDOW`, `NC_MAX_CHAN`, `NC_RECVQ`, `NC_UNREL_TXQ`,
   `NC_MAX_CONN`) let the whole thing live in BSS with no allocation after
   startup. netchan mallocs channels, per-message copies, reorder and outgoing
   queues on demand.

3. **Broadcast discovery.** A joining client sends its handshake SYN to a
   broadcast address; the host replies from its real unicast address, and the
   client locks onto it. netchan has a REDIRECT event for lobby handoff but no
   broadcast auto-discovery.

4. **A real playable game demo** (Caves-of-Thor style, server-authoritative,
   4-player) rather than just protocol tests.

## What "superset minus IPX" means concretely

Take the richer netchan and fold in the portability/embeddability gains from the
DOS work, keeping every netchan protocol feature (fragmentation, 64-slot reorder,
credit flow control, STREAM channels, stats/RTT, connection migration), and
dropping only the IPX transport backend:

- Introduce an `nc_addr`-style opaque address to replace baked-in `sockaddr` in
  the public API; provide a UDP backend that packs a sockaddr into it. No IPX
  backend.
- Add compile-time tunables so a static, no-malloc build is possible while the
  default POSIX build keeps dynamic allocation.
- Add broadcast discovery alongside the existing REDIRECT handoff.
- Optionally port the Caves-of-Thor demo to run on the updated netchan over UDP.

## Open scoping questions (unanswered)

- **Deliverable:** code-only update to `netchan/demo/`, code + revise
  `netchan/index.md` (via `revised:` frontmatter), or a new third topic dir for
  the merged library?
- **Feature scope:** which of the four deltas above are in scope (transport
  abstraction, static-alloc option, broadcast discovery, game demo)?
- **API naming:** keep `netchan_*` (netchan's) or converge on `nc_*`
  (netchan-ipx's shorter prefix)?
- **Wire compatibility:** the two are already wire-incompatible; confirm v2 keeps
  netchan's wire format (the capable one) and does not try to interoperate with
  the IPX subset.
