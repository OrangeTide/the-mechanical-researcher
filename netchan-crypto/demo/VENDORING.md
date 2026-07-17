# Vendored code

This demo vendors two things so it builds from a bare checkout with no package
manager. Provenance and licences are recorded here.

## Monocypher (`third_party/`)

- **Files:** `monocypher.c`, `monocypher.h`
- **What:** a small, auditable, single-file crypto library. Supplies X25519,
  XChaCha20-Poly1305, and BLAKE2b for `nc_crypto` (and the demo's PSK
  derivation).
- **Why it, not libsodium:** it is public domain, one translation unit, and has
  no build system of its own, which is the whole point of a vendorable demo.
- **Licence:** dual CC0-1.0 / BSD-2-Clause. See `third_party/LICENCE-monocypher.md`.

## iox (`iox/`)

- **Files:** `iox_loop.c`, `iox_fd.c`, `iox_signal.c`, `iox_timer.c`, their
  headers, and the header-only priority queue `pq.h`.
- **What:** a compact `poll()`-based event loop: file-descriptor watchers,
  one-shot timers backed by a binary heap, and signal delivery over a
  self-pipe. It is the same event loop the *lumi* terminal workspace uses.
- **Licence:** MIT-0 OR Public Domain (headers carry the notice).

## netchan itself

`netchan.c/.h`, `nc_addr.h`, `nc_udp.c/.h`, and `nc_crypto.c/.h` are copied
verbatim from the netchan-v2 demo. They are the subject of the article, not a
third-party dependency, so they live at the top level rather than under
`third_party/`.
