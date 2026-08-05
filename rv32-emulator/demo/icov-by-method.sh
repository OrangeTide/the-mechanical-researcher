#!/bin/sh
# icov-by-method.sh : which guest instructions each method executes
# Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain
#
# The companion to coverage-by-method.sh, asking the question that suits an
# emulator rather than the one that suits any C program. Line coverage says
# which of rv32.c a method ran. This says which of the instruction set it
# ran, and the two can disagree sharply: a single switch arm serves eight
# instructions, so a method can execute every line of the decoder while
# touching a fraction of the architecture.
#
# Usage: ./icov-by-method.sh [method ...]
# Methods: unit program apps archtest fuzz lockstep
#
# Every tool is built with -DRV_ICOV and linked with icov.c, which writes
# the table out at exit when RV_ICOV_OUT names a path. No tool's source
# knows about any of this.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
WORK=${WORK:-./icov-work}
CC=${CC:-gcc}
CFLAGS="-O2 -g -fno-math-errno -DRV_ICOV -I$HERE"
ACT=${ACT:-../../riscv-arch-test/riscv-test-suite}

METHODS="$*"
if [ -z "$METHODS" ]; then
    METHODS="unit program"
    if [ -f "$HERE/coremark.elf" ] || [ -f "$HERE/lua.elf" ]; then
        METHODS="$METHODS apps"
    fi
    if [ -d "$ACT/env" ]; then
        METHODS="$METHODS archtest"
    fi
    if command -v qemu-riscv32 >/dev/null 2>&1; then
        METHODS="$METHODS fuzz lockstep"
    fi
fi

rm -rf "$WORK"
mkdir -p "$WORK"

echo "building the instrumented tools"
$CC $CFLAGS -c "$HERE/rv32.c" -o "$WORK/rv32.o"
$CC $CFLAGS -c "$HERE/icov.c" -o "$WORK/icov.o"

link() {
    _out=$1
    shift
    $CC $CFLAGS -o "$WORK/$_out" "$@" "$WORK/rv32.o" "$WORK/icov.o" 2>/dev/null
}

link test_fp        "$HERE/test_fp.c" "$HERE/machine.c"
link test_mem       "$HERE/test_mem.c" "$HERE/machine.c"
link test_zcmp      "$HERE/test_zcmp.c" "$HERE/machine.c"
link test_zcb       "$HERE/test_zcb.c" "$HERE/machine.c"
link test_bitmanip  "$HERE/test_bitmanip.c" "$HERE/machine.c"
link test_atomic    "$HERE/test_atomic.c" "$HERE/machine.c"
link test_irq       "$HERE/test_irq.c" "$HERE/machine.c"
link harness        "$HERE/test_harness.c" "$HERE/machine.c" "$HERE/elf_loader.c"
link archtest-run   "$HERE/archtest.c" "$HERE/elf_loader.c"
link fuzz           "$HERE/fuzz.c" "$HERE/machine.c" "$HERE/elf_loader.c" \
                    "$HERE/gdbclient.c"
link lockstep       "$HERE/lockstep.c" "$HERE/machine.c" "$HERE/elf_loader.c" \
                    "$HERE/gdbclient.c"

TOTAL=0     # the dump lists every instruction, so the first method sets it

run_method() {
    case $1 in
    unit)
        for t in test_fp test_mem test_zcmp test_zcb test_bitmanip \
                 test_atomic test_irq; do
            "$WORK/$t" >/dev/null 2>&1 || true
        done
        ;;
    program)
        "$WORK/harness" "$HERE/test_program.elf" >/dev/null 2>&1 || true
        ;;
    apps)
        for e in coremark.elf lua.elf; do
            if [ -f "$HERE/$e" ]; then
                "$WORK/harness" "$HERE/$e" >/dev/null 2>&1 || true
            fi
        done
        ;;
    archtest)
        RUN="$WORK/archtest-run" WORK="$WORK/archtest-work" PORT=31370 \
            "$HERE/run-archtest.sh" "$ACT" >/dev/null 2>&1 || true
        ;;
    fuzz)
        "$WORK/fuzz" "$HERE/fuzz_target.elf" -n 20 -q >/dev/null 2>&1 || true
        ;;
    lockstep)
        ZB="rv32,zba=true,zbb=true,zbs=true,zcb=true"
        ZCMP="rv32,c=false,zca=true,zcf=true,zcmp=true"
        "$WORK/lockstep" "$HERE/test_program.elf" >/dev/null 2>&1 || true
        for g in bitmanip_guest.elf zcb_guest.elf; do
            if [ -f "$HERE/$g" ]; then
                "$WORK/lockstep" "$HERE/$g" -cpu "$ZB" >/dev/null 2>&1 || true
            fi
        done
        if [ -f "$HERE/test_program_zcmp.elf" ]; then
            "$WORK/lockstep" "$HERE/test_program_zcmp.elf" -cpu "$ZCMP" \
                >/dev/null 2>&1 || true
        fi
        ;;
    esac
}

echo
echo "running each method alone"
for m in $METHODS; do
    rm -f "$WORK"/count.*
    RV_ICOV_OUT="$WORK/count" run_method "$m"
    # The compliance runner forks per test, so merge whatever was left.
    cat "$WORK"/count.* 2>/dev/null |
        awk '{ n[$1] += $2 } END { for (k in n) print k, n[k] }' |
        sort > "$WORK/all-$m"
    awk '$2 > 0 { print $1 }' "$WORK/all-$m" | sort > "$WORK/set-$m"
    if [ "$TOTAL" -eq 0 ]; then
        TOTAL=$(wc -l < "$WORK/all-$m" | tr -d ' ')
        cp "$WORK/all-$m" "$WORK/every-name"
    fi
    echo "  $m: $(wc -l < "$WORK/set-$m") instructions"
done
rm -f "$WORK"/count.*

cat "$WORK"/set-* | sort -u > "$WORK/set-union"

echo
printf '%-10s %8s %9s %9s %9s\n' method seen "of $TOTAL" unique cumulative
printf '%-10s %8s %9s %9s %9s\n' ------ ---- ------- ------ ----------

: > "$WORK/cum"
for m in $METHODS; do
    others=""
    for o in $METHODS; do
        [ "$o" = "$m" ] || others="$others $WORK/set-$o"
    done
    if [ -n "$others" ]; then
        cat $others | sort -u > "$WORK/others"
    else
        : > "$WORK/others"
    fi
    n=$(wc -l < "$WORK/set-$m")
    u=$(comm -23 "$WORK/set-$m" "$WORK/others" | wc -l)
    cat "$WORK/cum" "$WORK/set-$m" | sort -u > "$WORK/cum.new"
    mv "$WORK/cum.new" "$WORK/cum"
    c=$(wc -l < "$WORK/cum")
    awk -v m="$m" -v n="$n" -v t="$TOTAL" -v u="$u" -v c="$c" \
        'BEGIN { printf "%-10s %8d %8.1f%% %9d %9d\n", m, n, n*100/t, u, c }'
done

U=$(wc -l < "$WORK/set-union")
awk -v n="$U" -v t="$TOTAL" \
    'BEGIN { printf "\nunion      %8d %8.1f%%\n", n, n*100/t }'

echo
echo "never executed by any method:"
awk '{ print $1 }' "$WORK/every-name" | sort > "$WORK/every"
comm -23 "$WORK/every" "$WORK/set-union" | tr '\n' ' ' | fold -s -w 72
echo
