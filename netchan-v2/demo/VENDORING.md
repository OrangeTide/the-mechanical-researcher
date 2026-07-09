# Vendoring netchan

netchan is meant to be dropped into another project. The top level of this
directory is only the files you need to *use* it; everything else (tests,
examples, the demo game, third-party code) sits in a subdirectory. This note
says what netchan itself needs, what the optional pieces pull in, and where to
point the build at your own copies.

## What netchan is

Copy these into your project. This is the whole library:

| File | What it is | Needs |
|---|---|---|
| `netchan.c` / `netchan.h` / `nc_addr.h` | the transport-agnostic protocol core | nothing external |
| `nc_udp.c` / `nc_udp.h` | the UDP backend (the only code that knows `sockaddr`) | a POSIX socket API |
| `nc_ws.c` / `nc_ws.h` | a dependency-free WebSocket codec (RFC 6455 handshake + framing) | nothing external |
| `nc_crypto.c` / `nc_crypto.h` | the encrypted-UDP decorator (desktop) | **Monocypher** (below) |

The core plus `nc_udp` and `nc_ws` have **no third-party dependency at all**: a
plain `cc netchan.c nc_udp.c your_app.c` links. `nc_crypto` is the only part
that reaches outside, and only for one small library.

## Third-party libraries

They live in `third_party/` and are copied in unmodified. You supply your own
copies and point the build at them.

| Library | Version | Where to get it | Needed for |
|---|---|---|---|
| [Monocypher](https://monocypher.org) | 4.0.2 | monocypher.org (public domain / CC0) | `nc_crypto` only: X25519 + XChaCha20-Poly1305. See `third_party/LICENCE-monocypher.md`. |
| microser | — | `serialization-formats/demo/` in this repo | regenerating the **game's** wire code only; not part of netchan |
| modular-make (`GNUmakefile`) | 1.6.0 | the `modular-make` project | the multi-target build driver; optional, see below |

microser is two files: `microser.h` (a heap-free, zero-copy encode/decode
runtime) and `microser-gen.sh` (an `awk` IDL compiler). They matter only if you
build the demo game, whose wire schema (`game/proto.idl`) is compiled to C by
`microser-gen.sh`. netchan does not use microser.

## How to point the build at your copies

There are two build fronts, and both take an override.

**`Makefile.simple`** (plain `cc`, no driver) has a `THIRD_PARTY` variable:

    make -f Makefile.simple                       # uses ./third_party
    make -f Makefile.simple THIRD_PARTY=/path/to/libs

Only the `test_nc_crypto` target reads it (to find `monocypher.c`); the core and
`netchan_example` targets need no third-party path.

**`module.mk`** (the modular-make build) builds Monocypher as a library under
`third_party/module.mk`. To use a copy elsewhere, edit `monocypher_DIR` there:

    monocypher_DIR := /path/to/your/monocypher/

The game's `proto` codegen invokes `$(TOP)third_party/microser-gen.sh`; change
that path in `game/module.mk` if your `microser-gen.sh` lives elsewhere.

## Generated files are not checked in

`proto.c` / `proto.h` are generated from `game/proto.idl` by `microser-gen.sh`
and land in the build tree (`_build/<triplet>/game/`), never the work tree. The
modular-make build generates them automatically; the standalone web build
scripts (`game/build-web*.sh`) generate them into a scratch `game/.gen/`. To
generate a copy by hand:

    cd third_party && ./microser-gen.sh ../game/proto.idl /tmp/proto   # -> /tmp/proto.{c,h}

## Dropping the build driver entirely

`GNUmakefile` is the vendored modular-make driver. If you would rather not carry
it, `Makefile.simple` builds the core and the UDP example with nothing but a C
compiler, and is the model for wiring netchan into your own build system.

All machine-authored files here are: Made by a machine. PUBLIC DOMAIN (CC0-1.0).
