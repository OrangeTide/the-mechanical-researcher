# Triton system emulator examples -- Makefile
#
# Cross-compile build for ColdFire target

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -g
LDFLAGS  = -lm

M68K_CC      = m68k-linux-gnu-gcc
M68K_CFLAGS  = -mcpu=5475 -O2 -nostdlib -static -ffreestanding
M68K_OBJCOPY = m68k-linux-gnu-objcopy
M68K_OBJDUMP = m68k-linux-gnu-objdump
M68K_LDFLAGS = -T app_link.ld

# We use 1 file per example. except for common.c
SRCS := $(filter-out common.c,$(wildcard *.c))
EXAMPLES = $(patsubst %.c,%.elf,$(SRCS))
DUMPFILES = $(EXAMPLES:%.elf=%.dump.s)

all: $(EXAMPLES)

clean:
	$(RM) $(EXAMPLES) common.o

distclean: clean
	$(RM) $(DUMPFILES)

disasm: $(DUMPFILES)

# Disassemble monitor ROM
%.dump.s: %.elf
	$(M68K_OBJDUMP) -d $< > $@

%.o: %.c
	$(M68K_CC) $(M68K_CFLAGS) -c -o $@ $<

%.elf: %.c common.o
	$(M68K_CC) $(M68K_CFLAGS) $(M68K_LDFLAGS) -o $@ $< common.o

# --- Testing -------------------------------------------------------------

### TODO ###

.PHONY : all clean distclean disasm
