#!/bin/sh
# run-tests.sh - run each build/<name> under qemu-m68k and check the
# exit status against the expected value encoded in tests/<name>.expect
# (one integer per file). Tests without an .expect file are run for
# their side effects (stdout) and only required to exit 0.
#
# Usage: run-tests.sh [qemu-binary]

set -eu

QEMU=${1:-qemu-m68k}
HERE=$(dirname "$0")
ROOT=$(cd "$HERE/.." && pwd)

fail=0
pass=0

for bin in "$ROOT"/build/*; do
    [ -x "$bin" ] || continue
    name=$(basename "$bin")
    # skip object files and the start runtime
    case $name in
        *.o|start*|tinc) continue ;;
    esac
    expect_file="$HERE/$name.expect"
    set +e
    "$QEMU" "$bin" >"$ROOT/build/$name.out" 2>&1
    rc=$?
    set -e
    if [ -f "$expect_file" ]; then
        want=$(cat "$expect_file")
        if [ "$rc" -eq "$want" ]; then
            printf 'PASS  %s (rc=%d)\n' "$name" "$rc"
            pass=$((pass + 1))
        else
            printf 'FAIL  %s (rc=%d, want %s)\n' "$name" "$rc" "$want"
            fail=$((fail + 1))
        fi
    else
        if [ "$rc" -eq 0 ]; then
            printf 'PASS  %s\n' "$name"
            pass=$((pass + 1))
        else
            printf 'FAIL  %s (rc=%d)\n' "$name" "$rc"
            fail=$((fail + 1))
        fi
    fi
done

printf '\n%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
