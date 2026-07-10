# netchan-v2 game demo -- Caves-of-Thor over netchan

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

# --- transport-agnostic simulation core (deterministic on the server) ---
LIBRARIES += game_core
game_core_DIR := $(ROOT)
game_core_SRCS = game.c rng.c game_wire.c
game_core_EXPORTED_CPPFLAGS = -I$(game_core_DIR)

# --- generated game wire messages (microser IDL) ---
# proto.c and proto.h are generated from proto.idl into the build tree, never
# the work tree. Declaring proto.h in _GENERATED_HDRS makes modular-make order
# it ahead of every consumer's objects (this library's and, transitively, every
# dependent's) and put its build directory on their include path, so dependents
# just #include "proto.h" with no manual -I or order-only prerequisite.
LIBRARIES += proto
proto_DIR := $(ROOT)
proto_GENERATED_SRCS = proto.c
proto_GENERATED_HDRS = proto.h
proto_CPPFLAGS          = -I$(TOP)third_party
proto_EXPORTED_CPPFLAGS = -I$(TOP)third_party

# microser-gen.sh writes "$2.c" and "$2.h" and hardcodes #include "$2.h", so
# it must run with a bare basename from inside the output directory (which
# modular-make creates for the generated files before this recipe runs).
$(BUILDDIR)/$(proto_DIR)proto.c $(BUILDDIR)/$(proto_DIR)proto.h &: \
		$(proto_DIR)proto.idl $(TOP)third_party/microser-gen.sh
	cd $(BUILDDIR)/$(proto_DIR) && \
	  $(abspath $(TOP)third_party/microser-gen.sh) $(abspath $(proto_DIR)proto.idl) proto

# --- server and native client over real UDP sockets: native only ---
ifneq ($(_TARGET_OS),Emscripten)
EXECUTABLES += game_server
game_server_DIR := $(ROOT)
game_server_SRCS = game_server.c
game_server_LIBS = game_core netchan_core nc_udp proto

EXECUTABLES += game_client
game_client_DIR := $(ROOT)
game_client_SRCS = game_client.c
game_client_LIBS = game_core netchan_core nc_udp proto

# playable terminal client (twin-stick controls, curses-free ANSI render)
EXECUTABLES += game_play
game_play_DIR := $(ROOT)
game_play_SRCS = game_play.c render.c plat_host.c
game_play_LIBS = game_core netchan_core nc_udp proto

# headless client over the WebSocket gateway: the browser's path, in C, so it
# can be tested without a browser (and proves a WS client shares the server)
EXECUTABLES += ws_client
ws_client_DIR := $(ROOT)
ws_client_SRCS = ws_client.c
ws_client_LIBS = game_core netchan_core nc_ws proto
endif
