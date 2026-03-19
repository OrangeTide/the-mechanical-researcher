# Minimal WASM 1.0 MVP Interpreter in C

## Feasibility

- **Under 100 lines**: Not feasible.
- **Under 500 lines**: Not realistic for anything that runs real `.wasm` files.
- **~1,500–2,000 lines**: Realistic for a no-validation interpreter that passes most spec tests. The smallest known working implementation ([wac](https://github.com/kanaka/wac)) is ~1,935 lines in a single C file.
- **JIT**: 5,000+ lines minimum. Interpreter is the right choice for minimal effort.

The opcode dispatch is mechanical but bulky. The real complexity lives in the module parser (LEB128, 11 section types, nested structures) and the ~15 control flow opcodes.

## Key References

### Specifications

- [WASM 1.0 Spec (W3C)](https://www.w3.org/TR/wasm-core-1/) — authoritative reference
- [WASM Binary Format](https://webassembly.github.io/spec/core/binary/index.html) — chapter 5 of the spec; the part you'll reference most
- [WASM Instruction Set](https://webassembly.github.io/spec/core/appendix/index-instructions.html) — complete opcode table with hex values and stack signatures
- [WASI Preview 1 API](https://github.com/WebAssembly/WASI/blob/main/legacy/preview1/docs.md) — the import interface users will implement

### Quick References

- [WASM opcode table](https://pengowray.github.io/wasm-ops/) — single-page opcode reference with hex values
- [WASM binary encoding](https://webassembly.github.io/spec/core/binary/modules.html) — module structure in binary
- [LEB128 encoding](https://en.wikipedia.org/wiki/LEB128) — variable-length integer format used throughout WASM

### Existing Minimal Implementations

| Project | Language | Core Lines | Notes |
|---------|----------|-----------|-------|
| [wac](https://github.com/kanaka/wac) | C | ~1,935 (`wa.c`) | **Start here.** Single-file interpreter, passes most spec tests. By Joel Martin (mal/Make-a-Lisp). WASI frontend is 83 lines (`wax.c`). |
| [tiny-wasm-runtime](https://github.com/r1ru/tiny-wasm-runtime) | C | ~4,800 | Passes all core spec tests. Separate decode/validate/exec phases. |
| [WAH](https://github.com/lifthrasiir/wah) | C | ~5,500 | Single stb-style header. Targets WASM 3.0 including SIMD. |
| [wasm3](https://github.com/wasm3/wasm3) | C | ~15K | Threaded-code rewrite at load time. 64KB binary. Runs on microcontrollers. |
| [wasm-micro-runtime](https://github.com/bytecodealliance/wasm-micro-runtime) | C | ~50K+ | Full spec, JIT, AOT. |
| [Irreducible blog](https://irreducible.io/blog/my-wasm-interpreter/) | C | — | "I Wrote a Wasm Interpreter in C" — walkthrough of the process. |
| [wasmgroundup.com](https://wasmgroundup.com/blog/wasm-vm-part-1/) | JS | — | "WebAssembly from the Ground Up" — incremental tutorial. |

**wac is the gold standard** for "smallest thing that actually works." Read `wa.c` end to end — it's one afternoon of reading.

## Development Tools

### Essential

- **wabt** (WebAssembly Binary Toolkit) — `apt install wabt` or build from [github.com/WebAssembly/wabt](https://github.com/WebAssembly/wabt)
  - `wat2wasm` — compile text format (.wat) to binary (.wasm). **Primary test input generator.**
  - `wasm2wat` — disassemble binary to text.
  - `wasm-objdump -d file.wasm` — hex dump with disassembly, showing exact byte offsets.
  - `wasm-objdump -x file.wasm` — show all sections, imports, exports.
  - `wasm-objdump -s file.wasm` — raw section hex dump.
  - `wasm-interp file.wasm -r func_name` — reference interpreter for checking expected results.
  - `wasm-validate file.wasm` — check if a module is valid.
- **xxd** — `xxd file.wasm` to inspect raw bytes. Already installed on most systems.
- **wasm-tools** — `cargo install wasm-tools` — `wasm-tools dump` gives clean hex+structure output.

### Compilers (for Generating Real Test Binaries)

- **clang** — `clang --target=wasm32-wasi --sysroot=/path/to/wasi-sysroot -o test.wasm test.c`
  - Get wasi-sysroot from [github.com/WebAssembly/wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases)
- **zig** — `zig build-exe -target wasm32-wasi test.zig` (built-in WASM support, no sysroot needed)
- **rustc** — `rustup target add wasm32-wasip1` then `cargo build --target wasm32-wasip1`

### Runtime References (for Comparing Behavior)

- **wasmtime** — `wasmtime run file.wasm` — production WASI runtime, great for "what should this do?"
- **wasm3** — fast interpreter, good for comparing output.

## Architecture

A WASM interpreter is a stack machine with two distinct phases:

### Phase 1: Load and Instantiate

1. Parse binary format (magic, version, sections)
2. Decode all sections into runtime data structures
3. Resolve imports against host-provided functions
4. Allocate linear memory
5. Copy data segments into memory (active segments have an offset expression)
6. Initialize globals (constant expressions only in MVP)
7. Copy element segments into tables
8. Call the start function, if one is declared

### Phase 2: Execute

1. Host calls an exported function by name
2. Push arguments onto the value stack, create a call frame
3. Enter the opcode dispatch loop
4. Return result(s) to host

### Core Components

1. **LEB128 decoder** (~15–20 lines) — unsigned and signed variants
2. **Module parser** (~300–500 lines) — 11 section types, nested structures, string decoding
3. **Execution engine** (~800–1200 lines) — opcode dispatch loop, control flow, function calls
4. **Runtime structures + API** (~100–200 lines) — stacks, memory, import/export resolution

### Runtime Data Structures

```
Value stack:   [i32|i64|f32|f64] values, one flat array
Call stack:    return address + locals base pointer per frame
Control stack: block/loop/if labels with target stack depth + PC
Linear memory: single contiguous uint8_t buffer (page size = 64KB)
Globals:       array of wasm_val
Table:         array of function indices (for call_indirect)
```

## Binary Format

### Module Header

Every `.wasm` file starts with 8 bytes:
```
00 61 73 6d    magic: \0asm
01 00 00 00    version: 1 (little-endian u32)
```

### Sections

Sections appear in order of ID. Each starts with `section_id (1 byte)` + `section_size (LEB128)`. Unknown or custom sections can be skipped by advancing `section_size` bytes.

| Section | ID | Purpose | Required? |
|---------|-----|---------|-----------|
| Custom | 0 | Name section, debug info | Skip entirely |
| Type | 1 | Function signatures | Yes |
| Import | 2 | External functions/memory | Yes (for WASI) |
| Function | 3 | Maps func index → type index | Yes |
| Table | 4 | Indirect call targets | If `call_indirect` used |
| Memory | 5 | Linear memory spec (initial/max pages) | Yes |
| Global | 6 | Global variables | If used |
| Export | 7 | Exposed functions/memory | Yes |
| Start | 8 | Auto-run function index | Optional |
| Element | 9 | Table initialization data | If table used |
| Code | 10 | Function bodies (bytecode) | Yes |
| Data | 11 | Memory initialization data | If used |

### Data Segments (Section 11)

MVP only has "active" data segments. Each contains:
- A memory index (always 0 in MVP)
- An offset expression (`i32.const N`, `end`)
- A byte vector to copy at that offset

These must be applied during instantiation before any code runs.

## Control Flow

Control flow is the trickiest part. WASM's `block`, `loop`, `if/else/end` create labels. `br N` branches to the Nth enclosing label. For blocks, branch goes forward to the matching `end`; for loops, branch goes backward to the `loop` start.

### Jump Target Resolution

A `br N` needs to know the PC of the target. Two approaches:

1. **Pre-scan at load time**: Parse each function's code once, record jump targets in a side table. This is what wac and wasm3 do. More code but O(1) branches at runtime.
2. **Linear scan at runtime**: When `br` executes, scan forward counting block nesting depth to find the matching `end`. Simpler code but O(n) per branch — will be slow in loops.

Pre-scan is recommended even for a minimal interpreter.

### Block Type Byte

The byte after `block`, `loop`, or `if` indicates the result type:
- `0x40` = void (no result)
- `0x7f` = i32, `0x7e` = i64, `0x7d` = f32, `0x7c` = f64

These are the only options in MVP (no multi-value blocks).

## Opcode Budget

WASM 1.0 MVP has ~170 opcodes. All should be implemented:

| Category | Count | Complexity | Notes |
|----------|-------|-----------|-------|
| Control flow | ~15 | **Hard** — the only complex part | block, loop, if, else, end, br, br_if, br_table, return, call, call_indirect, unreachable, nop |
| Parametric | 2 | Trivial | drop, select |
| Variable access | 5 | Trivial | local.get/set/tee, global.get/set |
| Memory load/store | ~20 | Easy | i32/i64/f32/f64 load/store + narrowing variants (i32.load8_s, etc.), memory.size, memory.grow |
| Constants | 4 | Easy | i32.const, i64.const, f32.const, f64.const |
| i32 ops | ~28 | **Trivial — 1 line each** | Arithmetic, bitwise, comparisons — direct C operator mapping |
| i64 ops | ~28 | **Trivial — 1 line each** | Same as i32 with uint64_t |
| f32 ops | ~15 | **Trivial — 1 line each** | C operators + `<math.h>` (fabsf, sqrtf, ceilf, floorf, etc.) |
| f64 ops | ~15 | **Trivial — 1 line each** | Same as f32 with double |
| Comparisons (f32/f64) | ~12 | **Trivial — 1 line each** | Float comparisons (watch NaN handling) |
| Conversions | ~20 | **Trivial — 1 line each** | C casts + reinterpret via memcpy/union |

The ~120 arithmetic/comparison/conversion opcodes are mechanical — they add line count but zero intellectual difficulty.

## Traps (Runtime Errors)

Even without validation, the interpreter must trap on these runtime conditions:

- **unreachable** opcode — always traps
- **Integer divide by zero** — `i32.div_s`, `i32.div_u`, `i32.rem_s`, `i32.rem_u` (and i64 variants)
- **Integer overflow** — `i32.div_s` with INT32_MIN / -1 (and i64 variant)
- **Out-of-bounds memory access** — load/store beyond memory size
- **Out-of-bounds table access** — `call_indirect` with index >= table size
- **Signature mismatch** — `call_indirect` where the function's type doesn't match the expected type. **This is the one runtime check you cannot skip**, even without validation.
- **Stack overflow** — deep recursion exhausting the call stack
- **Truncation overflow** — `i32.trunc_f32_s` etc. when the float value doesn't fit

A trap should unwind and return an error code to the host, not crash the process.

## Test Byte Sequences

Build and test incrementally. Create `.wat` files and compile with `wat2wasm -o test.wasm test.wat`.

### Test 1: Empty Module

Just the header — tests your module parser entry point.

```
00 61 73 6d 01 00 00 00
```

WAT: `(module)`

### Test 2: Return a Constant

```wat
(module
  (func (export "main") (result i32)
    i32.const 42))
```

Hand-assembled (37 bytes):
```
00 61 73 6d 01 00 00 00       ;; header
01 05 01 60 00 01 7f          ;; type section: 1 type, () -> i32
03 02 01 00                   ;; func section: func 0 = type 0
07 08 01 04 6d 61 69 6e 00 00 ;; export: "main" = func 0
0a 06 01 04 00 41 2a 0b       ;; code: 0 locals, i32.const 42, end
```

Code section byte-by-byte:
```
0a       section id 10 (Code)
06       section size: 6 bytes
01       1 function body
04       body size: 4 bytes
00       0 local declarations
41 2a    i32.const 42  (0x41 = opcode, 0x2a = 42 LEB128)
0b       end
```

**Expected**: calling "main" returns i32 42.

### Test 3: Addition

```wat
(module
  (func (export "add") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add))
```

Tests: parameter passing, local.get, i32.add.

### Test 4: Conditional

```wat
(module
  (func (export "abs") (param i32) (result i32)
    local.get 0
    i32.const 0
    i32.lt_s
    if (result i32)
      i32.const 0
      local.get 0
      i32.sub
    else
      local.get 0
    end))
```

Tests: if/else/end, comparison, block results.

### Test 5: Loop (Factorial) — First Real Milestone

```wat
(module
  (func (export "fac") (param i32) (result i32)
    (local i32)
    i32.const 1
    local.set 1
    block $done
      loop $loop
        local.get 0
        i32.eqz
        br_if $done
        local.get 1
        local.get 0
        i32.mul
        local.set 1
        local.get 0
        i32.const 1
        i32.sub
        local.set 0
        br $loop
      end
    end
    local.get 1))
```

Tests: block, loop, br, br_if, local variables. `fac(5)` = 120, `fac(10)` = 3628800. **If this works, your control flow is fundamentally correct.**

### Test 6: Memory

```wat
(module
  (memory (export "mem") 1)
  (func (export "store_load") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.store
    local.get 0
    i32.load))
```

Tests: linear memory, i32.store, i32.load. `store_load(0, 12345)` = 12345.

### Test 7: Function Calls

```wat
(module
  (func $double (param i32) (result i32)
    local.get 0
    i32.const 2
    i32.mul)
  (func (export "quad") (param i32) (result i32)
    local.get 0
    call $double
    call $double))
```

Tests: internal calls, call stack. `quad(3)` = 12.

### Test 8: Host Import

```wat
(module
  (import "env" "print" (func $print (param i32)))
  (func (export "main")
    i32.const 42
    call $print))
```

Tests: import resolution, host function calls. You provide `print` via `wasm_bind`.

### Test 9: Recursion (Fibonacci)

```wat
(module
  (func $fib (export "fib") (param i32) (result i32)
    local.get 0
    i32.const 2
    i32.lt_s
    if (result i32)
      local.get 0
    else
      local.get 0
      i32.const 1
      i32.sub
      call $fib
      local.get 0
      i32.const 2
      i32.sub
      call $fib
      i32.add
    end))
```

Tests: recursion, deep call stacks. `fib(10)` = 55, `fib(20)` = 6765.

### Test 10: br_table

```wat
(module
  (func (export "switch") (param i32) (result i32)
    block $d block $c block $b block $a
      local.get 0
      br_table $a $b $c $d
    end
    i32.const 10
    return
    end
    i32.const 20
    return
    end
    i32.const 30
    return
    end
    i32.const 99))
```

Tests: br_table (compiled switch statements). 0→10, 1→20, 2→30, default→99.

### Test 11: Traps

```wat
(module
  (func (export "div0") (result i32)
    i32.const 1
    i32.const 0
    i32.div_s))
```

Tests: trap handling. Must return an error, not crash.

### Test 12: Data Segments + memory.grow

```wat
(module
  (memory 1)
  (data (i32.const 0) "hello")
  (func (export "read") (result i32)
    i32.const 0
    i32.load8_u)
  (func (export "grow") (result i32)
    i32.const 1
    memory.grow))
```

Tests: data segment initialization, i32.load8_u, memory.grow. `read()` = 104 ('h'). `grow()` = 1 (old size was 1 page).

## Suggested Implementation Order

1. **Module header** — check magic + version (test 1)
2. **LEB128 decoder** — unsigned and signed variants
3. **Type section** — read function signatures
4. **Function + Code sections** — read function bodies as raw byte spans
5. **Export section** — find exported function by name
6. **Execution: constants + arithmetic** — i32.const, i32.add, etc. (tests 2, 3)
7. **Locals** — local.get/set/tee (test 3)
8. **Control flow** — block/loop/if/else/end/br/br_if (tests 4, 5) — **hardest step**
9. **Memory section + load/store** — (test 6)
10. **Function calls** — call, return, call stack (tests 7, 9)
11. **Imports** — import section, host function binding (test 8)
12. **br_table** — (test 10)
13. **Traps** — div by zero, OOB memory, call_indirect mismatch (test 11)
14. **Data segments, memory.grow** — (test 12)
15. **Remaining opcodes** — i64, f32, f64 arithmetic, conversions
16. **Globals, table, element sections, call_indirect**
17. **Start function** — call it during instantiation if declared

## API Design

### Option A: Raw Stack Access (Simpler, What wac Does)

```c
typedef union { uint32_t i32; uint64_t i64; float f32; double f64; } wasm_val;
typedef void (*wasm_hostfn)(wasm_val *stack, int *sp, uint8_t *mem);

wasm_module *wasm_load(const uint8_t *bytes, size_t len);
void         wasm_free(wasm_module *mod);
void         wasm_bind(wasm_module *mod, const char *module, const char *name,
                        wasm_hostfn fn);
int          wasm_call(wasm_module *mod, const char *name,
                        wasm_val *args, int nargs, wasm_val *ret);
```

Host functions pop/push directly on the interpreter's stack. Minimal code, but the host must know the stack layout.

### Option B: Args/Results Arrays (Safer)

```c
typedef void (*wasm_hostfn)(wasm_val *args, int nargs,
                             wasm_val *result, uint8_t *mem, uint32_t mem_size);
```

The interpreter copies args off the stack, calls the host function, pushes the result. Slightly more interpreter code but the host can't corrupt the stack.

**Recommendation**: Start with Option A for minimalism. Refactor to B later if needed.

### Example: WASI fd_write

```c
void wasi_fd_write(wasm_val *stack, int *sp, uint8_t *mem)
{
    uint32_t fd       = stack[*sp - 4].i32;
    uint32_t iovs     = stack[*sp - 3].i32;
    uint32_t iovs_len = stack[*sp - 2].i32;
    uint32_t nwritten = stack[*sp - 1].i32;
    *sp -= 4;

    uint32_t total = 0;
    for (uint32_t i = 0; i < iovs_len; i++) {
        uint32_t ptr = *(uint32_t *)(mem + iovs + i * 8);
        uint32_t len = *(uint32_t *)(mem + iovs + i * 8 + 4);
        write(fd, mem + ptr, len);
        total += len;
    }
    *(uint32_t *)(mem + nwritten) = total;
    stack[(*sp)++].i32 = 0; /* success */
}
```

## Minimal WASI for Practical Use

To run a C "hello world" compiled to WASM, implement ~6 WASI functions (~60–80 lines):

- `fd_write` — stdout/stderr output
- `fd_read` — stdin (stub returning 0 is OK initially)
- `proc_exit` — exit code
- `args_get` + `args_sizes_get` — command-line args (stub OK)
- `environ_get` + `environ_sizes_get` — environment variables (stub OK)

## Key Simplifications for Minimalism

1. **No validation** — trust the input module is well-formed (compiler output is)
2. **Fixed-size stacks** — static arrays, no dynamic allocation for stacks
3. **Single memory** — WASM 1.0 allows at most one
4. **No multi-value returns** — MVP limits blocks/functions to 0 or 1 return value
5. **Simple switch dispatch** — no computed goto, no threaded code
6. **Skip custom sections** — section ID 0 = skip by advancing `section_size` bytes

## Gotchas

- **LEB128 signed decoding**: Easy to get wrong for negative constants. `i32.const -1` encodes as `41 7f` — the sign bit is in bit 6 of the final byte, and must be extended to fill 32 bits.
- **Import ordering**: Import indices come before internal function indices. If a module imports 3 functions, the first internal function is index 3.
- **LEB128 immediates**: `i32.const`, `local.get`, `br`, `call`, block type, memory alignment+offset — all use LEB128. Your decoder is called constantly.
- **f32/f64 constants**: Read 4 or 8 raw IEEE 754 bytes (little-endian). Not LEB128.
- **br_table**: Variable-length opcode — reads a count, then count+1 LEB128 target indices (the last is the default). Compilers emit this for switch statements.
- **Memory alignment**: Load/store ops include alignment hints but the spec requires unaligned access to work. On x86 this is free; on ARM use memcpy.
- **Endianness**: WASM linear memory is little-endian. On little-endian hosts (x86, ARM LE, RISC-V) you can memcpy directly between the value stack and linear memory. On big-endian hosts, every load/store needs a byte swap. See the "Byte Swapping" section below.
- **NaN propagation in float comparisons**: `NaN != NaN` and `NaN < x` is false for all x. C handles this correctly for built-in operators, but watch `fmin`/`fmax` — WASM requires specific NaN behavior that may differ from C's.
- **Data segments**: Must be applied during instantiation before any code runs.
- **memory.grow**: Returns the previous size in pages (not bytes), or -1 on failure.
- **call_indirect**: Must check that the function's type signature matches the expected type at runtime. Mismatch = trap.

## Byte Swapping (Endianness Portability)

WASM specifies little-endian memory. On LE hosts (x86, x86_64, ARM LE, AArch64 LE, RISC-V) no swapping is needed — load/store can use `memcpy` directly. On BE hosts (s390x, SPARC, MIPS BE, PowerPC BE), every memory load/store must byte-swap.

### Detection

```c
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WASM_BIG_ENDIAN 1
#else
#define WASM_BIG_ENDIAN 0
#endif
```

`__BYTE_ORDER__` is defined by GCC (4.6+), Clang, and ICC. For MSVC, which only targets LE architectures, the `#else` branch is always correct.

For older/exotic compilers, a runtime fallback:

```c
static int is_big_endian(void)
{
    uint16_t x = 1;
    return *(uint8_t *)&x == 0;
}
```

### Swap Intrinsics

All major compilers provide byte-swap intrinsics that compile to a single instruction (BSWAP on x86, REV on ARM):

```c
#if defined(__GNUC__) || defined(__clang__)
    /* GCC 4.3+, Clang, ICC */
    #define bswap16(x) __builtin_bswap16(x)
    #define bswap32(x) __builtin_bswap32(x)
    #define bswap64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
    /* MSVC (all versions with <stdlib.h>) */
    #include <stdlib.h>
    #define bswap16(x) _byteswap_ushort(x)
    #define bswap32(x) _byteswap_ulong(x)
    #define bswap64(x) _byteswap_uint64(x)
#else
    /* Portable fallback — compiler may still recognize and optimize these */
    static inline uint16_t bswap16(uint16_t x) {
        return (x >> 8) | (x << 8);
    }
    static inline uint32_t bswap32(uint32_t x) {
        return (x >> 24) | ((x >> 8) & 0xFF00)
             | ((x << 8) & 0xFF0000) | (x << 24);
    }
    static inline uint64_t bswap64(uint64_t x) {
        return ((uint64_t)bswap32(x) << 32) | bswap32(x >> 32);
    }
#endif
```

### Load/Store Helpers

Wrap all linear memory access through these. On LE hosts the swap is compiled out. On BE hosts it compiles to a single `BSWAP`/`REV` instruction.

```c
static inline uint32_t wasm_load32(const uint8_t *mem, uint32_t addr)
{
    uint32_t val;
    memcpy(&val, mem + addr, 4);
#if WASM_BIG_ENDIAN
    val = bswap32(val);
#endif
    return val;
}

static inline void wasm_store32(uint8_t *mem, uint32_t addr, uint32_t val)
{
#if WASM_BIG_ENDIAN
    val = bswap32(val);
#endif
    memcpy(mem + addr, &val, 4);
}

static inline uint64_t wasm_load64(const uint8_t *mem, uint32_t addr)
{
    uint64_t val;
    memcpy(&val, mem + addr, 8);
#if WASM_BIG_ENDIAN
    val = bswap64(val);
#endif
    return val;
}

static inline void wasm_store64(uint8_t *mem, uint32_t addr, uint64_t val)
{
#if WASM_BIG_ENDIAN
    val = bswap64(val);
#endif
    memcpy(mem + addr, &val, 8);
}
```

The 8-bit and 16-bit variants follow the same pattern. For `i32.load8_u` no swap is needed (single byte). For `i32.load16_u` use `bswap16`. Float loads reinterpret through a union or memcpy after swapping the integer representation:

```c
static inline float wasm_loadf32(const uint8_t *mem, uint32_t addr)
{
    uint32_t bits = wasm_load32(mem, addr);
    float val;
    memcpy(&val, &bits, 4);
    return val;
}
```

Using `memcpy` for type punning (instead of pointer casts or unions) is the safest approach — it avoids strict aliasing violations and the compiler optimizes it away.

### Platform Notes

| Compiler | Swap intrinsic | Header | Notes |
|----------|---------------|--------|-------|
| GCC 4.3+ | `__builtin_bswap{16,32,64}` | none | `bswap16` requires GCC 4.8+ |
| Clang | `__builtin_bswap{16,32,64}` | none | All versions |
| MSVC | `_byteswap_u{short,long,int64}` | `<stdlib.h>` | Only targets LE, so swaps are only needed for cross-compilation |
| ICC | `__builtin_bswap{16,32,64}` | none | GCC-compatible |
| TCC | No intrinsics | — | Use portable fallback; TCC will not optimize it to BSWAP |

On x86, `__builtin_bswap32` compiles to a single `bswap` instruction. On ARM/AArch64, it compiles to `rev`. On RISC-V, GCC 12+ with Zbb extension emits `rev8`. The portable fallback compiles to 3–5 shift/mask instructions but GCC and Clang often recognize the pattern and emit `bswap` anyway.

## Conformance Testing

### Official Spec Test Suite

- **Repository**: https://github.com/WebAssembly/testsuite — mirror of `.wast` files (~266 test files)
- **Source**: `test/core/` in https://github.com/WebAssembly/spec

### .wast Format

`.wast` extends `.wat` with test directives:

```wast
(module
  (func (export "add") (param i32 i32) (result i32)
    local.get 0  local.get 1  i32.add))

(assert_return (invoke "add" (i32.const 1) (i32.const 1)) (i32.const 2))
(assert_trap (invoke "div_s" (i32.const 1) (i32.const 0)) "integer divide by zero")
```

Assertion types:
- `assert_return` — expects a specific return value
- `assert_trap` — expects a runtime trap
- `assert_invalid` — expects validation failure (skip if not validating)
- `assert_malformed` — expects binary decode failure
- `assert_unlinkable` — expects import resolution failure
- `assert_exhaustion` — expects stack overflow

### Smoke Test Order

Start with these `.wast` files (no imports needed):

1. `i32.wast` — i32 arithmetic
2. `i64.wast` — i64 arithmetic
3. `block.wast` — block/end
4. `loop.wast` — loop/br
5. `if.wast` — if/else
6. `br.wast`, `br_if.wast`, `br_table.wast` — branching
7. `call.wast` — function calls
8. `local_get.wast`, `local_set.wast`, `local_tee.wast` — locals
9. `memory.wast` — load/store
10. `global.wast` — globals
11. `return.wast` — early return
12. `f32.wast`, `f64.wast` — floating point
13. `conversions.wast` — type conversions
14. `call_indirect.wast` — table-based dispatch
15. `select.wast`, `stack.wast`

### Running Spec Tests

**Approach 1: Borrow wac's test driver**

wac's `runtest.py` parses `.wast`, compiles modules with `wat2wasm`, feeds them to the interpreter, and checks assertions:

```sh
./runtest.py --wat2wasm /usr/bin/wat2wasm --interpreter ./mywasm \
    spec/test/core/i32.wast
```

**Approach 2: JSON conversion**

The spec's OCaml reference interpreter converts `.wast` to JSON + `.wasm` files:

```sh
# build the reference interpreter
git clone https://github.com/WebAssembly/spec && cd spec/interpreter && make
# convert
./wasm test.wast -o test.json
```

The JSON lists test commands with expected results. Write a small C harness that reads JSON, loads `.wasm` files, and checks results.

**Approach 3: Manual**

For early development, grab `.wat` snippets from `.wast` files, compile with `wat2wasm`, and hardcode expected results in a shell script. Graduate to a proper runner later.

## Opcode Dispatch Performance

### switch Statement (Recommended)

A `switch` on `uint8_t` with -O2 compiles to a direct jump table on GCC and Clang. WASM MVP opcodes span 0x00–0xC4 — dense enough for the compiler to generate a 256-entry table. O(1) with a single indexed memory load.

```c
switch (code[pc++]) {
case 0x41: /* i32.const */ ...
case 0x6A: /* i32.add  */ ...
default: return ERR_UNIMPLEMENTED;
}
```

Verify: `gcc -O2 -S interp.c` — look for a jump table.

### gperf

gperf generates perfect hash functions for **string keys**. It is the wrong tool for byte-valued opcode dispatch — a switch on a byte is already optimal.

gperf **is** useful for import/export name resolution (string lookups). Example for WASI:

```
/* wasi_funcs.gperf */
%struct-type
struct wasi_func { const char *name; void (*handler)(vm_state *); };
%%
fd_write,       wasi_fd_write
fd_read,        wasi_fd_read
proc_exit,      wasi_proc_exit
args_get,       wasi_args_get
args_sizes_get, wasi_args_sizes_get
%%
```

```sh
gperf wasi_funcs.gperf > wasi_funcs.c   # O(1) string lookup, zero collisions
```

### computed goto (Optional Upgrade)

GCC/Clang support computed goto for faster dispatch:

```c
static void *dispatch[] = { &&op_unreachable, &&op_nop, &&op_block, ... };
goto *dispatch[code[pc++]];
op_i32_add: sp--; stack[sp-1].i32 += stack[sp].i32; goto *dispatch[code[pc++]];
```

Eliminates switch branch prediction overhead. Non-portable (GCC/Clang extension). Worth considering after the interpreter works.
