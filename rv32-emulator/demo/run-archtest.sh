#!/bin/sh
# run-archtest.sh : build and run the riscv-arch-test suites for RV32IMFC
# Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain
#
# Each test is assembled against the target port in archtest/, executed on
# the emulator, and its signature region compared byte for byte against the
# same binary running on qemu-system-riscv32.
#
# Usage: ./run-archtest.sh <path-to-riscv-test-suite> [suite ...]
#
# The suite directory is the riscv-test-suite/ directory of a checkout of
# https://github.com/riscv-non-isa/riscv-arch-test on the old-framework-3.x
# branch. The current main branch needs a toolchain whose assembler pads
# .p2align with nops while relaxation is disabled; binutils 2.42 fills it
# with zeros instead, which makes those binaries hang on any model.

set -e

SUITE_DIR="$1"
if [ -z "$SUITE_DIR" ] || [ ! -d "$SUITE_DIR/env" ]; then
    echo "usage: $0 <path-to-riscv-test-suite> [suite ...]" >&2
    exit 2
fi
shift

SUITES="$*"
[ -n "$SUITES" ] || SUITES="I M A C F F_Zcf Zifencei"

HERE=$(cd "$(dirname "$0")" && pwd)
PORT=${PORT:-31339}
WORK=${WORK:-./archtest-work}
CC=${CC:-riscv64-linux-gnu-gcc}
CFLAGS="-march=rv32imafc_zicsr_zifencei -mabi=ilp32f -nostdlib -static"
CFLAGS="$CFLAGS -fno-pic -no-pie -mcmodel=medany"
CFLAGS="$CFLAGS -Wl,--build-id=none -DXLEN=32 -DFLEN=32"

mkdir -p "$WORK"

pass=0
fail=0
build_fail=0
skipped=0

for suite in $SUITES; do
    src="$SUITE_DIR/rv32i_m/$suite/src"
    if [ ! -d "$src" ]; then
        echo "skip $suite: no such suite in $SUITE_DIR"
        continue
    fi

    echo
    echo "=== $suite"
    for test in "$src"/*.S; do
        name=$(basename "$test" .S)
        elf="$WORK/$suite-$name.elf"

        # cebreak stores mstatus into its signature. mstatus is a WARL
        # register whose readable bits depend on which privilege modes
        # exist, and qemu's virt CPU implements machine, supervisor and
        # user mode where this emulator implements machine mode only. The
        # two therefore read back different legal values and the test is
        # not comparable between them.
        if [ "$suite/$name" = "C/cebreak-01" ]; then
            echo "skip  $suite/$name (mstatus WARL bits differ: the "\
"reference implements S and U mode)"
            skipped=$((skipped + 1))
            continue
        fi

        # Each test declares the macros it needs inside its RVTEST_CASE
        # string, in the form "def NAME=True". Turning those into -D flags
        # is what enables features such as the machine trap handler that
        # the breakpoint and CSR tests rely on.
        defs=$(grep -o 'def [A-Za-z_][A-Za-z0-9_]*=[A-Za-z0-9_]*' "$test" |
               sed 's/^def /-D/' | sort -u | tr '\n' ' ')

        if ! $CC $CFLAGS $defs -I "$HERE/archtest-port" -I "$SUITE_DIR/env" \
                -T "$HERE/archtest-port/link.ld" -o "$elf" "$test" \
                2>"$WORK/$suite-$name.buildlog"; then
            echo "BUILD $suite/$name"
            build_fail=$((build_fail + 1))
            continue
        fi

        # Capture first, then test: piping into sed would report sed's
        # exit status instead of the runner's and hide every failure.
        if out=$("$HERE/archtest-run" "$elf" -p "$PORT" 2>/dev/null); then
            pass=$((pass + 1))
        else
            fail=$((fail + 1))
        fi
        echo "$out" | sed "s|$elf|$suite/$name|"
    done
done

echo
echo "riscv-arch-test: $pass passed, $fail failed, $skipped skipped, $build_fail did not build"
[ "$fail" -eq 0 ] && [ "$build_fail" -eq 0 ]
