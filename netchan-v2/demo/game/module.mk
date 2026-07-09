# netchan-v2 game demo -- Caves-of-Thor over netchan

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

# --- transport-agnostic simulation core (deterministic on the server) ---
LIBRARIES += game_core
game_core_DIR := $(ROOT)
game_core_SRCS = game.c rng.c game_wire.c
game_core_EXPORTED_CPPFLAGS = -I$(game_core_DIR)

# --- generated game wire messages (microser IDL) ---
# proto.c and proto.h are generated from proto.idl into the build tree, never
# the work tree. Dependents reach the generated proto.h via the exported
# -I$(BUILDDIR)/$(proto_DIR) and microser.h via -I to third_party.
LIBRARIES += proto
proto_DIR := $(ROOT)
proto_GENERATED_SRCS = proto.c
proto_CPPFLAGS          = -I$(TOP)third_party
proto_EXPORTED_CPPFLAGS = -I$(BUILDDIR)/$(proto_DIR) -I$(TOP)third_party

# microser-gen.sh writes "$2.c" and "$2.h" and hardcodes #include "$2.h", so
# it must run with a bare basename from inside the output directory.
PROTO_GEN_H := $(BUILDDIR)/$(proto_DIR)proto.h
$(BUILDDIR)/$(proto_DIR)proto.c $(PROTO_GEN_H) &: \
		$(proto_DIR)proto.idl $(TOP)third_party/microser-gen.sh
	@mkdir -p $(BUILDDIR)/$(proto_DIR)
	cd $(BUILDDIR)/$(proto_DIR) && \
	  $(abspath $(TOP)third_party/microser-gen.sh) $(abspath $(proto_DIR)proto.idl) proto

# Clean-build ordering: objects that #include the generated proto.h must not
# compile before it exists. Make records this in the .dep files after the first
# build, but the first build needs the order-only prerequisite spelled out
# (modular-make does not auto-wire a generated header across target boundaries).
# test/module.mk adds test_proto.o to the same PROTO_GEN_H order-only barrier.
$(addprefix $(BUILDDIR)/$(proto_DIR),game_server.o game_client.o game_play.o ws_client.o) : | $(PROTO_GEN_H)

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
