# third_party -- vendored libraries. See ../VENDORING.md for provenance.
#
# Monocypher provides X25519, XChaCha20-Poly1305, and BLAKE2b for the
# nc_crypto backend (and the demo's PSK derivation).

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

LIBRARIES += monocypher
monocypher_DIR := $(ROOT)
monocypher_SRCS = monocypher.c
monocypher_EXPORTED_CPPFLAGS = -I$(monocypher_DIR)
