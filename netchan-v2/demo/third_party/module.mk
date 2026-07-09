# third_party -- vendored libraries. See ../VENDORING.md for provenance.
#
# Only monocypher has a .c to compile; microser is a header-only runtime
# (microser.h) plus a codegen script (microser-gen.sh) the game invokes.

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

# --- Monocypher: X25519 + XChaCha20-Poly1305 for the nc_crypto backend ---
# Desktop only, so it is excluded from the wasm build (nothing links it there).
ifneq ($(_TARGET_OS),Emscripten)
LIBRARIES += monocypher
monocypher_DIR := $(ROOT)
monocypher_SRCS = monocypher.c
monocypher_EXPORTED_CPPFLAGS = -I$(monocypher_DIR)
endif
