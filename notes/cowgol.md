# Cowgol

Cowgol is an experimental, Ada-inspired programming language designed for very small systems (6502, Z80, 8080, etc.). Created by David Given. The end goal is full self-hosting on 8-bit microcomputers.

- Website: https://cowlark.com/cowgol/
- Source: https://github.com/davidgiven/cowgol
- License: 2-clause BSD
- Last updated: ~2022

## Language Design

Ada-inspired syntax with `sub`/`end sub`, `if`/`end if`, `while`/`end loop` blocks. Uses `#` for comments.

### Type System

Very strongly typed -- no implicit casts, not even between integer widths of the same signedness. Explicit `as` casts required everywhere:

```
var i: uint8 := 1;
var j: uint16 := 2;
j := j + (i as uint16);  -- explicit cast required
j := j + 1;              -- numeric constants are the exception
```

Scalar types: `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`. Custom range types via `int(-8, 7)`.

### No Recursion

The most distinctive constraint. Forbidding recursion allows the linker to statically prove which functions can't be live simultaneously, enabling variable overlap -- different functions' local variables occupy the same memory addresses. On a 6502 with 256 bytes of zero page, this is essential. The compiler itself uses only 146 bytes of zero page.

### Other Features

- Records with inheritance and `@at()` for explicit offsets (unions, hardware registers)
- Single-dimensional arrays with `@sizeof` and `@indexof` operators
- Pointers
- Nested subroutines with access to enclosing scope
- Multiple return values: `sub swap(a, b): (out1, out2)`
- Forward declarations: `@decl sub Foo(i: uint8);` ... `@impl sub Foo is ... end sub;`
- External subroutines for separate compilation: `@extern("_name")`
- Static initializers for arrays and records
- Simple type inference on declaration with assignment

### Missing / Limitations

- No `null` keyword (cast `0` to pointer types)
- No for loops
- No debugging beyond `print()`
- No stable standard library
- No floating point

## Compiler Toolchain

### Architecture

Multi-stage pipeline, all written in Cowgol:

1. **Lexer** (`cowfe`): Hand-tooled, push-driven, ~650 lines
2. **Parser** (`cowfe`): Uses a customized Lemon parser generator modified to emit Cowgol. ~1300 line `.y` grammar file. Type checking and AST generation happen inline -- no separate AST rewrite stage
3. **Code generator** (`cowbe`): Table-driven. A tool called `newgen` reads backend definition files and emits Cowgol source. Backend definitions specify AST matching rules and register information. Instruction selection and register allocation happen simultaneously in a single bottom-up pass
4. **Linker** (`cowlink`): Global analysis and static variable placement. Analyzes the call graph to determine which variables can overlap in memory

### Code Generation

The code generator uses bottom-up, right-to-left pattern matching on a two-operand AST. Rules are sorted by complexity (most complex matched first). A destination-driven register allocator sees consumers before producers. When no register can be matched, spills are generated -- but often the spill can go to another register rather than the stack.

Example for 8080 (one accumulator):
```
lda y        -- load y into a
mov b, a     -- spill a to b (code gen sees b is free for intermediate range)
lda z        -- load z into a
add b        -- add b to a
sta x        -- store result
```

### Limitations of Code Generation

- No costing of rules -- always selects first matching rule
- Cannot reason about which registers are available during matching
- Spill heuristics are simple (no graph coloring)

### Backend Targets

Adding backends is cheap (~1-2 KLOC each):

**Compiler backends:**
- Z80 and 8080 (CP/M)
- 6502 and 65c02 (BBC Micro with Tube)
- 6303 (Fuzix)
- 6502 bytecode interpreter (BBC Micro)
- 80386 (Linux) -- 70 KB binary
- ARM Thumb2 (Linux)
- PowerPC (Linux)
- 68000 (Atari ST TOS, Linux m68k)
- 8086 (DOS, small mode .exe)
- PDP-11 (V7 Unix)
- Generic C (for bootstrapping)
- Microsoft BASIC (joke backend, partial language support)

**Self-hosting platforms** (compiler runs on the target):
- 80386 Linux, ARM Linux, PowerPC Linux, 68000 Linux
- BBC Micro with Tube second processor
- CP/M (Z80 and 8080)

### Bootstrap Chain

Cowgol bootstraps via its "generic and terrible C" backend:
1. Generate C code from Cowgol source
2. Compile with any C compiler to get a native Cowgol compiler
3. Use that to compile Cowgol for the target platform

### Performance

- 80386 Linux binary: 70 KB (including ELF overhead)
- 8080 CP/M compiler: 58 KB (split across two executables)
- Self-compilation time on PC: ~80 ms

## Static Variable Placement

The linker's most interesting feature. Since recursion is forbidden, the linker knows the complete call graph at link time. It can prove that certain functions will never be active simultaneously, and assign their local variables to overlapping memory addresses. This is what makes Cowgol feasible on machines with tiny address spaces.

For the 6502, the linker also knows that pointers must reside in zero page (an architectural requirement). Without overlap analysis, the 256-byte zero page would fill up immediately. With it, the compiler itself uses only 146 bytes.

## Separate Compilation

Cowgol supports separate compilation with external symbols. The linker resolves these across `.coo` files and performs global dead code elimination -- unreferenced code is stripped from the final binary.
