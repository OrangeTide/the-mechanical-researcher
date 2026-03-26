# Makefile for ColdFire V4e emulator demo
# Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain

# Host compiler
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g

# Cross compiler for ColdFire V4e
M68K_CC     = m68k-linux-gnu-gcc
M68K_CFLAGS = -mcpu=5475 -O2 -nostdlib -static -ffreestanding
M68K_LD     = -T link.ld

# Targets
HARNESS     = test_harness
SMOKE       = smoke_test
TEST_ELF    = test_program.elf
QEMU_VALID  = qemu_validate

all: $(HARNESS) $(TEST_ELF)

# Host: build test harness
$(HARNESS): test_harness.c coldfire.c elf_loader.c coldfire.h elf_loader.h
	$(CC) $(CFLAGS) -o $@ test_harness.c coldfire.c elf_loader.c -lm

# Cross: build bare-metal test program
$(TEST_ELF): test_program.c link.ld
	$(M68K_CC) $(M68K_CFLAGS) $(M68K_LD) -o $@ test_program.c

# Host: build smoke test (no cross-compiler needed)
$(SMOKE): smoke_test.c coldfire.c coldfire.h
	$(CC) $(CFLAGS) -o $@ smoke_test.c coldfire.c -lm

# Smoke test — embedded binary, no cross-tools required
smoke: $(SMOKE)
	./$(SMOKE)

# Run tests (requires cross-compiler)
test: $(HARNESS) $(TEST_ELF)
	./$(HARNESS) $(TEST_ELF)

# Disassemble test program (for debugging)
disasm: $(TEST_ELF)
	m68k-linux-gnu-objdump -d $(TEST_ELF)

# Show section headers (for verifying .results placement)
sections: $(TEST_ELF)
	m68k-linux-gnu-readelf -S $(TEST_ELF)

# Cross: build QEMU validation program (no libc)
$(QEMU_VALID): qemu_validate.c
	$(M68K_CC) $(M68K_CFLAGS) -fno-builtin -o $@ $<

# Validate against QEMU (requires cross-compiler and qemu-m68k)
validate: $(QEMU_VALID)
	qemu-m68k -cpu cfv4e ./$(QEMU_VALID)

# Memory check
valgrind: $(HARNESS) $(TEST_ELF)
	valgrind --leak-check=full ./$(HARNESS) $(TEST_ELF)

clean:
	rm -f $(HARNESS) $(SMOKE) $(TEST_ELF) $(QEMU_VALID)

.PHONY: all test smoke disasm sections validate valgrind clean
