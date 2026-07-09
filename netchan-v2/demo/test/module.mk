# test -- every test lives here, named test_<thing>.
#
# The socketless tests (protocol core, WebSocket codec, IDL round-trip, game
# wire packing) build and run under native and wasm alike; the socket and
# crypto tests are native only. Library dependencies are resolved by name
# across modules, so a test can link a lib defined under game/ or third_party/.

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

# --- protocol loopback test (socketless: builds and runs everywhere) ---
EXECUTABLES += test_netchan
test_netchan_DIR := $(ROOT)
test_netchan_SRCS = test_netchan.c
test_netchan_LIBS = netchan_core
# Under emscripten, compile the wasm synchronously so `node test.js` loads it
# from the filesystem instead of via fetch() (which node's global fetch breaks).
test_netchan_LDFLAGS.Emscripten = -sWASM_ASYNC_COMPILATION=0
define test_netchan_TESTCMD
$(test_netchan_RUN)
endef
TEST_TARGETS += test_netchan

# --- WebSocket codec test: RFC 6455 known-answer + frame round-trip ---
EXECUTABLES += test_nc_ws
test_nc_ws_DIR := $(ROOT)
test_nc_ws_SRCS = test_nc_ws.c
test_nc_ws_LIBS = nc_ws
test_nc_ws_LDFLAGS.Emscripten = -sWASM_ASYNC_COMPILATION=0
define test_nc_ws_TESTCMD
$(test_nc_ws_RUN)
endef
TEST_TARGETS += test_nc_ws

# --- IDL round-trip test (socketless) ---
EXECUTABLES += test_proto
test_proto_DIR := $(ROOT)
test_proto_SRCS = test_proto.c
test_proto_LIBS = proto
test_proto_LDFLAGS.Emscripten = -sWASM_ASYNC_COMPILATION=0
define test_proto_TESTCMD
$(test_proto_RUN)
endef
TEST_TARGETS += test_proto
# test_proto.c includes the game's generated proto.h; on a clean build wait for
# it (PROTO_GEN_H is defined in game/module.mk, loaded before this file).
$(BUILDDIR)/$(ROOT)test_proto.o : | $(PROTO_GEN_H)

# --- game sim + snapshot round-trip test (socketless) ---
EXECUTABLES += test_game_wire
test_game_wire_DIR := $(ROOT)
test_game_wire_SRCS = test_game_wire.c
test_game_wire_LIBS = game_core
test_game_wire_LDFLAGS.Emscripten = -sWASM_ASYNC_COMPILATION=0
define test_game_wire_TESTCMD
$(test_game_wire_RUN)
endef
TEST_TARGETS += test_game_wire

# --- socket and crypto tests: native only ---
ifneq ($(_TARGET_OS),Emscripten)

# real-socket test: drives nc_udp over two live loopback UDP sockets
EXECUTABLES += test_nc_udp
test_nc_udp_DIR := $(ROOT)
test_nc_udp_SRCS = test_nc_udp.c
test_nc_udp_LIBS = netchan_core nc_udp
define test_nc_udp_TESTCMD
$(test_nc_udp_RUN)
endef
TEST_TARGETS += test_nc_udp

# encrypted-session test: full reliable session through the cipher, plus
# tamper and replay rejection
EXECUTABLES += test_nc_crypto
test_nc_crypto_DIR := $(ROOT)
test_nc_crypto_SRCS = test_nc_crypto.c
test_nc_crypto_LIBS = netchan_core nc_crypto
define test_nc_crypto_TESTCMD
$(test_nc_crypto_RUN)
endef
TEST_TARGETS += test_nc_crypto

endif
