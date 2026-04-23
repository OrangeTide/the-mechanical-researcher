# Makefile for ColdFire V4e multi-language bare-metal demo
# Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain

# Host compiler
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g

# Cross compilers for ColdFire V4e
M68K_GCC     = m68k-linux-gnu-gcc
M68K_GXX     = m68k-linux-gnu-g++
M68K_GDC     = m68k-linux-gnu-gdc-9
M68K_GFORT   = m68k-linux-gnu-gfortran
M68K_GM2     = m68k-linux-gnu-gm2
M68K_GNAT_CC = m68k-linux-gnu-gcc-10

M68K_CPU     = -mcpu=5475
M68K_CFLAGS  = $(M68K_CPU) -O2 -nostdlib -static -ffreestanding
M68K_CXXFLAGS = $(M68K_CFLAGS) -fno-exceptions -fno-rtti
M68K_LD      = -T link.ld

# Targets
HARNESS  = test_harness

TEST_ELFS = test_cpp.elf \
            test_objc.elf \
            test_objcpp.elf \
            test_d.elf \
            test_fortran.elf \
            test_ada.elf \
            test_modula2.elf

all: $(HARNESS) $(TEST_ELFS)

# Host: build test harness
$(HARNESS): test_harness.c coldfire.c elf_loader.c coldfire.h elf_loader.h
	$(CC) $(CFLAGS) -o $@ test_harness.c coldfire.c elf_loader.c -lm

# --- Standalone languages (single source → ELF) ---

test_cpp.elf: test_cpp.cpp link.ld
	$(M68K_GXX) $(M68K_CXXFLAGS) $(M68K_LD) -o $@ test_cpp.cpp

test_objc.elf: test_objc.m link.ld
	$(M68K_GCC) $(M68K_CFLAGS) -x objective-c $(M68K_LD) -o $@ test_objc.m

test_objcpp.elf: test_objcpp.mm link.ld
	$(M68K_GXX) $(M68K_CXXFLAGS) -x objective-c++ $(M68K_LD) -o $@ test_objcpp.mm

test_d.elf: test_d.d object.d link.ld
	$(M68K_GDC) $(M68K_CPU) -O2 -fno-druntime -nostdlib -static -I. $(M68K_LD) -o $@ test_d.d

# --- Languages requiring C shim (compile then link) ---

test_fortran.o: test_fortran.f90
	$(M68K_GFORT) $(M68K_CPU) -O2 -c -o $@ test_fortran.f90

test_fortran.elf: shim_fortran.c test_fortran.o link.ld
	$(M68K_GCC) $(M68K_CFLAGS) $(M68K_LD) -o $@ shim_fortran.c test_fortran.o

test_ada.o: test_ada.adb
	$(M68K_GNAT_CC) $(M68K_CPU) -O2 -gnatp -c -o $@ test_ada.adb

test_ada.elf: shim_ada.c test_ada.o link.ld
	$(M68K_GNAT_CC) $(M68K_CFLAGS) $(M68K_LD) -o $@ shim_ada.c test_ada.o

test_modula2.o: test_modula2.mod
	$(M68K_GM2) $(M68K_CPU) -O2 -c -o $@ test_modula2.mod

test_modula2.elf: shim_modula2.c test_modula2.o link.ld
	$(M68K_GCC) $(M68K_CFLAGS) $(M68K_LD) -o $@ shim_modula2.c test_modula2.o

# --- Test targets ---

test: $(HARNESS) $(TEST_ELFS)
	@pass=0; fail=0; \
	for elf in $(TEST_ELFS); do \
	    echo "=== $$elf ==="; \
	    if ./$(HARNESS) $$elf; then \
	        pass=$$((pass + 1)); \
	    else \
	        fail=$$((fail + 1)); \
	    fi; \
	    echo; \
	done; \
	echo "=== Summary: $$pass passed, $$fail failed out of $$(echo $(TEST_ELFS) | wc -w) ==="

# --- Utilities ---

disasm-%: %.elf
	m68k-linux-gnu-objdump -d $<

sections-%: %.elf
	m68k-linux-gnu-readelf -S $<

clean:
	rm -f $(HARNESS) $(TEST_ELFS) test_fortran.o test_ada.o test_modula2.o

.PHONY: all test clean
