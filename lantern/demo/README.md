# Lantern Skeleton

A starting skeleton for **Lantern**, a static reliable/unreliable channel
library for FreePascal, designed in the article
[Lantern: A Static Reliable-Channel Library for FreePascal](../). This is the
scaffolding, not a finished library: the load-bearing pieces are implemented
and tested, and the protocol state machines are laid out as honest stubs that
say what they still owe.

## What Works Today

- **The static arena** (`LanternArena`, `LanternCore`): one `GetMem` at startup,
  sized from a start-time `TLanternConfig`, carved into a connection freelist and
  per-channel buffers. No heap traffic after creation.
- **The admission cookie** (`LanternCore`): the stateless return-routability
  token, `SipHash-2-4(secret, addr | nonce | epoch)`, with two-generation key
  rotation. This is the anti-amplification, anti-DoS gate from the article.
- **The wire codec** (`LanternWire`): little-endian put/get over a bounded
  cursor with a sticky error flag, the runtime the generated serializers use.
- **The IDL tool** (`lanterngen`): a complete lexer and recursive-descent parser
  for the Pascal-flavored schema language, plus an emitter that writes real
  FreePascal record types. The serializer bodies are stubbed.

## What Is Stubbed

- `LanternFeed` / `LanternSendNext` / `LanternService`: the handshake, channel
  windows, acks, retransmission, and keepalive/idle timers. The signatures and
  the clock discipline (every entry point takes the caller's `nowMs`) are in
  place.
- `LanternCrypto` / `LanternAuth`: the X25519/XChaCha20 decorator and the host-key
  login, awaiting a Monocypher binding (compiled in only under
  `-dLANTERN_WITH_MONOCYPHER`).
- `LanternUDP`: the Windows (winsock2) path mirrors the compile-tested Unix path
  and is marked for verification.
- `lanterngen`'s serializer code generation, a mechanical walk of the AST it
  already builds.

## Prerequisites

- **FreePascal** 3.2.0 or newer (`fpc` on `PATH`). Install from
  <https://www.freepascal.org/> or a package manager (`apt install fpc`,
  `brew install fpc`, or the Windows installer). Nothing else is required; the
  skeleton has no external dependencies.

## Build and Run

```sh
make            # compile the smoke test and the lanterngen tool into build/
make check      # run the smoke test
make gen        # compile examples/game.pidl to a FreePascal unit and print it
make clean      # remove build/ and the generated unit
```

`make check` exercises the parts that are implemented and should print a list of
passing checks ending in `All checks passed.`, including the arena sizes for 8,
32, and 128 players and three cookie forgery-rejection cases.

`make gen` runs the IDL compiler on the example schema `examples/game.pidl` and
writes `examples/GameProto.pas`, a unit of `packed record` types you can compile
directly:

```sh
fpc -Mobjfpc -Fusrc examples/GameProto.pas
```

## Layout

```
src/lanterntypes.pas   constants, config record, opaque address
src/lanternarena.pas   the one-block bump allocator
src/lanternwire.pas    little-endian wire codec
src/lanterncore.pas    server/connection/channel, freelist, cookie, stubs
src/lanterncrypto.pas  encryption decorator (stub)
src/lanternauth.pas    host-key and login (stub)
src/lanternudp.pas     optional cross-platform UDP helper
tools/lanterngen.pas   the Pascal IDL compiler
examples/game.pidl    an example schema
examples/smoke.pas    the sanity test
```

The unit files use flat names (`LanternCore`, not `Lantern.Core`) so the build is
one `fpc` call with `-Fusrc` and no namespace configuration, which keeps CI on
Linux, macOS, and Windows identical.
