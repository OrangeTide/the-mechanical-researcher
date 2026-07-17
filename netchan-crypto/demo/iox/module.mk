# iox -- vendored event loop: poll-based fd watchers, one-shot timers,
# and self-pipe signal delivery. Self-contained apart from the header-only
# priority queue (pq.h) it bundles for the timer heap.

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

LIBRARIES += iox
iox_DIR := $(ROOT)
iox_SRCS = iox_loop.c iox_fd.c iox_signal.c iox_timer.c
iox_EXPORTED_CPPFLAGS = -I$(iox_DIR)
