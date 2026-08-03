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
SMOKE       = test_coldfire
TEST_ELF    = test_program.elf
QEMU_VALID  = qemu_validate
INSTR_ELF   = test_instructions.elf
QEMU_INSTR  = test_instructions_qemu

all: $(HARNESS) $(TEST_ELF)

# Host: build test harness
$(HARNESS): test_harness.c coldfire.c elf_loader.c coldfire.h elf_loader.h
	$(CC) $(CFLAGS) -o $@ test_harness.c coldfire.c elf_loader.c -lm

# Cross: build bare-metal test program
$(TEST_ELF): test_program.c link.ld
	$(M68K_CC) $(M68K_CFLAGS) $(M68K_LD) -o $@ test_program.c

# Host: build smoke test (no cross-compiler needed)
$(SMOKE): test_coldfire.c coldfire.c coldfire.h
	$(CC) $(CFLAGS) -o $@ test_coldfire.c coldfire.c -lm

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

# Cross: build comprehensive instruction test
$(INSTR_ELF): test_instructions.c link.ld
	$(M68K_CC) $(M68K_CFLAGS) -O0 $(M68K_LD) -o $@ $<

$(QEMU_INSTR): test_instructions.c
	$(M68K_CC) $(M68K_CFLAGS) -O0 -fno-builtin -DQEMU_USERMODE -o $@ $<

# Run instruction tests on emulator
instrtest: $(HARNESS) $(INSTR_ELF)
	./$(HARNESS) $(INSTR_ELF)

# Run instruction tests on QEMU for comparison
instrtest-qemu: $(QEMU_INSTR)
	qemu-m68k -cpu cfv4e ./$(QEMU_INSTR)

# EMAC differential test against QEMU.
#
# Only the subset qemu-m68k models faithfully is compared. Its cfv4e EMAC
# gets mac.l right but treats msac.l as an add and does not carry a
# product past 32 bits into the 48-bit accumulator, so the saturation
# cases are run separately rather than diffed against a model that does
# not implement them.
EMAC_ELF   = test_emac.elf
EMAC_QEMU  = test_emac_qemu
EMAC_FULL  = test_emac_full.elf
EMAC_DUMP  = emac_dump

$(EMAC_ELF): test_emac.c link.ld
	$(M68K_CC) $(M68K_CFLAGS) -DEMAC_COMPARABLE $(M68K_LD) -o $@ $<

$(EMAC_FULL): test_emac.c link.ld
	$(M68K_CC) $(M68K_CFLAGS) $(M68K_LD) -o $@ $<

$(EMAC_QEMU): test_emac.c
	$(M68K_CC) $(M68K_CFLAGS) -fno-builtin -DQEMU_USERMODE \
	    -DEMAC_COMPARABLE -o $@ $<

$(EMAC_DUMP): emac_dump.c coldfire.c elf_loader.c coldfire.h elf_loader.h
	$(CC) $(CFLAGS) -o $@ emac_dump.c coldfire.c elf_loader.c -lm

emactest: $(EMAC_DUMP) $(EMAC_ELF) $(EMAC_QEMU) $(EMAC_FULL)
	@./$(EMAC_DUMP) $(EMAC_ELF) > emac-ours.txt
	@qemu-m68k -cpu cfv4e ./$(EMAC_QEMU) > emac-qemu.txt
	@if diff -q emac-ours.txt emac-qemu.txt > /dev/null; then \
	    echo "emac: $$(wc -l < emac-ours.txt) records match qemu"; \
	else \
	    echo "emac: DIFFERS from qemu"; \
	    diff emac-ours.txt emac-qemu.txt; exit 1; \
	fi
	@./$(EMAC_DUMP) $(EMAC_FULL) > emac-full.txt
	@echo "emac: $$(wc -l < emac-full.txt) records from the full set" \
	     "(saturation included, emulator only)"

# Memory check
valgrind: $(HARNESS) $(TEST_ELF)
	valgrind --leak-check=full ./$(HARNESS) $(TEST_ELF)

# Coverage analysis with gcov
COV_CFLAGS  = -Wall -Wextra -O0 -g --coverage
COV_HARNESS = test_harness_cov
COV_SMOKE   = test_coldfire_cov

coldfire_cov.o: coldfire.c coldfire.h
	$(CC) $(COV_CFLAGS) -c -o $@ coldfire.c

$(COV_SMOKE): test_coldfire.c coldfire_cov.o coldfire.h
	$(CC) $(COV_CFLAGS) -o $@ test_coldfire.c coldfire_cov.o -lm

$(COV_HARNESS): test_harness.c coldfire_cov.o elf_loader.c coldfire.h elf_loader.h
	$(CC) $(COV_CFLAGS) -o $@ test_harness.c coldfire_cov.o elf_loader.c -lm

coverage: $(COV_SMOKE) $(COV_HARNESS) $(INSTR_ELF)
	rm -f *.gcda
	./$(COV_SMOKE)
	./$(COV_HARNESS) $(INSTR_ELF)
	gcov -o coldfire_cov.gcno coldfire.c

clean:
	rm -f $(HARNESS) $(SMOKE) $(TEST_ELF) $(QEMU_VALID) $(INSTR_ELF) $(QEMU_INSTR)
	rm -f $(COV_SMOKE) $(COV_HARNESS) *.gcno *.gcda *.gcov
	rm -f $(EMAC_ELF) $(EMAC_FULL) $(EMAC_QEMU) $(EMAC_DUMP) emac-*.txt

.PHONY: all test smoke disasm sections validate valgrind instrtest instrtest-qemu emactest coverage clean
