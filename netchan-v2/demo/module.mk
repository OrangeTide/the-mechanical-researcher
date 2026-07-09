# netchan-v2 -- modular-make build descriptor (native + WebAssembly)
#
# The top level is the netchan library itself: the transport-agnostic core,
# and the three transport backends. Everything that merely *uses* netchan
# lives in a subdirectory:
#
#   third_party/  vendored libraries (monocypher); see VENDORING.md
#   examples/     the chat example and the WebSocket-to-UDP gateway
#   test/         every test (test_*), socketless ones also build to wasm
#   game/         the Caves-of-Thor demo game and its wire schema (proto)
#
# Native:  make               (builds every target to _out/<triplet>/bin)
#          make run-tests      (runs the socketless + socket test targets)
# Wasm:    make CC=emcc CXX=em++
#          socket-only targets drop out automatically; the core and its
#          socketless tests compile to wasm because the core has no sockets.

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
# game before test: test/ references the proto codegen path game/ defines.
SUBDIRS = third_party game examples test

# --- the transport-agnostic protocol core (no socket headers) ---
LIBRARIES += netchan_core
netchan_core_DIR := $(ROOT)
netchan_core_SRCS = netchan.c
netchan_core_EXPORTED_CPPFLAGS = -I$(netchan_core_DIR)

# --- UDP transport backend (the only code that knows sockaddr) ---
LIBRARIES += nc_udp
nc_udp_DIR := $(ROOT)
nc_udp_SRCS = nc_udp.c
nc_udp_EXPORTED_CPPFLAGS = -I$(nc_udp_DIR)

# --- WebSocket framing/handshake codec (no sockets, no external libs) ---
# Portable enough to compile anywhere, but only the native gateway and test
# client use it, so it is built for native alongside them.
LIBRARIES += nc_ws
nc_ws_DIR := $(ROOT)
nc_ws_SRCS = nc_ws.c
nc_ws_EXPORTED_CPPFLAGS = -I$(nc_ws_DIR)

# --- encrypted UDP transport decorator: desktop only ---
# Browsers get transport encryption for free (WebRTC DTLS, wss), so the
# crypto backend never compiles into the wasm build. Its one dependency,
# monocypher, is a vendored library built under third_party/.
ifneq ($(_TARGET_OS),Emscripten)
LIBRARIES += nc_crypto
nc_crypto_DIR := $(ROOT)
nc_crypto_SRCS = nc_crypto.c
nc_crypto_LIBS = monocypher
nc_crypto_EXPORTED_CPPFLAGS = -I$(nc_crypto_DIR)
endif
