# GNUmakefile - host (Linux) build for development and tests.
# Use plain `make` here; use `make dos` (or `wmake`) for the DOS target.
# Made by a machine. PUBLIC DOMAIN (CC0-1.0)

CC      = gcc
CFLAGS  = -std=c99 -D_GNU_SOURCE -W -Wall -Wextra -O2 -g

all: ncdemo test_netchan test_game test_gnet thor

ncdemo: ncdemo.o netchan.o
	$(CC) $(CFLAGS) -o $@ ncdemo.o netchan.o

test_netchan: test_netchan.o netchan.o nc_udp.o
	$(CC) $(CFLAGS) -o $@ test_netchan.o netchan.o nc_udp.o

test_game: test_game.o game.o rng.o
	$(CC) $(CFLAGS) -o $@ test_game.o game.o rng.o

test_gnet: test_gnet.o game_net.o game.o rng.o netchan.o nc_udp.o
	$(CC) $(CFLAGS) -o $@ test_gnet.o game_net.o game.o rng.o netchan.o nc_udp.o

thor: thor.o render.o plat_host.o game.o game_net.o netchan.o nc_udp.o rng.o
	$(CC) $(CFLAGS) -o $@ thor.o render.o plat_host.o game.o game_net.o \
		netchan.o nc_udp.o rng.o

shot: shot.o game.o rng.o
	$(CC) $(CFLAGS) -o $@ shot.o game.o rng.o

check: test_netchan test_game test_gnet
	./test_netchan && ./test_game >/dev/null && ./test_gnet

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Cross-build the DOS .exe via Open Watcom wmake.
dos:
	wmake -f makefile

clean:
	rm -f ncdemo test_netchan test_game test_gnet thor shot *.o
	rm -f *.exe *.obj

# Regenerate the article screenshot (needs ../../tools/term-screenshot).
screenshot: shot
	./shot | ../../tools/term-screenshot \
		-t "Conan the ASCII Destroyer - netchan over IPX" -w 400 -o ../screenshot.svg

.PHONY: all dos check clean screenshot
