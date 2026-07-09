# examples -- programs that use netchan (native only; both need sockets).

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

ifneq ($(_TARGET_OS),Emscripten)

# chat example over real UDP sockets
EXECUTABLES += netchan_example
netchan_example_DIR := $(ROOT)
netchan_example_SRCS = netchan_example.c
netchan_example_LIBS = netchan_core nc_udp

# browser-to-UDP gateway: terminates WebSocket clients and relays them onto
# the unmodified UDP game server as ordinary peers
EXECUTABLES += ws_gateway
ws_gateway_DIR := $(ROOT)
ws_gateway_SRCS = ws_gateway.c
ws_gateway_LIBS = nc_ws

endif
