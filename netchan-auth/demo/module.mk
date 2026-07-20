# netchan-auth -- modular-make build descriptor (native only).
#
# The stack, bottom to top:
#
#   third_party/  vendored monocypher (X25519, Ed25519, AEAD, BLAKE2b, Argon2)
#   iox/          the vendored event loop (poll fds, timers, signals)
#   nc_crypto     transport decorator: secrecy plus the server's identity key
#   netchan_core  the reliable-channel protocol, oblivious to both
#   nc_auth       the login conversation, oblivious to its transport
#   auth_link     the only file that has to see all four at once
#
# Build:  make                 (auth_server, auth_client, nc_keygen, tests)
#         make run-tests       (login state machine, keystore, loopback echo)

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

# --- encrypted transport decorator, now with static identity keys ---
LIBRARIES += nc_crypto
nc_crypto_DIR := $(ROOT)
nc_crypto_SRCS = nc_crypto.c
nc_crypto_LIBS = monocypher
nc_crypto_EXPORTED_CPPFLAGS = -I$(nc_crypto_DIR)

# --- the login conversation: no socket, no netchan, no event loop ---
LIBRARIES += nc_auth
nc_auth_DIR := $(ROOT)
nc_auth_SRCS = nc_auth.c
nc_auth_LIBS = monocypher
nc_auth_EXPORTED_CPPFLAGS = -I$(nc_auth_DIR)

# --- on-disk keys and credentials, in the shape ssh uses ---
LIBRARIES += keystore
keystore_DIR := $(ROOT)
keystore_SRCS = keystore.c
keystore_LIBS = monocypher
keystore_EXPORTED_CPPFLAGS = -I$(keystore_DIR)

# --- terminal and socket odds and ends the demo programs share ---
LIBRARIES += demoutil
demoutil_DIR := $(ROOT)
demoutil_SRCS = prompt.c sockutil.c
demoutil_LIBS = nc_udp
demoutil_EXPORTED_CPPFLAGS = -I$(demoutil_DIR)

# --- auth_link: netchan + nc_crypto + nc_auth, driven by the iox loop ---
LIBRARIES += auth_link
auth_link_DIR := $(ROOT)
auth_link_SRCS = auth_link.c
auth_link_LIBS = netchan_core nc_udp nc_crypto nc_auth iox
auth_link_EXPORTED_CPPFLAGS = -I$(auth_link_DIR)

NC_LIBS = auth_link netchan_core nc_udp nc_crypto nc_auth keystore demoutil \
          iox monocypher

# --- the demo programs ---
EXECUTABLES += auth_server
auth_server_DIR := $(ROOT)
auth_server_SRCS = auth_server.c
auth_server_LIBS = $(NC_LIBS)

EXECUTABLES += auth_client
auth_client_DIR := $(ROOT)
auth_client_SRCS = auth_client.c
auth_client_LIBS = $(NC_LIBS)

EXECUTABLES += nc_keygen
nc_keygen_DIR := $(ROOT)
nc_keygen_SRCS = nc_keygen.c
nc_keygen_LIBS = keystore demoutil nc_udp monocypher

# --- tests ---
EXECUTABLES += test_nc_auth
test_nc_auth_DIR := $(ROOT)
test_nc_auth_SRCS = test_nc_auth.c
test_nc_auth_LIBS = nc_auth monocypher
define test_nc_auth_TESTCMD
$(test_nc_auth_RUN)
endef
TEST_TARGETS += test_nc_auth

EXECUTABLES += test_keystore
test_keystore_DIR := $(ROOT)
test_keystore_SRCS = test_keystore.c
test_keystore_LIBS = keystore monocypher
define test_keystore_TESTCMD
$(test_keystore_RUN)
endef
TEST_TARGETS += test_keystore

EXECUTABLES += test_auth_link
test_auth_link_DIR := $(ROOT)
test_auth_link_SRCS = test_auth_link.c
test_auth_link_LIBS = $(NC_LIBS)
define test_auth_link_TESTCMD
$(test_auth_link_RUN)
endef
TEST_TARGETS += test_auth_link
