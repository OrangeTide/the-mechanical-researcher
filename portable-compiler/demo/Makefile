# Makefile : build the TinC compiler and its test programs.
#
# Host build:         make          -> build/tinc
# Target build+run:   make check    -> assembles/links each tests/*.tc
#                                      with m68k-linux-gnu-{as,ld} and
#                                      runs the result under qemu-m68k.
#
# Toolchain overrides:
#   CC         host C compiler
#   M68K_AS    cross assembler (default: m68k-linux-gnu-as)
#   M68K_LD    cross linker    (default: m68k-linux-gnu-ld)
#   QEMU       qemu user-mode  (default: qemu-m68k)

CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter
M68K_AS ?= m68k-linux-gnu-as
M68K_LD ?= m68k-linux-gnu-ld
QEMU    ?= qemu-m68k

IR_SRC  := ir/ir.c ir/util.c
BE_SRC  := backend/regalloc_cf.c backend/cf_emit.c
TC_SRC  := tinc/lex.c tinc/parse.c tinc/lower.c tinc/main.c
SRC     := $(IR_SRC) $(BE_SRC) $(TC_SRC)

TESTS := $(wildcard tests/*.tc)

all: build/tinc

build/tinc: $(SRC) ir/ir.h tinc/tinc.h | build
	$(CC) $(CFLAGS) -Iir -Itinc -o $@ $(SRC)

# Per-test rules: tests/<name>.tc -> build/<name>.s -> build/<name>.o
#                 + build/start.o -> build/<name>
build/%.s: tests/%.tc build/tinc | build
	./build/tinc -o $@ $<

build/%.o: build/%.s
	$(M68K_AS) -o $@ $<

build/start.o: runtime/start.S | build
	$(M68K_AS) -o $@ $<

build/%: build/%.o build/start.o
	$(M68K_LD) -o $@ build/start.o $<

build:
	mkdir -p build

check: build/tinc $(TESTS:tests/%.tc=build/%)
	@sh tests/run-tests.sh "$(QEMU)"

clean:
	rm -rf build

.PHONY: all check clean
