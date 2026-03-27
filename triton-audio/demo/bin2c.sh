#!/bin/sh
# bin2c.sh — convert a ColdFire ELF disassembly to a C byte array
# Usage: ./bin2c.sh my_test.elf
#
# Produces a C array on stdout with one line per instruction,
# hex bytes on the left and the disassembled mnemonic as a comment.
# Pipe into your smoke test source or redirect to a file.
#
# Example:
#   ./bin2c.sh test_program.elf > /tmp/array.c

set -e

if [ $# -lt 1 ]; then
    echo "usage: $0 <elf-file>" >&2
    exit 1
fi

OBJDUMP="${OBJDUMP:-m68k-linux-gnu-objdump}"

"$OBJDUMP" -d "$1" | awk '
/^[0-9a-f]+ <.*>:/ {
    label = $2
    gsub(/[<>:]/, "", label)
    if (NR > 1) printf "\n"
    printf "    /* %s */\n", label
    next
}
/^\s+[0-9a-f]+:\t/ {
    match($0, /^\s+([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$/, m)
    if (m[1] == "") next
    gsub(/\s+$/, "", m[2])
    n = split(m[2], bytes, " ")
    line = "    "
    for (i = 1; i <= n; i++) {
        b = bytes[i]
        for (j = 1; j <= length(b); j += 2)
            line = line sprintf("0x%s,", substr(b, j, 2))
    }
    pad = 40 - length(line)
    if (pad < 1) pad = 1
    printf "%s%*s/* %s */\n", line, pad, " ", m[3]
}'
