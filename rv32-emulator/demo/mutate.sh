#!/bin/sh
# mutate.sh : does the test rig notice when the interpreter is wrong?
# Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain
#
# Coverage says which lines ran. It cannot say whether anything would have
# complained had those lines been wrong, and that is the question a test
# suite exists to answer. This introduces one deliberate defect at a time
# into rv32.c, rebuilds, and runs the rig. A mutant that still passes
# everything is a hole: some line was executed, produced a wrong answer,
# and nothing noticed.
#
# Usage: ./mutate.sh [-n count] [-s seed] [-m method] [-v]
#
#   -n  how many mutants to try (default 120)
#   -s  seed for choosing sites (default 1)
#   -m  which rig to run against each mutant, in stages, so that what each
#       stage adds is what the ones before it missed:
#         unit      the suites that need no reference model (default, fast)
#         full      unit, then lockstep and the fuzzer
#         all       full, then the compliance suite
#   -v  name every mutant as it is judged
#
# Survivors are left in the work directory with the diff that produced them.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
WORK=${WORK:-./mutate-work}
CC=${CC:-gcc}
CFLAGS="-O1 -g -fno-math-errno -w -I$HERE"
COUNT=120
SEED=1
MODE=unit
VERBOSE=0

while [ $# -gt 0 ]; do
    case $1 in
    -n) COUNT=$2; shift 2 ;;
    -s) SEED=$2; shift 2 ;;
    -m) MODE=$2; shift 2 ;;
    -v) VERBOSE=1; shift ;;
    *)  echo "usage: $0 [-n count] [-s seed] [-m unit|full] [-v]" >&2; exit 2 ;;
    esac
done

rm -rf "$WORK"
mkdir -p "$WORK"

# ------------------------------------------------------------ the sites
#
# Every mutation is a single token substitution on one line of rv32.c.
# Comment lines, preprocessor lines and the string literals are left
# alone: changing those produces a mutant that is not a different machine,
# only a different message, and it would be reported as a hole that is not
# one.

awk '
    # Skip anything that is not executable code.
    /^[ \t]*[*]/          { next }
    /^[ \t]*\/\*/         { next }
    /^[ \t]*\/\//         { next }
    /^[ \t]*#/            { next }
    /^[ \t]*$/            { next }
    { line = $0 }
    # One record per (line number, operator) pair that can be mutated.
    line ~ /[^<>=!+*\/-]<[^<=]/  { print NR "\t<\t<="  }
    line ~ /[^<>=!]>[^>=]/       { print NR "\t>\t>="  }
    line ~ /<=/                  { print NR "\t<=\t<"  }
    line ~ />=/                  { print NR "\t>=\t>"  }
    line ~ /==/                  { print NR "\t==\t!=" }
    line ~ /!=/                  { print NR "\t!=\t==" }
    line ~ /&&/                  { print NR "\t&&\t||" }
    line ~ /\|\|/                { print NR "\t||\t&&" }
    line ~ /[a-z0-9_)] \+ /      { print NR "\t + \t - " }
    line ~ /[a-z0-9_)] - /       { print NR "\t - \t + " }
    line ~ /[a-z0-9_)] \| /      { print NR "\t | \t & " }
    line ~ /[a-z0-9_)] & [^&]/   { print NR "\t & \t | " }
    line ~ /<< /                 { print NR "\t<< \t>> " }
    line ~ />> /                 { print NR "\t>> \t<< " }
' "$HERE/rv32.c" > "$WORK/sites.all"

TOTAL_SITES=$(wc -l < "$WORK/sites.all" | tr -d ' ')
echo "$TOTAL_SITES mutable sites in rv32.c"

# Shuffle deterministically from the seed, then take the first COUNT.
awk -v seed="$SEED" 'BEGIN { srand(seed) } { print rand() "\t" $0 }' \
    "$WORK/sites.all" | sort -n | cut -f2- | head -n "$COUNT" \
    > "$WORK/sites"

# ------------------------------------------------------------- the rigs

build_mutant() {
    _src=$1

    $CC $CFLAGS -c "$_src" -o "$WORK/rv32.o" 2>/dev/null || return 1
    for t in test_fp test_mem test_zcmp test_zcb test_bitmanip \
             test_atomic test_irq; do
        $CC $CFLAGS -o "$WORK/$t" "$HERE/$t.c" "$HERE/machine.c" \
            "$WORK/rv32.o" 2>/dev/null || return 1
    done
    $CC $CFLAGS -o "$WORK/harness" "$HERE/test_harness.c" "$HERE/machine.c" \
        "$HERE/elf_loader.c" "$WORK/rv32.o" 2>/dev/null || return 1
    if [ "$MODE" = full ] || [ "$MODE" = all ]; then
        $CC $CFLAGS -o "$WORK/lockstep" "$HERE/lockstep.c" "$HERE/machine.c" \
            "$HERE/elf_loader.c" "$HERE/gdbclient.c" "$WORK/rv32.o" \
            2>/dev/null || return 1
        $CC $CFLAGS -o "$WORK/fuzz" "$HERE/fuzz.c" "$HERE/machine.c" \
            "$HERE/elf_loader.c" "$HERE/gdbclient.c" "$WORK/rv32.o" \
            2>/dev/null || return 1
    fi
    if [ "$MODE" = all ]; then
        $CC $CFLAGS -o "$WORK/archtest-run" "$HERE/archtest.c" \
            "$HERE/elf_loader.c" "$WORK/rv32.o" 2>/dev/null || return 1
    fi
    return 0
}

# The compliance suite is minutes per run, so it is asked only about the
# mutants everything cheaper has already failed to notice, and only for the
# suites whose signatures cover the widest part of the machine.
ACT=${ACT:-../../riscv-arch-test/riscv-test-suite}
ARCH_SUITES=${ARCH_SUITES:-I M B}

run_compliance() {
    [ -d "$ACT/env" ] || return 1
    rm -rf "$WORK/archtest-work"
    RUN="$WORK/archtest-run" WORK="$WORK/archtest-work" PORT=31390 \
        timeout 600 "$HERE/run-archtest.sh" "$ACT" $ARCH_SUITES \
        >"$WORK/archtest.log" 2>&1 && return 1
    return 0
}

# Returns 0 when the rig noticed. A timeout counts as noticing: a mutant
# that makes the interpreter loop forever has been detected, just slowly.
run_unit() {
    for t in test_fp test_mem test_zcmp test_zcb test_bitmanip \
             test_atomic test_irq; do
        timeout 20 "$WORK/$t" >/dev/null 2>&1 || return 0
    done
    timeout 20 "$WORK/harness" "$HERE/test_program.elf" >/dev/null 2>&1 \
        || return 0
    return 1
}

run_reference() {
    ZB="rv32,zba=true,zbb=true,zbs=true,zcb=true"

    timeout 60 "$WORK/lockstep" "$HERE/test_program.elf" -p 31380 \
        >/dev/null 2>&1 || return 0
    if [ -f "$HERE/bitmanip_guest.elf" ]; then
        timeout 60 "$WORK/lockstep" "$HERE/bitmanip_guest.elf" -cpu "$ZB" \
            -p 31380 >/dev/null 2>&1 || return 0
    fi
    timeout 120 "$WORK/fuzz" "$HERE/fuzz_target.elf" -n 6 -q -p 31381 \
        >/dev/null 2>&1 || return 0
    return 1
}

# -------------------------------------------------------------- the run

killed=0
killed_unit=0
killed_ref=0
killed_act=0
survived=0
noncompiling=0
equivalent=0
n=0

: > "$WORK/survivors"

while IFS="	" read -r lineno from to <&3; do
    n=$((n + 1))
    awk -v ln="$lineno" -v from="$from" -v to="$to" '
        NR == ln { sub(from, to) } { print }' \
        "$HERE/rv32.c" > "$WORK/mutant.c"

    if cmp -s "$WORK/mutant.c" "$HERE/rv32.c"; then
        equivalent=$((equivalent + 1))
        continue
    fi
    if ! build_mutant "$WORK/mutant.c"; then
        noncompiling=$((noncompiling + 1))
        continue
    fi

    if run_unit; then
        killed=$((killed + 1))
        killed_unit=$((killed_unit + 1))
        [ "$VERBOSE" = 1 ] && echo "  killed   line $lineno: $from -> $to"
    elif { [ "$MODE" = full ] || [ "$MODE" = all ]; } && run_reference; then
        killed=$((killed + 1))
        killed_ref=$((killed_ref + 1))
        [ "$VERBOSE" = 1 ] &&
            echo "  killed by the reference  line $lineno: $from -> $to"
        echo "reference-only line $lineno: $from -> $to" >> "$WORK/survivors"
    elif [ "$MODE" = all ] && run_compliance; then
        killed=$((killed + 1))
        killed_act=$((killed_act + 1))
        [ "$VERBOSE" = 1 ] &&
            echo "  killed by compliance     line $lineno: $from -> $to"
        echo "compliance-only line $lineno: $from -> $to" >> "$WORK/survivors"
    else
        survived=$((survived + 1))
        echo "SURVIVED line $lineno: $from -> $to" >> "$WORK/survivors"
        [ "$VERBOSE" = 1 ] && echo "  SURVIVED line $lineno: $from -> $to"
        sed -n "${lineno}p" "$HERE/rv32.c" |
            sed "s/^/    was: /" >> "$WORK/survivors"
        cp "$WORK/mutant.c" "$WORK/survivor-$lineno.c"
    fi
done 3< "$WORK/sites"

tried=$((killed + survived))
echo
echo "mutants tried:        $n"
echo "  did not compile:    $noncompiling"
echo "  no textual change:  $equivalent"
echo "  judged:             $tried"
echo "  killed:             $killed"
echo "    by the unit rig:  $killed_unit"
[ "$killed_ref" -gt 0 ] && echo "    only by lockstep or the fuzzer: $killed_ref"
[ "$killed_act" -gt 0 ] && echo "    only by the compliance suite:   $killed_act"
echo "  survived:           $survived"
if [ "$tried" -gt 0 ]; then
    awk -v k="$killed" -v t="$tried" \
        'BEGIN { printf "\nmutation score: %.1f%%\n", k * 100 / t }'
fi
echo
echo "survivors and their lines are in $WORK/survivors"
