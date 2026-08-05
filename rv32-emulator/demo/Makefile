# Makefile for the RV32IMAFC_Zicsr_Zifencei_Zba_Zbb_Zbs_Zcb_Zcmp emulator demo
# Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain

# Host compiler.
#
# -fno-math-errno matters: it keeps the one square root the emulator uses a
# single machine instruction instead of a call into the math library, which
# is what lets the interpreter link without -lm and build for WebAssembly
# unchanged.
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -fno-math-errno

# Cross compiler for the guest programs
RVCC        = riscv64-linux-gnu-gcc
RVARCH      = rv32imafc
RVCFLAGS    = -march=$(RVARCH) -mabi=ilp32f -O2 -nostdlib -static \
              -ffreestanding -fno-pic -Wl,--build-id=none
RVLD        = -T link.ld

# GNU as cannot assemble Zcmp yet, so those guests are assembled with clang
# and linked with GNU ld, which produces a layout qemu-riscv32 can load.
CLANG       = clang
RVOBJCOPY   = riscv64-linux-gnu-objcopy
RVLDBIN     = riscv64-linux-gnu-ld

# The bit-manipulation architecture used for the A/B comparison. Any guest
# named foo_zb.elf is the same source as foo.elf built for it, which is
# what makes the two instruction counts comparable.
RVARCH_ZB  := $(RVARCH)_zba_zbb_zbs
RVARCH_ZCB := $(RVARCH_ZB)_zcb

# qemu CPU models. Zcmp shares its encoding space with the compressed
# double-precision loads and stores, so the compressed set has to be
# spelled out rather than selected with C.
QEMU_CPU_ZCMP = rv32,c=false,zca=true,zcf=true,zcmp=true

# The reference for the bit-manipulation work. qemu's default rv32 model
# already has these, but naming them keeps the reference pinned to the
# emulator's ISA rather than to a default that may change.
QEMU_CPU_ZB   = rv32,zba=true,zbb=true,zbs=true,zcb=true

CORE        = rv32.c
TOOLS       = test_harness test_fp test_mem test_zcmp test_atomic \
              test_bitmanip test_zcb test_irq \
              lockstep fuzz \
              archtest-run bench_rv32
GUESTS      = test_program.elf fuzz_target.elf bench.elf bitmanip_guest.elf \
              zcb_guest.elf

all: $(TOOLS) $(GUESTS)

# ---------------------------------------------------------------- tools

test_harness: test_harness.c $(CORE) machine.c elf_loader.c
	$(CC) $(CFLAGS) -o $@ $^

test_fp: test_fp.c $(CORE) machine.c
	$(CC) $(CFLAGS) -o $@ $^

test_mem: test_mem.c $(CORE) machine.c
	$(CC) $(CFLAGS) -o $@ $^

test_atomic: test_atomic.c $(CORE) machine.c
	$(CC) $(CFLAGS) -o $@ $^

test_bitmanip: test_bitmanip.c $(CORE) machine.c
	$(CC) $(CFLAGS) -o $@ $^

test_irq: test_irq.c $(CORE) machine.c
	$(CC) $(CFLAGS) -o $@ $^

test_zcb: test_zcb.c $(CORE) machine.c
	$(CC) $(CFLAGS) -o $@ $^

test_zcmp: test_zcmp.c $(CORE) machine.c
	$(CC) $(CFLAGS) -o $@ $^

lockstep: lockstep.c $(CORE) machine.c elf_loader.c gdbclient.c
	$(CC) $(CFLAGS) -o $@ $^

fuzz: fuzz.c $(CORE) machine.c elf_loader.c gdbclient.c
	$(CC) $(CFLAGS) -o $@ $^

archtest-run: archtest.c $(CORE) elf_loader.c
	$(CC) $(CFLAGS) -o $@ $^

bench_rv32: bench_rv32.c $(CORE) machine.c elf_loader.c
	$(CC) $(CFLAGS) -o $@ $^

# --------------------------------------------------------------- guests

test_program.elf: test_program.c start.S link.ld
	$(RVCC) $(RVCFLAGS) $(RVLD) -o $@ start.S test_program.c

fuzz_target.elf: fuzz_target.S link.ld
	$(RVCC) $(RVCFLAGS) $(RVLD) -o $@ fuzz_target.S

bench.elf bench_zb.elf bench_zcb.elf: bench.c start.S link.ld
	$(RVCC) $(RVCFLAGS) $(RVLD) -o $@ start.S bench.c

test_program_zb.elf test_program_zcb.elf: test_program.c start.S link.ld
	$(RVCC) $(RVCFLAGS) $(RVLD) -o $@ start.S test_program.c

# A guest written so that a compiler emits the bit-manipulation
# instructions itself, for the lockstep comparison against qemu.
bitmanip_guest.elf: RVARCH = $(RVARCH_ZB)
zcb_guest.elf: RVARCH = $(RVARCH_ZCB)
zcb_guest.elf: zcb_guest.c start.S link.ld
	$(RVCC) $(RVCFLAGS) $(RVLD) -o $@ start.S zcb_guest.c

bitmanip_guest.elf: bitmanip_guest.c start.S link.ld
	$(RVCC) $(RVCFLAGS) $(RVLD) -o $@ start.S bitmanip_guest.c

# Any guest whose name ends in _zb is the same source built for Zba, Zbb
# and Zbs. Target-specific variables do the work, so the recipes above are
# shared between the two builds and cannot drift apart.
%_zb.elf: RVARCH = $(RVARCH_ZB)
%_zcb.elf: RVARCH = $(RVARCH_ZCB)

# Guests built with whole-frame push and pop. GNU as cannot assemble Zcmp,
# so clang produces the objects and GNU ld places them.
%_zcmp.elf: %.c start.S link.ld
	$(CLANG) --target=riscv32 -march=rv32imfc_zcmp -mabi=ilp32f -O2 \
	    -ffreestanding -fno-pic -mno-relax -c -o $*_zcmp.o $*.c
	$(CLANG) --target=riscv32 -march=rv32imfc_zcmp -mabi=ilp32f \
	    -fno-pic -mno-relax -c -o start_zcmp.o start.S
	$(RVLDBIN) -m elf32lriscv -T link.ld --build-id=none -o $@ \
	    start_zcmp.o $*_zcmp.o

# ---------------------------------------------------------------- tests

# Everything that needs no reference model
check: test_fp test_mem test_zcmp test_zcb test_irq test_atomic \
       test_bitmanip test_harness test_program.elf
	./test_fp
	./test_mem
	./test_zcmp
	./test_bitmanip
	./test_zcb
	./test_irq
	./test_atomic
	./test_harness test_program.elf

# Instruction-by-instruction comparison against qemu-riscv32
lockstep-run: lockstep test_program.elf
	./lockstep test_program.elf

test-irq: test_irq
	./test_irq

lockstep-zcb: lockstep zcb_guest.elf
	./lockstep zcb_guest.elf -cpu $(QEMU_CPU_ZB)

lockstep-bitmanip: lockstep bitmanip_guest.elf
	./lockstep bitmanip_guest.elf -cpu $(QEMU_CPU_ZB)

lockstep-zcmp: lockstep test_program_zcmp.elf
	./lockstep test_program_zcmp.elf -cpu $(QEMU_CPU_ZCMP)

# Randomized encodings and operands, compared against qemu-riscv32
fuzz-run: fuzz fuzz_target.elf
	./fuzz fuzz_target.elf -n 50

# The compliance suite. ACT points at the riscv-test-suite directory of a
# checkout of riscv-arch-test on the old-framework-3.x branch.
ACT ?= ../../riscv-arch-test/riscv-test-suite

archtest: archtest-run
	./run-archtest.sh $(ACT)

# ------------------------------------------------------------ benchmark

bench: bench_rv32 bench.elf
	./bench_rv32 bench.elf 20

# Compares against the ColdFire V4e emulator from the earlier article
CF = ../../coldfire-emulator/demo

bench_coldfire: bench_coldfire.c $(CF)/coldfire.c $(CF)/elf_loader.c
	$(CC) $(CFLAGS) -I $(CF) -o $@ $^ -lm

bench_cf.elf: bench.c
	m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -ffreestanding \
	    -Wl,--build-id=none -T $(CF)/link.ld -o $@ bench.c

bench-compare: bench_rv32 bench.elf bench_coldfire bench_cf.elf
	./bench_rv32 bench.elf 20
	./bench_coldfire bench_cf.elf 20

# ----------------------------------------------------------- webassembly

wasm:
	./wasm/build-wasm.sh

# ----------------------------------------------------- bit manipulation

# The A/B measurement: the same source built twice, differing only in
# -march, run to completion on the same emulator. What is compared is
# retired instructions rather than wall-clock time, because that is the
# number a guest's own architecture decides and the host cannot perturb.
#
# CM and LUA have to point at checkouts for the last two rows.
BM_GUESTS ?= test_program bench coremark lua

# The published interpreter, kept unchanged so that the cost of a larger
# decoder can be measured rather than asserted. rv32-orig.c is a
# byte-for-byte copy of rv32.c as the first article described it.
ORIG = rv32-orig.c

bench_rv32_orig: bench_rv32.c $(ORIG) machine.c elf_loader.c
	$(CC) $(CFLAGS) -o $@ $^

# Does the wider decode path slow down code that uses none of it? Both
# binaries run the same base-ISA guest.
bitmanip-overhead: bench_rv32 bench_rv32_orig bench.elf
	@echo "published interpreter, base-ISA guest:"
	@./bench_rv32_orig bench.elf 20 | tail -1
	@echo "with Zba, Zbb and Zbs decoded, same guest:"
	@./bench_rv32 bench.elf 20 | tail -1

# Zcb is a code-size extension, not an instruction-count one: every
# encoding stands for exactly one 32-bit instruction, so the same program
# retires the same instructions from a smaller image. The column that
# matters here is .text.
zcb-compare: test_harness
	@printf '%-14s %10s %10s %9s %12s\n' guest "+B text" "+Zcb text" change insns
	@for g in $(BM_GUESTS); do \
	    $(MAKE) -s $${g}_zb.elf $${g}_zcb.elf >/dev/null 2>&1 || \
	        { printf '%-14s %10s\n' $$g "not built"; continue; }; \
	    ta=$$(riscv64-linux-gnu-size $${g}_zb.elf | awk 'NR==2{print $$1}'); \
	    tb=$$(riscv64-linux-gnu-size $${g}_zcb.elf | awk 'NR==2{print $$1}'); \
	    a=$$(./test_harness $${g}_zb.elf 2>/dev/null | \
	         sed -n 's/^\([0-9]*\) instructions retired.*/\1/p'); \
	    b=$$(./test_harness $${g}_zcb.elf 2>/dev/null | \
	         sed -n 's/^\([0-9]*\) instructions retired.*/\1/p'); \
	    awk -v n=$$g -v ta=$$ta -v tb=$$tb -v a=$$a -v b=$$b 'BEGIN{ \
	        printf "%-14s %10d %10d %8.2f%% %+12.4f%%\n", \
	               n, ta, tb, (tb-ta)*100.0/ta, (b-a)*100.0/a }'; \
	done

bitmanip-compare: test_harness
	@printf '%-14s %14s %14s %9s %10s\n' guest base "Zba+Zbb+Zbs" change text
	@for g in $(BM_GUESTS); do \
	    $(MAKE) -s $$g.elf $${g}_zb.elf >/dev/null 2>&1 || \
	        { printf '%-14s %14s\n' $$g "not built"; continue; }; \
	    a=$$(./test_harness $$g.elf 2>/dev/null | \
	         sed -n 's/^\([0-9]*\) instructions retired.*/\1/p'); \
	    b=$$(./test_harness $${g}_zb.elf 2>/dev/null | \
	         sed -n 's/^\([0-9]*\) instructions retired.*/\1/p'); \
	    ta=$$(riscv64-linux-gnu-size $$g.elf | awk 'NR==2{print $$1}'); \
	    tb=$$(riscv64-linux-gnu-size $${g}_zb.elf | awk 'NR==2{print $$1}'); \
	    awk -v n=$$g -v a=$$a -v b=$$b -v ta=$$ta -v tb=$$tb 'BEGIN{ \
	        printf "%-14s %14d %14d %8.2f%% %9.2f%%\n", \
	               n, a, b, (b-a)*100.0/a, (tb-ta)*100.0/ta }'; \
	done

# ------------------------------------------------------------- coverage

COV = coverage-build

coverage: $(GUESTS)
	rm -rf $(COV) && mkdir -p $(COV)
	$(CC) -O0 -g --coverage -fno-math-errno -I. -c rv32.c -o $(COV)/rv32.o
	$(CC) -O0 -g --coverage -fno-math-errno -I. -o $(COV)/test_fp \
	    test_fp.c $(COV)/rv32.o machine.c
	$(CC) -O0 -g --coverage -fno-math-errno -I. -o $(COV)/test_mem \
	    test_mem.c $(COV)/rv32.o machine.c
	$(CC) -O0 -g --coverage -fno-math-errno -I. -o $(COV)/test_zcmp \
	    test_zcmp.c $(COV)/rv32.o machine.c
	$(CC) -O0 -g --coverage -fno-math-errno -I. -o $(COV)/test_atomic \
	    test_atomic.c $(COV)/rv32.o machine.c
	$(CC) -O0 -g --coverage -fno-math-errno -I. -o $(COV)/harness \
	    test_harness.c $(COV)/rv32.o machine.c elf_loader.c
	cd $(COV) && ./test_fp >/dev/null && ./test_mem >/dev/null && \
	    ./test_zcmp >/dev/null && ./test_atomic >/dev/null && \
	    ./harness ../test_program.elf >/dev/null
	cd $(COV) && gcov -o . rv32.o | grep -A1 rv32.c

clean:
	rm -f $(TOOLS) $(GUESTS) bench_coldfire bench_cf.elf *_zcmp.elf
	rm -f *.o *.gcno *.gcda *.gcov
	rm -rf $(COV) archtest-work
	rm -f wasm/rv32.wasm wasm/guest.elf wasm/guest.bin wasm/index.html
	rm -f coremark.elf ee_printf_port.c lua.elf
	rm -f bench_rv32_orig *_zb.elf *_zcb.elf

.PHONY: all check test-irq lockstep-run lockstep-zcmp lockstep-bitmanip \
        lockstep-zcb fuzz-run \
        archtest bench bitmanip-compare bitmanip-overhead zcb-compare \
        coremark lua \
        bench-compare wasm coverage clean

# ------------------------------------------------------------- coremark

# CoreMark is a third-party benchmark and is not vendored here. Point
# CM at a checkout:
#   git clone --depth 1 https://github.com/eembc/coremark.git ../../coremark
#
# The port in coremark-port/ supplies the four things CoreMark's barebones
# target expects from a board: a clock, board initialization, a character
# sink, and the seed variables. Its own ee_printf is used with the
# placeholder character sink removed, since ours lives in the port.
CM ?= ../../coremark
CM_ITERATIONS ?= 100

coremark.elf coremark_zb.elf coremark_zcb.elf: coremark-port/core_portme.c \
                             coremark-port/core_portme.h start.S
	@test -d $(CM) || { echo "set CM to a coremark checkout" >&2; exit 1; }
	perl -0pe 's{void\nuart_send_char\(char c\)\n\{.*?\n\}\n}{/* uart_send_char is supplied by the port */\n}s' \
	    $(CM)/barebones/ee_printf.c > ee_printf_port.c
	$(RVCC) $(RVCFLAGS) $(RVLD) -I coremark-port -I $(CM) \
	    -DPERFORMANCE_RUN=1 -DITERATIONS=$(CM_ITERATIONS) -DHAS_FLOAT=0 \
	    -DHAS_TIME_H=0 -DUSE_CLOCK=0 -DHAS_PRINTF=0 \
	    -DFLAGS_STR='"$(RVCFLAGS)"' -o $@ \
	    start.S $(CM)/core_list_join.c $(CM)/core_main.c \
	    $(CM)/core_matrix.c $(CM)/core_state.c $(CM)/core_util.c \
	    coremark-port/core_portme.c ee_printf_port.c

# CoreMark self-validates: it prints the CRCs of each workload and checks
# them. The iterations-per-second figure is not a CoreMark score, because
# the clock here counts retired instructions rather than time.
coremark: test_harness coremark.elf
	./test_harness coremark.elf

# ------------------------------------------------------------------ lua

# Lua is a much harder test than a benchmark: it allocates constantly,
# uses setjmp and longjmp for errors, formats floating-point numbers, and
# its script checks its own answers. Running it needs a C library, which
# is not part of a stock Ubuntu riscv cross toolchain:
#
#   sudo apt-get install gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf
#   git clone --depth 1 -b v5.4.7 https://github.com/lua/lua.git ../../lua
#
# Lua itself is not vendored here; point LUA at a checkout.
LUA      ?= ../../lua
LUACC    ?= riscv64-unknown-elf-gcc
LUAARCH  ?= rv32imafc
LUAFLAGS  = -march=$(LUAARCH) -mabi=ilp32f -O2 --specs=picolibc.specs \
            -DLUA_32BITS=0 -DLUA_USE_C89=0 \
            -Wl,--build-id=none -T lua-port/lua.ld -nostartfiles

# Everything except lua.c and onelua.c, which carry their own main
LUA_SRCS = $(filter-out $(LUA)/lua.c $(LUA)/onelua.c, $(wildcard $(LUA)/*.c))

lua.elf lua_zb.elf lua_zcb.elf: lua-port/lua_port.c lua-port/lua_start.S \
                    lua-port/lua.ld
	@test -d $(LUA) || { echo "set LUA to a lua checkout" >&2; exit 1; }
	$(LUACC) $(LUAFLAGS) -I $(LUA) -o $@ \
	    lua-port/lua_start.S lua-port/lua_port.c $(LUA_SRCS) -lm

lua_zb.elf: LUAARCH = $(LUAARCH_ZB)
lua_zcb.elf: LUAARCH = $(LUAARCH_ZB)_zcb
LUAARCH_ZB := rv32imafc_zba_zbb_zbs

lua: test_harness lua.elf
	./test_harness lua.elf
