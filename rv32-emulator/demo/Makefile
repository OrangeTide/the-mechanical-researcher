# Makefile for the RV32IMAFC_Zicsr_Zifencei_Zcmp emulator demo
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

# qemu CPU models. Zcmp shares its encoding space with the compressed
# double-precision loads and stores, so the compressed set has to be
# spelled out rather than selected with C.
QEMU_CPU_ZCMP = rv32,c=false,zca=true,zcf=true,zcmp=true

CORE        = rv32.c
TOOLS       = test_harness test_fp test_mem test_zcmp test_atomic \
              lockstep fuzz \
              archtest-run bench_rv32
GUESTS      = test_program.elf fuzz_target.elf bench.elf

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

bench.elf: bench.c start.S link.ld
	$(RVCC) $(RVCFLAGS) $(RVLD) -o $@ start.S bench.c

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
check: test_fp test_mem test_zcmp test_atomic test_harness test_program.elf
	./test_fp
	./test_mem
	./test_zcmp
	./test_atomic
	./test_harness test_program.elf

# Instruction-by-instruction comparison against qemu-riscv32
lockstep-run: lockstep test_program.elf
	./lockstep test_program.elf

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

.PHONY: all check lockstep-run lockstep-zcmp fuzz-run archtest bench \
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

coremark.elf: coremark-port/core_portme.c coremark-port/core_portme.h start.S
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
LUAFLAGS  = -march=rv32imafc -mabi=ilp32f -O2 --specs=picolibc.specs \
            -DLUA_32BITS=0 -DLUA_USE_C89=0 \
            -Wl,--build-id=none -T lua-port/lua.ld -nostartfiles

# Everything except lua.c and onelua.c, which carry their own main
LUA_SRCS = $(filter-out $(LUA)/lua.c $(LUA)/onelua.c, $(wildcard $(LUA)/*.c))

lua.elf: lua-port/lua_port.c lua-port/lua_start.S lua-port/lua.ld
	@test -d $(LUA) || { echo "set LUA to a lua checkout" >&2; exit 1; }
	$(LUACC) $(LUAFLAGS) -I $(LUA) -o $@ \
	    lua-port/lua_start.S lua-port/lua_port.c $(LUA_SRCS) -lm

lua: test_harness lua.elf
	./test_harness lua.elf
