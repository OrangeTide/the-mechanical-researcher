# netchan-crypto -- modular-make build descriptor (native only).
#
# This demo is deliberately socket-bound: it shows the encrypted UDP
# transport running under a real event loop, so there is no wasm target.
#
#   third_party/  vendored monocypher (X25519 + XChaCha20-Poly1305)
#   iox/          the vendored event loop (poll fds, timers, signals)
#
# Build:  make                 (echo_server, echo_client, test_secure_link)
#         make run-tests       (loopback echo round-trip over real sockets)

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
SUBDIRS = third_party iox

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

# --- encrypted UDP transport decorator (X25519 + XChaCha20-Poly1305) ---
LIBRARIES += nc_crypto
nc_crypto_DIR := $(ROOT)
nc_crypto_SRCS = nc_crypto.c
nc_crypto_LIBS = monocypher
nc_crypto_EXPORTED_CPPFLAGS = -I$(nc_crypto_DIR)

# --- secure_link: glue that drives netchan + nc_crypto over the iox loop ---
LIBRARIES += secure_link
secure_link_DIR := $(ROOT)
secure_link_SRCS = secure_link.c
secure_link_LIBS = netchan_core nc_udp nc_crypto iox
secure_link_EXPORTED_CPPFLAGS = -I$(secure_link_DIR)

# --- the two demo programs ---
EXECUTABLES += echo_server
echo_server_DIR := $(ROOT)
echo_server_SRCS = echo_server.c
echo_server_LIBS = secure_link netchan_core nc_udp nc_crypto iox monocypher

EXECUTABLES += echo_client
echo_client_DIR := $(ROOT)
echo_client_SRCS = echo_client.c
echo_client_LIBS = secure_link netchan_core nc_udp nc_crypto iox monocypher

# --- self-contained loopback test: encrypted echo round-trip ---
EXECUTABLES += test_secure_link
test_secure_link_DIR := $(ROOT)
test_secure_link_SRCS = test_secure_link.c
test_secure_link_LIBS = secure_link netchan_core nc_udp nc_crypto iox monocypher
define test_secure_link_TESTCMD
$(test_secure_link_RUN)
endef
TEST_TARGETS += test_secure_link
