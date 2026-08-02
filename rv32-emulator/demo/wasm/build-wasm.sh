#!/bin/sh
# build-wasm.sh : build the WebAssembly module, the guest, and the page
# Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain
#
# Produces a single self-contained index.html with the module and the guest
# binary embedded, so the demo opens straight from the filesystem with no
# server and no network access.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
CLANG=${CLANG:-clang}
RVCC=${RVCC:-riscv64-linux-gnu-gcc}
OBJCOPY=${OBJCOPY:-riscv64-linux-gnu-objcopy}
OUT=${OUT:-$HERE}

# The interpreter, as a freestanding wasm32 module with no imports.
# -fno-math-errno keeps the one square root a single f64.sqrt instruction
# instead of a call to a library that does not exist here.
$CLANG --target=wasm32 -O2 -fno-math-errno -nostdlib -ffreestanding \
    -Wall -Wextra \
    -I "$HERE/.." -I "$HERE/include" \
    -Wl,--no-entry -Wl,--export-dynamic \
    -o "$OUT/rv32.wasm" "$HERE/rv32_wasm.c" "$HERE/../rv32.c"

# The guest, as a flat image loaded at 0x1000 in the emulator's memory
$RVCC -march=rv32imfc -mabi=ilp32f -O2 -nostdlib -static -ffreestanding \
    -fno-pic -Wl,--build-id=none -T "$HERE/guest.ld" \
    -o "$OUT/guest.elf" "$HERE/guest_start.S" "$HERE/guest.c" 2>&1 |
    grep -v 'LOAD segment with RWX permissions' || true

$OBJCOPY -O binary "$OUT/guest.elf" "$OUT/guest.bin"

awk -v w="$(base64 -w0 "$OUT/rv32.wasm")" \
    -v g="$(base64 -w0 "$OUT/guest.bin")" \
    '{ gsub(/__WASM_B64__/, w); gsub(/__GUEST_B64__/, g); print }' \
    "$HERE/page.html.in" > "$OUT/index.html"

printf 'rv32.wasm  %s bytes\n' "$(wc -c < "$OUT/rv32.wasm")"
printf 'guest.bin  %s bytes\n' "$(wc -c < "$OUT/guest.bin")"
printf 'index.html %s bytes\n' "$(wc -c < "$OUT/index.html")"
