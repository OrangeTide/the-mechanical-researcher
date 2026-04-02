# Microser Demo

Minimal tagged serialization for embedded systems. An IDL compiler
(shell + awk) generates C encode/decode functions with no runtime
dependencies beyond `<stdint.h>` and `<string.h>`.

## Prerequisites

- POSIX shell and awk
- C99 compiler (gcc, clang, tcc, etc.)

## Build

    make

This runs the generator on `sensor.idl` to produce `sensor.h` and
`sensor.c`, then compiles the demo.

## Run

    ./demo

## Expected Output

    SensorReading (17 bytes on wire):
      0f 00              length = 15
      08 07            field 1 (8-bit) = 7
      12 00 0c 0b 66   field 2 (32-bit) = 1712000000
      18 03            field 3 (8-bit) = 3
      21 ea 00         field 4 (16-bit) = 234
      29 8f 02         field 5 (16-bit) = 655
      decoded: id=7 ts=1712000000 kind=3 temp=234 hum=655

    DeviceInfo (14 bytes on wire):
      0c 00              length = 12
      09 34 12         field 1 (16-bit) = 4660
      10 02            field 2 (8-bit) = 2
      18 01            field 3 (8-bit) = 1
      22 80 51 01 00   field 4 (32-bit) = 86400
      decoded: id=0x1234 fw=2.1 uptime=86400

## Files

- `microser.h` - runtime primitives (static inline, zero dependencies)
- `sensor.idl` - example IDL defining sensor protocol messages
- `gen.sh` - IDL compiler (shell + awk), generates C code
- `demo.c` - demo program showing encode/decode/annotate
- `Makefile` - build instructions
