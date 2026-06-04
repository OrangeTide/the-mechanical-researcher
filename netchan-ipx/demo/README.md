# netchan-ipx demo

A reliable + unreliable multiplexed channel library (a subset of
[netchan](../../netchan/)) for **IPX on 16-bit MS-DOS**, plus a four-player,
server-authoritative game ("thor") with a Caves-of-Thor text-mode look.

The core is transport-agnostic. It builds on the host over UDP (for fast
iteration and unit tests) with `gcc`, and on DOS over IPX with Open Watcom.

## Layout

| File | What it is |
|---|---|
| `netchan.c` / `netchan.h` | transport-agnostic core: handshake, channels, Go-Back-N reliability |
| `nc_addr.h` | opaque transport address (UDP 6 B, IPX 12 B) |
| `nc_udp.c` | host UDP transport |
| `nc_ipx.c` / `nc_ipx.h` | 16-bit DOS IPX transport (INT 2Fh, polled ECBs, static pools) |
| `game.c` / `game.h` | portable server-authoritative game simulation |
| `game_net.c` / `game_net.h` | the game's wire protocol and multi-peer server glue |
| `render.c`, `plat_*.c`, `plat.h` | text-mode rendering and platform layer (DOS + host) |
| `thor.c` | the game program (host-and-join) |
| `rng.c` / `rng.h` | tiny PRNG used for map and creature generation |
| `test_*.c` | host unit tests |

## Prerequisites

- A C compiler (`gcc`) for the host build and tests.
- [Open Watcom](https://github.com/open-watcom/open-watcom-v2) for the DOS
  build, with the `WATCOM` environment variable set and `$WATCOM/binl` on
  `PATH` (provides `wcc`, `wcl`, `wmake`).
- [DOSBox](https://www.dosbox.com/) to run the DOS binaries.
- For networked play: an IPX-over-UDP relay that speaks the DOSBox tunnel
  protocol, e.g. [ipxrelay](https://www.kernel.org/pub/software/network/ipxrelay/).

## Build

Host (development + tests):

```sh
make            # builds the host tools and the game (thor) over UDP
make check      # runs the host unit tests
```

DOS (Open Watcom):

```sh
wmake           # builds thor.exe, ipxtest.exe, ncdemo.exe (16-bit, large model)
```

The two build systems coexist in this directory: GNU `make` reads
`GNUmakefile` (host), Open Watcom `wmake` reads `makefile` (DOS). Host objects
are `*.o`, DOS objects are `*.obj`, so they never collide.

## Run the unit tests (host)

```sh
./test_netchan   # handshake, reliable delivery under induced loss, unreliable
./test_gnet      # map streaming, state sync, input path, chat round-trip
./test_game      # prints ASCII frames of the simulation
```

## Play it over IPX (DOSBox + relay)

1. **Start the relay** on the host, e.g. on UDP port 19900:

   ```sh
   ipxrelay -F -p 19900 -a 127.0.0.1
   ```

2. **Configure each DOSBox** with IPX enabled, a fixed CPU cycle count, and an
   autoexec that connects to the relay and launches the game. A minimal
   `dosbox.conf` for the host player:

   ```ini
   [cpu]
   cycles=20000
   [ipx]
   ipx=true
   [autoexec]
   mount c /path/to/demo
   c:
   ipxnet connect 127.0.0.1 19900
   thor.exe s
   exit
   ```

   For a joining player, use `thor.exe` (no `s`) instead of `thor.exe s`.

   A fixed `cycles` value matters: `cycles=max` makes two co-running instances
   starve each other.

3. **Run two (or more) DOSBox instances.** Start the host first, then the
   joiners. Each joiner discovers the host with an IPX broadcast.

### Controls

- **Arrow keys** &#8212; move in eight directions (hold to keep moving)
- **Z or Space** &#8212; fire
- **Enter** &#8212; open the chat line; type, then Enter to send (Esc cancels)
- **Q or Esc** &#8212; quit

## Test harness scripts

These automate the relay and a headless two-instance match; they only manage
the processes they start.

```sh
scripts/relay.sh start|stop|restart|status   # manage the test relay
sh scripts/ipxtest.sh pair ipxtest.exe s     # reliability test over IPX (PASS/FAIL)
sh scripts/playtest.sh                        # run the game headless; dumps both screens
```

`thor.exe` accepts a `dump` argument that writes the current 40&#215;25 screen to
a text file once a second, which is how the headless harness and the article
screenshot are captured.

## Made by a machine. PUBLIC DOMAIN (CC0-1.0)
