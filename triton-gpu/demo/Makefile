# Triton system emulator + GPU rasterizer -- Makefile
#
# Default build requires only a host C compiler.
# SDL3 build: run 'sh fetch-sdl3.sh' first, or install libsdl3-dev.
# Regenerating ROM headers requires m68k-linux-gnu-gcc.

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -g
LDFLAGS  = -lm

M68K_CC      = m68k-linux-gnu-gcc
M68K_CFLAGS  = -mcpu=5475 -O2 -nostdlib -static -ffreestanding
M68K_OBJCOPY = m68k-linux-gnu-objcopy

# SDL3: prefer local build from fetch-sdl3.sh, fall back to pkg-config
ifneq (,$(wildcard sdl3/lib/pkgconfig/sdl3.pc))
  SDL_CFLAGS = $(shell PKG_CONFIG_PATH=sdl3/lib/pkgconfig pkg-config --cflags sdl3)
  SDL_LIBS   = $(shell PKG_CONFIG_PATH=sdl3/lib/pkgconfig pkg-config --libs --static sdl3)
else ifneq (,$(wildcard sdl3/lib64/pkgconfig/sdl3.pc))
  SDL_CFLAGS = $(shell PKG_CONFIG_PATH=sdl3/lib64/pkgconfig pkg-config --cflags sdl3)
  SDL_LIBS   = $(shell PKG_CONFIG_PATH=sdl3/lib64/pkgconfig pkg-config --libs --static sdl3)
else
  SDL_CFLAGS = $(shell pkg-config --cflags sdl3 2>/dev/null)
  SDL_LIBS   = $(shell pkg-config --libs sdl3 2>/dev/null)
endif

SRC  = triton.c coldfire.c glide_raster.c
HDRS = triton.h coldfire.h glide_raster.h monitor_rom.h hello_program.h

all: triton-headless

# Headless build (no SDL3 dependency)
# Pre-generated monitor_rom.h and hello_program.h are committed to the repo.
# No cross-compiler needed for the default build.
triton-headless: $(SRC) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

# SDL3 build (run 'sh fetch-sdl3.sh' first if SDL3 is not installed)
triton: $(SRC) $(HDRS)
	$(CC) $(CFLAGS) -DTRITON_SDL3 $(SDL_CFLAGS) -o $@ $(SRC) $(SDL_LIBS) $(LDFLAGS)

# Fetch and build SDL3 locally
fetch-sdl3:
	sh fetch-sdl3.sh

# Run with SDL3
run: triton
	./triton

# Run headless
run-headless: triton-headless
	./triton-headless

# --- Cross-compilation (requires m68k-linux-gnu-gcc) ---------------------

# Rebuild all pre-compiled headers from source
regen:
	$(M68K_CC) $(M68K_CFLAGS) -T monitor_link.ld -o monitor.elf monitor.c
	$(M68K_OBJCOPY) -O binary monitor.elf monitor.bin
	sh gen_rom.sh monitor.bin monitor_rom monitor_rom.h
	$(M68K_CC) $(M68K_CFLAGS) -T hello_link.ld -o hello.elf hello.c
	sh gen_rom.sh hello.elf hello_program hello_program.h

# Disassemble monitor ROM
disasm-monitor:
	$(M68K_CC) $(M68K_CFLAGS) -T monitor_link.ld -o monitor.elf monitor.c
	m68k-linux-gnu-objdump -d monitor.elf

# Disassemble hello program
disasm-hello:
	$(M68K_CC) $(M68K_CFLAGS) -T hello_link.ld -o hello.elf hello.c
	m68k-linux-gnu-objdump -d hello.elf

# --- Testing -------------------------------------------------------------

valgrind: triton-headless
	valgrind --leak-check=full --error-exitcode=1 ./triton-headless

clean:
	rm -f triton triton-headless monitor.elf monitor.bin hello.elf hello.bin

distclean: clean
	rm -rf sdl3 SDL-release-*

.PHONY: all fetch-sdl3 run run-headless regen disasm-monitor disasm-hello \
        valgrind clean distclean
