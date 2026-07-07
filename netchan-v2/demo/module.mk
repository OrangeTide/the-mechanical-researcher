# netchan-v2 demo -- modular-make build descriptor (native + WebAssembly)
#
# Native:  make               (builds every target to _out/<triplet>/bin)
#          make run-tests      (runs the socketless test targets)
# Wasm:    make CC=emcc CXX=em++
#          the socket example is dropped automatically; the protocol core
#          and its tests compile to wasm because the core has no sockets.

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

# --- the transport-agnostic protocol core (no socket headers) ---
LIBRARIES += netchan_core
netchan_core_DIR := $(ROOT)
netchan_core_SRCS = netchan.c
netchan_core_EXPORTED_CPPFLAGS = -I$(ROOT)

# --- generated game wire messages (microser IDL) ---
LIBRARIES += proto
proto_DIR := $(ROOT)
proto_SRCS = proto.c
proto_EXPORTED_CPPFLAGS = -I$(ROOT)

# proto.c and proto.h are generated from the schema; both land in the
# source dir so a plain "cc" build and the wasm build share them.
$(ROOT)proto.c $(ROOT)proto.h &: $(ROOT)proto.idl $(ROOT)microser-gen.sh
	$(ROOT)microser-gen.sh $(ROOT)proto.idl $(ROOT)proto

# --- protocol loopback tests (socketless: build and run everywhere) ---
EXECUTABLES += netchan_test
netchan_test_DIR := $(ROOT)
netchan_test_SRCS = netchan_test.c
netchan_test_LIBS = netchan_core
# Under emscripten, compile the wasm synchronously so `node test.js` loads it
# from the filesystem instead of via fetch() (which node's global fetch breaks).
netchan_test_LDFLAGS.Emscripten = -sWASM_ASYNC_COMPILATION=0
define netchan_test_TESTCMD
$(netchan_test_RUN)
endef
TEST_TARGETS += netchan_test

# --- IDL round-trip test (socketless) ---
EXECUTABLES += proto_test
proto_test_DIR := $(ROOT)
proto_test_SRCS = proto_test.c
proto_test_LIBS = proto
proto_test_LDFLAGS.Emscripten = -sWASM_ASYNC_COMPILATION=0
define proto_test_TESTCMD
$(proto_test_RUN)
endef
TEST_TARGETS += proto_test

# --- chat example over real UDP sockets: native only ---
# Emscripten has no BSD sockets, so this target is excluded there. That
# it can be excluded without touching the core is the whole point of the
# nc_addr transport seam.
ifneq ($(_TARGET_OS),Emscripten)
EXECUTABLES += netchan_example
netchan_example_DIR := $(ROOT)
netchan_example_SRCS = netchan_example.c nc_udp.c
netchan_example_LIBS = netchan_core

# real-socket test: drives nc_udp over two live loopback UDP sockets
EXECUTABLES += nc_udp_test
nc_udp_test_DIR := $(ROOT)
nc_udp_test_SRCS = nc_udp_test.c nc_udp.c
nc_udp_test_LIBS = netchan_core
define nc_udp_test_TESTCMD
$(nc_udp_test_RUN)
endef
TEST_TARGETS += nc_udp_test
endif

# --- encrypted UDP transport decorator: desktop only ---
# Browsers get transport encryption for free (WebRTC DTLS, wss), so the
# crypto backend never compiles into the wasm build.
ifneq ($(_TARGET_OS),Emscripten)
LIBRARIES += nc_crypto
nc_crypto_DIR := $(ROOT)
nc_crypto_SRCS = nc_crypto.c monocypher.c
nc_crypto_EXPORTED_CPPFLAGS = -I$(ROOT)

EXECUTABLES += nc_crypto_test
nc_crypto_test_DIR := $(ROOT)
nc_crypto_test_SRCS = nc_crypto_test.c
nc_crypto_test_LIBS = netchan_core nc_crypto
define nc_crypto_test_TESTCMD
$(nc_crypto_test_RUN)
endef
TEST_TARGETS += nc_crypto_test
endif
