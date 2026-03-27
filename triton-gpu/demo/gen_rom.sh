#!/bin/sh
# Generate a C header from a flat binary file.
# Usage: gen_rom.sh input.bin array_name output.h
set -e
BIN="$1"
NAME="$2"
OUT="$3"
SIZE=$(wc -c < "$BIN" | tr -d ' ')

{
    echo "/* Auto-generated from $BIN -- do not edit */"
    echo ""
    echo "static const unsigned int ${NAME}_size = ${SIZE};"
    echo ""
    echo "static const unsigned char ${NAME}_data[] = {"
    xxd -i < "$BIN" | sed 's/^/    /'
    echo "};"
} > "$OUT"

echo "$OUT: $SIZE bytes from $BIN"
