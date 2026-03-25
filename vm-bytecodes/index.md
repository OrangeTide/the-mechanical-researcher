---
title: Virtual Machine Bytecodes
date: 2026-03-22
abstract: A comparative survey of virtual machine bytecode architectures — from the JVM's thirty-year dominance to WebAssembly's sandboxed linear memory — evaluating the design trade-offs that determine performance, security, complexity, and portability for anyone choosing or building a bytecode VM.
category: systems
---

## Introduction

Every bytecode virtual machine answers the same fundamental question: what is the right level of abstraction between source code and hardware? Too high, and you sacrifice performance. Too low, and you lose portability and safety. The answer has been attempted dozens of times across five decades — from Niklaus Wirth's P-code in 1970 to WebAssembly's 2017 MVP — and the range of solutions is striking.

A stack machine with 60 opcodes can sandbox Quake 3 game mods in about 2,000 lines of C. A register machine with 91 fixed-width instructions can power the scripting layer of every major game engine. An SSA-form intermediate representation with 67 instruction types can target twenty hardware architectures. These are not minor variations on a theme — they represent fundamentally different philosophies about what a virtual instruction set should be, what guarantees it should provide, and what it should leave to the implementor.

This article surveys eleven bytecode systems across six dimensions: architecture, performance, security, code density, implementation complexity, and practical limitations. The goal is not academic taxonomy but engineering decision-making — if you are choosing a bytecode format for a new project or building a VM from scratch, these are the trade-offs that matter.

## Abstract

We compare the Java Virtual Machine (JVM), Android's Dalvik/ART, LLVM IR bitcode, the Dis VM from Inferno OS, WebAssembly 1.0 MVP, and the Quake 3 VM (Q3VM) — with additional context from UCSD P-code, Smalltalk-80, Lua 5.4, .NET CIL, and Erlang's BEAM. The comparison is organized thematically: execution architecture (stack vs. register vs. memory-to-memory), instruction encoding and density, type systems and verification, memory models, compilation strategies, sandboxing, implementation complexity, and practical limitations. We find that no single design dominates across all dimensions — the right choice depends on whether the priority is sandboxing (Wasm, Q3VM), language ecosystem (JVM, CIL), compilation infrastructure (LLVM), embeddability (Lua), or minimalism (Dis, Q3VM).

## The Landscape at a Glance

Before diving into individual dimensions, a summary of what each system is and where it came from:

| System | Year | Origin | Purpose | Architecture |
|--------|------|--------|---------|-------------|
| UCSD P-code | 1973 | UC San Diego | Portable Pascal | Stack, 16-bit |
| Smalltalk-80 | 1980 | Xerox PARC | OOP environment | Stack, image-based |
| JVM | 1995 | Sun Microsystems | Portable applications | Stack, 32-bit slots |
| Dis VM | 1995 | Bell Labs (Lucent) | Inferno OS modules | Memory-to-memory, 3-address |
| .NET CIL | 2000 | Microsoft | Common language runtime | Stack, native word |
| Dalvik/ART | 2007 | Google (Android) | Mobile applications | Register, 16-bit units |
| LLVM IR | 2003 | UIUC / Chris Lattner | Compiler infrastructure | SSA, infinite registers |
| Lua 5.0+ | 2003 | PUC-Rio | Embeddable scripting | Register, fixed 32-bit |
| BEAM | 1998 | Ericsson | Concurrent/fault-tolerant | Register, per-process |
| Quake 3 VM (Q3VM) | 1999 | id Software | Sandboxed game mods | Stack, 32-bit |
| WebAssembly | 2017 | W3C (browser vendors) | Sandboxed portable code | Stack, structured control flow |

Five decades of experimentation — and the field has not converged on a single answer.

## Execution Architecture: Stack, Register, or Something Else

The most fundamental design choice is how instructions reference their operands. This single decision shapes everything downstream: code density, dispatch overhead, instruction count, and optimization difficulty.

### Stack Machines

In a stack machine, most instructions take no explicit operands. They consume values from the top of an implicit operand stack and push results back. `iadd` in the JVM means "pop two integers, push their sum." The advantage is compact encoding — if instructions do not name their operands, they need fewer bits. The disadvantage is instruction count — moving data between the stack and local variables requires explicit load/store instructions that a register machine would not need.

The JVM, .NET CIL, WebAssembly, Quake 3 Q3VM, UCSD P-code, and Smalltalk-80 all use stack architectures. But the similarities end at the high level. The JVM and CIL are classically stack-based: a local variable array per method frame, an operand stack, and instructions that shuffle values between them. WebAssembly adds *structured control flow* — no arbitrary gotos, only nested `block`/`loop`/`if` constructs — which makes validation simpler and compilation to native code more straightforward. Q3VM is the most stripped-down: a flat operand stack with four pseudo-registers (PC, SP, LP, FP) and no type information at all.

### Register Machines

In a register machine, instructions name their source and destination registers explicitly. `add-int v0, v1, v2` in Dalvik means "add v1 and v2, store in v0." This requires more bits per instruction (register indices must be encoded) but produces fewer total instructions, because intermediate values stay in registers rather than being pushed and popped.

Dalvik, Lua 5.4, and Erlang's BEAM use register architectures. Dalvik's encoding is clever: instructions are aligned to 16-bit boundaries with the opcode in the low 8 bits of the first unit. Most instructions address registers v0–v15 using 4-bit fields, with wider forms available for v0–v255 or v0–v65535. Lua uses fixed 32-bit instructions with a 7-bit opcode and 8-bit register fields — simpler to decode but less compact. BEAM takes a different approach entirely: X registers for temporaries and inter-function argument passing, Y registers for values that survive function calls, and a compact tagged encoding for operands.

The register-vs-stack debate was settled empirically by Yunhe Shi et al. (2005, "Virtual Machine Showdown") and by the Dalvik team's own measurements: register bytecode produces roughly 47% fewer executed instructions than stack bytecode for the same program, at the cost of 25% larger instruction stream. For an interpreter, fewer dispatches wins — instruction dispatch is typically the bottleneck. For a JIT compiler, the difference largely disappears because both are converted to native register code anyway.

### Memory-to-Memory: The Dis Exception

The Dis VM, designed by the same Bell Labs team that built Plan 9 and Inferno, chose neither stack nor register. Dis instructions are three-address with memory operands: `addi src, mid, dst` means "load from src, add mid, store to dst," where src, mid, and dst are offsets from either the frame pointer (fp) or the module pointer (mp). There is no operand stack and no register file — operands are always memory locations.

This is unusual. It maps cleanly to the Limbo language's semantics and simplifies JIT compilation (memory operands map directly to native load/store sequences), but it produces bulkier code than either stack or register designs. The address mode byte — 1 byte encoding source, middle, and destination addressing modes — adds overhead that neither the JVM nor Dalvik incur.

### SSA Form: LLVM's Non-Bytecode

LLVM IR is not bytecode in any traditional sense. It is a serialized Static Single Assignment (SSA) intermediate representation with infinite virtual registers, explicit control flow graphs, and full type annotations on every value. Each instruction produces a new named value that is used exactly once as a definition. This is the form that compilers *convert to internally* for optimization — LLVM simply exposes it as a portable format.

The serialized "bitcode" uses variable-bit-rate encoding with abbreviation tables, producing a compact binary — but it is not designed for interpretation or sandboxed execution. It exists to feed into LLVM's optimization and code-generation pipeline. Including it in a bytecode comparison is like including blueprints in a comparison of building materials — it is the design language, not the final product. That said, it is sometimes *used* as a distribution format (Apple's discontinued App Store bitcode, Android's renderscript), making the comparison relevant.

## Instruction Encoding and Code Density

How many bytes does it take to express a program? This matters for distribution size, cache utilization, memory-constrained devices, and network transfer.

| System | Encoding | Bytes per Instruction (avg) | Notes |
|--------|----------|----------------------------|-------|
| Smalltalk-80 | Variable, 1–3B | ~1.5 | Operands packed into opcode byte |
| JVM | Variable, 1–5B | ~1.9 | Many zero-operand opcodes |
| UCSD P-code | Variable, 1–3B | ~2.0 | Compact for its era |
| .NET CIL | Variable, 1–9B | ~2.2 | Short-form opcodes help |
| WebAssembly | Variable, 1–17B | ~2.5 | LEB128 operand encoding |
| Q3VM | Fixed, 1 or 5B | ~2.8 | Bimodal: 1B or 5B, nothing between |
| Dalvik/ART | Variable, 2–10B (16-bit units) | ~3.2 | Register indices cost bits |
| BEAM | Variable, tagged operands | ~4.0 | Tagged encoding is verbose |
| Lua 5.4 | Fixed, 4B always | 4.0 | Simplicity over density |
| Dis VM | Variable, 3–7B | ~4.5 | Address mode byte adds overhead |
| LLVM bitcode | Bitstream, variable | N/A | Not comparable; compiler IR |

Smalltalk-80 achieves the highest density by packing operands directly into opcode bytes — the instruction "push receiver variable 3" is a single byte (opcode 3), not an opcode-plus-operand pair. This is elegant but inflexible: the 256-byte opcode space is essentially full, leaving no room for extension.

The JVM and CIL achieve good density through a different trick: specialized short-form opcodes. `aload_0` (load `this` reference) is a single byte — semantically identical to the two-byte `aload 0` but one byte shorter. Since `this` is loaded in nearly every method, the savings add up. CIL takes this further with short forms for many common patterns: `ldc.i4.0` through `ldc.i4.8` are each one byte.

WebAssembly uses LEB128 (Little-Endian Base 128) encoding for integer operands — small values take one byte, larger values take more. This is a good trade-off for code density, since most operands (local indices, type indices, small constants) are small. But memory access instructions carry both an alignment hint and an offset, each LEB128-encoded, which can make them surprisingly large.

Fixed-width encodings (Lua, Q3VM) sacrifice density for decode simplicity. A fixed 4-byte instruction can be fetched, decoded, and dispatched without parsing — just mask and shift. This matters more for interpreters than for JIT compilers, and it makes Lua's interpreter loop remarkably clean: every iteration reads exactly one 32-bit word.

Q3VM's encoding is peculiar: the `.qvm` file stores instructions as either 1 byte (no operand) or 5 bytes (1-byte opcode + 4-byte little-endian parameter). There is no 2-byte or 3-byte form. `ADD` is 1 byte; `CONST 42` is 5 bytes. This is simple but wasteful — a constant value of 1 takes the same 5 bytes as a constant value of two billion. During loading, `VM_PrepareInterpreter()` expands the variable-length bytecode into a fixed array of 32-bit words — one word per instruction, with operands stored in a parallel array — so the interpreter loop never parses instruction boundaries at runtime.

But raw bytes-per-instruction is misleading without considering *instructions per operation*. A register machine needs fewer instructions for the same computation because it avoids the push/pop traffic of a stack machine. Dalvik averages 3.2 bytes per instruction but uses 47% fewer instructions than the JVM's 1.9 bytes per instruction for equivalent programs — so total program size is comparable, sometimes even smaller for Dalvik.

## Type Systems and Verification

Can the bytecode be statically validated before execution? This determines whether you can trust untrusted code without running it first — a property that matters enormously for web browsers, plugin systems, and any environment that executes third-party modules.

### Strongly Typed with Mandatory Verification

**WebAssembly** has the strictest validation regime. Every Wasm module *must* pass validation before execution — there is no "unverified" mode. The validator checks type consistency (operand stack types match instruction expectations), structured control flow (every `block` has an `end`, branches target valid labels), function signatures, and memory bounds. Validation is single-pass, O(n) in module size. This is possible because Wasm's structured control flow eliminates the need for fixed-point dataflow analysis — unlike the JVM, where arbitrary `goto` targets require iterative type inference.

**JVM** verification became mandatory with class file version 50.0 (Java 6). The verifier checks that every execution path produces a consistent operand stack depth and type. Stack map frames (pre-computed type information at branch targets) enable single-pass verification in modern class files. The verifier prevents type confusion: you cannot push an integer and pop it as an object reference.

**.NET CIL** has a similar verifier, but with an interesting twist: CIL supports *unverifiable* code marked with the `unsafe` keyword. Unverifiable code can perform pointer arithmetic and bypass type safety, but requires explicit trust from the runtime configuration. This creates a spectrum of safety: fully verifiable managed code, verifiable-but-uses-pointers code, and explicitly unverifiable code.

### Typed Without Verification

**Dalvik/ART** bytecode carries type information and is verified at install time (dex2oat), but the verification is practically mandatory only because Android controls the entire pipeline. The DEX format encodes full type signatures for methods and fields.

**Dis VM** takes a middle path: type descriptors contain bit vectors indicating which offsets in a data structure are pointers (for GC traversal), and module linking uses MD5 hashes of type signatures. But there is no full bytecode verifier — the system trusts the Limbo compiler. This is adequate for a single-source-language VM but would not work for a multi-language or adversarial environment.

### Untyped

**Q3VM**, **Lua**, and **Smalltalk-80** have no type information in their bytecode. Stack values are raw 32-bit words (Q3VM), tagged unions (Lua), or object references (Smalltalk). Instructions determine interpretation: Q3VM's `ADD` and `ADDF` both pop two values but interpret the bits differently — the operand stack is just an array of `int` and the float opcodes cast through `(float*)opStack`. Lua's register values are self-describing (each carries a type tag at runtime), but the bytecode itself does not constrain what types a register may hold.

For Q3VM, this is acceptable — the bytecode comes from id Software's trusted LCC compiler and runs in a sandbox with no escape. For Lua, it is explicitly documented as a security boundary violation: "malicious bytecode can crash the VM." Loading untrusted Lua bytecode is not safe.

**BEAM** added a bytecode validator in OTP 21 that checks register liveness and type consistency within basic blocks — a middle ground between no verification and full type verification. The validator runs at compile time, and the loader performs structural validation.

### Verification Cost

The cost of verification is not negligible. A single-pass O(n) validator (Wasm) adds milliseconds. The JVM's verifier with stack map frames is also effectively O(n) but with larger constants due to the need to track more complex type hierarchies. Without stack map frames (class file versions before 50.0), JVM verification required iterative dataflow analysis — O(n²) in the worst case.

The engineering lesson: if you want cheap verification, design the bytecode format for it from the start. Wasm's structured control flow makes validation trivial. The JVM's arbitrary `goto` made it hard. Retrofitting verification onto a format not designed for it (Lua, Python) is either impossible or not worth the effort.

## Memory Models

How does the VM manage memory? This choice determines garbage collection overhead, interop with native code, security boundaries, and the programming model available to the bytecode.

### Garbage-Collected Managed Heap

The JVM, Dalvik/ART, .NET CIL, Smalltalk-80, Lua, and BEAM all use garbage-collected heaps. The specifics vary enormously:

| System | GC Strategy | Pauses | Special Properties |
|--------|-------------|--------|--------------------|
| JVM (ZGC) | Concurrent, region-based | Sub-millisecond | Colored pointers, load barriers |
| JVM (G1) | Generational, region-based | Low milliseconds | Predictable pause targets |
| Dalvik/ART | Concurrent copying | Single pause point | Compacting, reduces fragmentation |
| .NET | Generational (3 gens + LOH) | Low milliseconds | Value types on stack avoid GC |
| Smalltalk-80 | Mark-sweep (originally) | Variable | Image-based persistence |
| Lua 5.4 | Incremental mark-sweep | Amortized | Generational mode option |
| BEAM | Per-process copying | Per-process only | No global stop-the-world |

BEAM's per-process GC is architecturally unique: each Erlang process has its own heap (default ~233 words initial), collected independently. No process is ever paused by another process's garbage collection. The cost is data copying on message passing — sending a message copies the data into the recipient's heap — but for a system designed around millions of concurrent processes with soft real-time requirements, per-process GC is a decisive advantage.

.NET's value types (structs) are the most significant GC optimization in any managed runtime: a `struct` lives on the stack or inline within its containing object, with zero GC overhead. Combined with `Span<T>` and `stackalloc`, .NET allows performance-critical code to avoid allocation entirely without leaving the managed world.

### Hybrid: Reference Counting Plus Mark-Sweep

The Dis VM uses both reference counting and mark-and-sweep GC. Reference counting provides immediate reclamation when the last reference to an object is dropped — useful for resource cleanup (closing files, releasing channels). Mark-and-sweep handles reference cycles that reference counting misses. This dual approach is conceptually clean but adds overhead: every pointer assignment must update reference counts, and the cycle collector must still run periodically.

### Linear Memory

WebAssembly and Q3VM use a fundamentally different model: a flat, contiguous, byte-addressable array of memory. No objects, no pointers, no garbage collection. Wasm's linear memory is bounds-checked on every access — the engine traps on out-of-bounds reads or writes. Q3VM's memory is similarly bounded. Within the sandbox, the program manages memory however it wants (malloc/free, arena allocation, or nothing).

This model has three critical properties:

1. **No GC pauses.** Memory management is entirely the program's responsibility. A game running in Wasm will not stutter because the garbage collector kicked in.

2. **Predictable performance.** Memory access has deterministic cost — a bounds check plus a load or store. No object header overhead, no indirection through GC handles.

3. **C/C++ compatibility.** Linear memory maps directly to C's memory model: a flat address space with pointers as integer offsets. This is why Wasm can run Clang-compiled C/C++ with minimal shims.

The trade-off is that the program must manage memory manually, and bugs that would be caught by a GC (use-after-free, double-free, memory leaks) manifest as they do in native C: silently wrong behavior within the sandbox. But the sandbox contains the damage — a buffer overflow in Wasm cannot corrupt the host.

### Explicit Memory (Compiler IR)

LLVM IR exposes raw memory operations: `alloca` for stack slots, `load`/`store` for memory access, `getelementptr` for pointer arithmetic, `fence`/`cmpxchg`/`atomicrmw` for concurrency. There is no GC and no safety net. This is appropriate for a compiler IR — the target language's runtime provides whatever memory management it needs — but it means LLVM IR is not suitable as a sandboxed execution format.

## Compilation Strategies

The path from bytecode to native execution determines startup time, peak performance, and memory overhead. But between pure interpretation and full JIT compilation lies an often-overlooked middle ground: interpreter dispatch optimization.

### Interpreter Dispatch: Switch, Computed Goto, and Threaded Code

Before considering JIT compilation, the interpreter dispatch mechanism itself has a significant impact on performance. The simplest approach is a `switch` statement in a loop:

```c
while (1) {
    switch (code[pc++]) {
    case OP_ADD: val = pop() + pop(); push(val); break;
    case OP_CONST: push(code[pc++]); break;
    /* ... */
    }
}
```

A decent compiler turns this into a single indirect branch through a jump table. But because all opcodes share that one branch instruction, the CPU's branch predictor has a single entry covering all opcode transitions — it will almost always mispredict, flushing the pipeline. On modern CPUs with deep pipelines, a mispredicted branch costs 10–20 cycles.

**Computed goto** (GCC's labels-as-values extension) solves this by giving each opcode handler its own indirect jump:

```c
static void *dispatch[] = { &&op_add, &&op_const, /* ... */ };
#define DISPATCH() goto *dispatch[code[pc++]]

op_add:
    val = pop() + pop(); push(val);
    DISPATCH();
op_const:
    push(code[pc++]);
    DISPATCH();
```

Each handler ends with its own `goto *dispatch[...]` instruction at a different address. The branch predictor maintains a separate prediction for each of these jump sites — effectively predicting the *next* opcode for each current opcode. For bytecode with any regularity (loops, common sequences), these per-site predictions are far more accurate than a single shared prediction.

The technique was popularized by Eli Bendersky's 2012 analysis and is used by **CPython** (15–20% speedup), **Ruby's YARV**, **Dalvik**, and the standalone **Q3VM** interpreter (jnz/q3vm). Q3VM's implementation is representative: a `goto_OP_*` label for each of 60 opcodes, a 64-entry dispatch table of `&&label` addresses, and a `DISPATCH()` macro that reads TOS/NIS registers and jumps. When compiled without GCC, the same code falls back to `switch/case` via preprocessor macros that redefine `goto_OP_ADD` as `case OP_ADD`. The Q3VM project's own benchmarks show the impact clearly:

| Dispatch | Time | Relative |
|----------|------|----------|
| Switch (`-O2`, GCC 7) | 3.063 s | 1.00x |
| Computed goto (`-O2`, GCC 7) | 1.771 s | 1.73x |
| Native C (`-O2`, GCC 7) | 0.307 s | 9.98x |

Computed goto is **73% faster** than switch for Q3VM — even larger than CPython's 15–20% gain, likely because Q3VM's opcodes are simpler (less work per dispatch, so dispatch overhead dominates more). The interpreted Q3VM with computed gotos runs at roughly 17% of native speed, or about **5.8x slower than native C**.

The portability caveat: computed goto requires GCC, Clang, or ICC. MSVC does not support labels-as-values. Any VM using this technique needs a switch fallback for MSVC, exactly as Q3VM implements.

**Threaded code** (Forth-style) goes further by embedding dispatch pointers directly in the instruction stream. Direct threading stores the *address* of each handler as the instruction itself — no opcode lookup needed, just `goto *(*ip++)`. Indirect threading adds one level of indirection. These techniques predate computed goto by decades (Forth, 1970) but require the instruction stream to be pre-processed into native pointers, losing portability.

### Interpretation Only

UCSD P-code was purely interpreted with simple switch dispatch, achieving roughly 10–20x slowdown versus native code. This was acceptable in 1978 when the alternative was rewriting software for every new processor. Pure interpretation is simple to implement and portable, but the performance ceiling is low.

Standard Lua is interpreted (LuaJIT is a separate project). Lua uses switch dispatch despite being performance-conscious — Roberto Ierusalimschy considered computed goto but chose portability. Lua's fixed-width 32-bit instructions make its interpreter loop unusually efficient — the core VM (`lvm.c`) is ~1,800 lines — but performance still lands at 10–30x slower than C for compute-heavy code.

### JIT Compilation

JIT compilation translates bytecode to native code at runtime. The approaches span a wide range:

**Trace-based JIT** (LuaJIT, original Dalvik): Records a linear trace of hot execution paths and compiles the trace. Simple to implement, excellent for tight loops, but struggles with branchy code and polymorphic call sites.

**Method-based JIT** (HotSpot C2, RyuJIT): Compiles entire methods. More complex but handles control flow better. HotSpot's C2 compiler performs aggressive optimizations (escape analysis, loop unrolling, inlining) that rival AOT compilers.

**Tiered JIT** (HotSpot, ART, .NET): Starts with interpretation or a fast baseline JIT (low optimization, fast compilation), then recompiles hot methods with an optimizing JIT. HotSpot's tiers: interpreter → C1 (client JIT) → C2 (server JIT). .NET: interpreter → tier 0 (quick JIT) → tier 1 (optimized JIT with PGO).

**Streaming JIT** (V8 Liftoff for Wasm): Compiles Wasm bytecode to native code *while the module is still downloading*. Each function is compiled independently in a single forward pass. This drastically reduces startup time for large Wasm modules.

**Always-on JIT** (BEAM since OTP 24): BeamAsm compiles all BEAM bytecode to native code, with no interpreter tier. The JIT is simple (no heavy optimization) but provides a consistent 20–40% improvement over the previous threaded interpreter.

**Load-time JIT** (Dis, original Quake 3): The original Quake 3 engine included an x86 JIT (~1,500 LOC in ioquake3) that macro-expanded bytecodes to native instructions at module load time. Dis allows per-module flags (MUSTCOMPILE, DONTCOMPILE) to control JIT behavior. These are simple, non-optimizing JITs. Note that the standalone Q3VM project (jnz/q3vm) omits the JIT entirely, relying on computed goto dispatch instead — and still achieves 8.9x faster execution than the Triseism Q3 interpreter, demonstrating that dispatch optimization alone can close much of the gap.

### AOT Compilation

AOT (Ahead-of-Time) compilation produces native binaries before execution:

**LLVM** is fundamentally an AOT system — `llc` compiles IR to native code for the target architecture. This is LLVM's raison d'être.

**GraalVM Native Image** compiles JVM bytecode to standalone native binaries with no JVM dependency. Startup is sub-millisecond instead of seconds. The trade-off: no dynamic class loading, no runtime reflection without configuration, and peak throughput may be lower than a warmed-up JIT.

**ART's hybrid model** (Android 7.0+) is the most sophisticated: AOT-compile at install time using profile data from previous runs, JIT-compile newly encountered hot paths, and store those profiles for the next AOT pass. This closed-loop optimization adapts to actual usage patterns.

**.NET NativeAOT** (since .NET 7) produces self-contained native binaries. Like GraalVM, it sacrifices some dynamic capabilities for startup time and deployment simplicity.

**Wasm AOT** is supported by Wasmtime, Wasmer, and WAMR. `wasm2c` takes a different approach — it compiles Wasm to portable C code, which any C compiler can then optimize for the target platform.

### The Compilation Tier Stack

| System | Tier 0 | Tier 1 | Tier 2 | Peak vs. Native C |
|--------|--------|--------|--------|-------------------|
| JVM (HotSpot) | Interpreter | C1 JIT | C2 JIT | 0.8–1.2x |
| .NET | Interpreter | Quick JIT | Optimized JIT + PGO | 0.8–1.1x |
| ART | Interpreter | JIT | AOT (profile-guided) | 0.7–1.0x |
| Wasm (V8) | Liftoff (baseline) | TurboFan (optimizing) | — | 0.7–0.9x |
| LuaJIT | Interpreter | Trace JIT | — | 0.2–0.5x |
| BEAM | JIT (always-on) | — | — | 0.2–0.5x (sequential) |
| Q3VM | Computed goto interpreter | — | — | 0.15–0.20x |
| Lua 5.4 | Switch interpreter | — | — | 0.03–0.1x |

The numbers reveal a clear pattern: multi-tier JIT systems with profile-guided optimization approach native performance; single-tier JITs reach 50–80% of native; optimized interpreters (computed goto) reach 15–20% of native; and plain switch interpreters top out at 3–10% of native for compute-heavy workloads.

## Sandboxing and Security

If you are executing code from an untrusted source — a browser loading a web page, a game engine loading a mod, a cloud platform running a customer's function — the VM must guarantee that the code cannot escape its sandbox.

### Designed for Sandboxing

**WebAssembly** was built from the ground up as a sandbox. The security properties are structural, not bolted on:

- **No ambient access.** A Wasm module has no filesystem, network, or system call access unless the host explicitly provides imported functions. The module cannot even learn the current time without an import.
- **Linear memory isolation.** The module's memory is a contiguous array that cannot address host memory. Out-of-bounds access traps.
- **Structured control flow.** No computed jumps to arbitrary addresses. Branch targets are always statically known (labels within blocks). This prevents ROP (return-oriented programming) attacks.
- **Type-checked function calls.** Indirect calls go through a type-checked table. Calling a function with the wrong signature traps.

**Q3VM** achieves similar isolation with much less sophistication but a clever implementation. Code and data occupy separate address spaces (Harvard architecture), preventing self-modifying code. The data region is rounded to the next power of two, producing a `dataMask` — every memory access is masked with `image[addr & dataMask]`, a single bitwise AND that is cheaper than a bounds comparison and branch. The opcode index is similarly masked (`opcode & 0x3F`) to prevent escape via invalid opcode values. Jump targets are pre-validated during loading. System calls use a trap mechanism — calling with a negative program counter invokes a host-provided callback. The host translates VM addresses to real pointers via a `VMA()` macro and can validate memory ranges with `VM_MemoryRangeValid()` to prevent sandbox escape through syscall arguments. The sandbox is simpler than Wasm's but sufficient for its purpose: game mods cannot corrupt the engine.

### Designed for Safety (Not Sandboxing)

The **JVM** and **.NET CIL** provide *type safety* — bytecode verification ensures you cannot forge object references, access private fields, or violate type boundaries. But type safety is not sandboxing. The JVM's `SecurityManager` (now removed in JDK 24) attempted to layer sandboxing on top of type safety, but it was plagued by escape vulnerabilities for its entire existence. The bytecode verifier prevents *accidents*; it was never sufficient to contain *adversaries*.

.NET's Code Access Security (CAS) had the same trajectory — conceptually sound, practically deprecated.

### Module-Level Isolation

The **Dis VM** uses module signatures (MD5 hashes of type descriptors) to ensure that modules agree on their interfaces. Modules can be digitally signed. Per-process namespaces (inherited from Plan 9) provide filesystem-level isolation. This is not a bytecode sandbox — it is an operating system security model applied to VM modules.

**BEAM** provides *process isolation* — each Erlang process has its own heap, cannot access another process's memory, and communicates only through message passing. This is not sandboxing against malicious code (Erlang's NIF interface allows arbitrary C code), but it provides fault isolation: a crashing process cannot corrupt other processes.

### No Sandboxing

**LLVM IR** provides no sandboxing at all — it can express arbitrary memory access, inline assembly, and system calls. **Lua**'s bytecode can crash the interpreter if loaded from an untrusted source. **Smalltalk-80** runs in a shared image with no isolation between objects beyond the language-level message dispatch.

### The Sandbox Spectrum

| System | Sandbox Level | Threat Model |
|--------|--------------|--------------|
| WebAssembly | Full sandbox | Adversarial code from the internet |
| Q3VM | Memory sandbox | Untrusted game mods |
| JVM / CIL | Type safety only | Accidental type confusion (not adversaries) |
| Dis VM | Module signatures + OS namespaces | Cooperating but mutually suspicious modules |
| BEAM | Process isolation | Fault tolerance (not security) |
| Lua, Smalltalk, LLVM | None | Trusted code only |

The critical insight: sandboxing is an *architectural* property that must be designed in from the beginning. The JVM's attempt to retrofit sandboxing via `SecurityManager` failed after twenty years. WebAssembly's sandbox works because the bytecode format itself makes escape structurally impossible — you cannot construct an instruction sequence that addresses host memory, because the instruction set does not support it.

## Implementation Complexity

How much code does it take to implement a working VM? This determines feasibility for embedded systems, hobby projects, educational use, and any context where a minimal, auditable implementation matters.

| System | Minimal Interpreter (LOC) | Full Runtime (LOC) | Key Complexity Drivers |
|--------|--------------------------|--------------------|-----------------------|
| Q3VM | ~700 (dispatch loop) | 2,011 (vm.c) + 287 (vm.h) | Almost nothing to implement |
| UCSD P-code | 1,000–2,000 | 3,000–5,000 | Simple stack machine |
| Smalltalk-80 | 1,500–3,000 | 10,000–20,000 | GC, method lookup, image format |
| Lua 5.4 | 1,500–2,500 | 30,000 (full language) | GC, tables, strings, metatables |
| microjvm | ~60 (3 opcodes!) | ~1,900 (barely runs Hello World) | JVM complexity defeats minimalism |
| Wasm MVP | 2,000–4,000 | 200,000+ (Wasmtime) | Validation, linear memory, imports |
| Dis VM | 3,000–5,000 | 20,000–30,000 | Address modes, refcount+GC, JIT |
| JVM | 3,000–5,000 | 250,000+ (HotSpot) | Class loading, verification, GC, JIT |
| .NET CIL | 5,000–8,000 | 2,000,000+ (CoreCLR) | Generics, value types, JIT, GC |
| Dalvik/ART | 5,000–8,000 | 500,000+ (ART) | Register allocation, DEX format, GC |
| BEAM | 10,000–15,000 | 500,000+ (OTP) | Pattern matching, process scheduling, distribution |
| LLVM IR | 5,000–10,000 (reader) | 3,000,000+ (LLVM) | Optimization passes, target codegen |

Q3VM's implementation simplicity is its defining feature. The standalone Q3VM project (jnz/q3vm, forked from ioquake3) puts the entire VM in a single file: `vm.c` at 2,011 lines including the loader, instruction preparation, computed goto dispatch, debug support, and all 60 opcode handlers. The header `vm.h` adds 287 lines of API, structs, and error codes. The dispatch loop itself — from the first `DISPATCH()` to the last opcode handler — is roughly 700 lines. There is no type system, no verifier, no GC, and no JIT. A competent programmer can read and understand the entire VM in an afternoon. This is why id Software chose the architecture: as John Carmack wrote in his 1998 `.plan` file, "a virtual RISC-like CPU... loads and stores are confined to a preset block of memory, and access to all external system facilities is done with system traps to the main game code, so it is completely secure."

The contrast with microjvm (0xaa55h/microjvm) is instructive. This project attempts a minimal JVM in C — all 205 JVM opcodes are defined in its header, the class file parser works, and the frame/stack machinery is in place. But the actual bytecode interpreter (`isa_exec.c`) implements only three opcodes: `GETSTATIC`, `LDC`, and `INVOKESTATIC`. That is enough to print "Hello, World!" and nothing more. Nearly 1,900 lines of C produces a VM that cannot add two integers. The JVM's complexity — constant pool resolution, class loading, method dispatch, type verification — cannot be meaningfully reduced. Q3VM can be minimal because its instruction set is minimal. The JVM cannot.

At the other extreme, LLVM is not really a "VM" at all — it is a compiler framework. Its three million lines of code include target-specific code generators for over twenty architectures, hundreds of optimization passes, a linker, debugger integration, and more. Comparing its LOC to Q3VM's is comparing a factory to a hand tool.

WebAssembly sits at an interesting middle point. The wasm3 interpreter (a fast, portable Wasm interpreter designed for embedded use) is approximately 5,000 lines of C — competitive with a minimal JVM interpreter. But a production Wasm runtime with JIT (Wasmtime, Wasmer) is 200,000+ lines, driven by the complexity of optimizing compilation and the WASI system interface.

The Lua number is deceptive: 30,000 lines covers the *entire language* — parser, compiler, VM, GC, standard library, and C API. The VM core alone (`lvm.c`) is ~1,800 lines, making Lua's interpreter one of the most code-efficient register machines ever built. This is Lua's primary engineering achievement: not raw performance, but functionality per line of code.

## Performance Characteristics

Performance comparisons across VMs are fraught with methodological problems — workload choice, warmup effects, GC behavior, and measurement methodology all influence results. The following represents consensus from published benchmarks and engineering reports, not a single controlled experiment.

### Compute-Bound Performance (Relative to Native C = 1.0)

| System | Interpreted | JIT/AOT | Notes |
|--------|-------------|---------|-------|
| LLVM-generated native | — | 0.95–1.05 | The reference AOT compiler |
| JVM (HotSpot C2, warmed) | 0.03–0.10 | 0.8–1.2 | Can exceed C via profile-guided devirtualization |
| .NET (RyuJIT, warmed) | 0.03–0.10 | 0.8–1.1 | Value types help numeric code |
| ART (AOT, profile-guided) | 0.03–0.10 | 0.7–1.0 | Hybrid AOT+JIT |
| Wasm (V8 TurboFan) | — | 0.7–0.9 | Bounds checks cost ~5–10% |
| Dis VM (JIT) | 0.05–0.10 | 0.5–0.8 | Limited optimization |
| LuaJIT | 0.03–0.10 | 0.2–0.5 | Trace JIT; fast for traces, degrades on branches |
| BEAM (JIT) | — | 0.2–0.5 | Sequential performance; strength is concurrency |
| Q3VM (computed goto) | 0.15–0.20 | — | No JIT in standalone; dispatch optimization only |
| Lua 5.4 (interpreted) | 0.03–0.10 | — | No standard JIT |
| UCSD P-code | 0.05–0.10 | — | Historical baseline |

The JVM and .NET can *exceed* native C performance in specific scenarios: profile-guided devirtualization eliminates virtual call overhead that C++ pays at every polymorphic call site, and escape analysis can eliminate heap allocations that C/C++ programmers must explicitly manage. These optimizations are only available with runtime profiling data, which AOT compilers lack.

WebAssembly pays a consistent ~5–10% overhead for bounds checking on memory access, plus the overhead of structured control flow (branches cannot fall through, switches generate nested `br_table` constructs instead of computed jumps). The SIMD extension narrows the gap for data-parallel workloads.

BEAM's numbers require context: 0.2–0.5x native C for sequential code sounds poor, but BEAM is not competing on sequential throughput. Its value is in concurrent workloads — millions of lightweight processes with soft real-time scheduling and per-process GC. A sequential benchmark misses the point.

### Startup Time

| System | Cold Start | Notes |
|--------|-----------|-------|
| Q3VM | <1 ms | Load + optional JIT compile |
| Wasm (Liftoff) | ~1–5 ms | Streaming baseline compilation |
| Lua 5.4 | <1 ms | Parse/compile source, or load bytecode |
| BEAM | ~10–50 ms | Load + JIT compile all modules |
| JVM (HotSpot) | 100–500 ms | Class loading, verification, interpreter start |
| JVM (GraalVM Native Image) | <5 ms | AOT-compiled, no JVM overhead |
| .NET | 50–200 ms | Assembly loading, JIT tier 0 |
| .NET (NativeAOT) | <5 ms | Self-contained native binary |
| ART | 50–200 ms | DEX loading, may interpret before AOT profile available |

Startup time is where AOT compilation and simple VMs dominate. The JVM's cold start — loading hundreds of classes, verifying bytecode, interpreting before the JIT kicks in — is its most consistent criticism. GraalVM Native Image and .NET NativeAOT solve this at the cost of losing some dynamic capabilities.

## Practical Limitations

Every bytecode system has hard constraints that limit its applicability. Knowing these limits upfront prevents choosing a system that will fail at the edge cases your project requires.

### Address Space and Memory Limits

- **Q3VM**: Fixed memory allocation at load time. No dynamic allocation within the VM. Total memory typically a few megabytes. No 64-bit integers, no double-precision floats.
- **Wasm MVP**: 32-bit linear memory, maximum 4 GB. The memory64 proposal lifts this but is post-MVP. Each memory access is bounds-checked.
- **JVM**: 64 KB maximum method size (bytecode length per method). 255 maximum parameters per method. No unsigned integer types (until recent proposals). Erasure-based generics — type parameters are erased at runtime.
- **Dalvik**: 65,536 method reference limit per DEX file. Large apps require multidex. Method reference count, not method count, is the constraint.
- **UCSD P-code**: 16-bit address space, ~64 KB. The defining constraint of its era.
- **Lua**: 255 register limit per function (8-bit register index field). 200 local variable limit. Bytecode is not portable across architectures (endianness and sizes embedded).

### Missing Features (at the Bytecode Level)

| Feature | JVM | CIL | Wasm MVP | Q3VM | Dalvik | Dis | Lua | BEAM |
|---------|-----|-----|----------|-----|--------|-----|-----|------|
| Tail calls | No* | Implementation-dependent | Post-MVP proposal | No | No | Yes | Yes (via loop) | Yes |
| Exceptions | Yes | Yes | Post-MVP proposal | No | Yes | Yes (alt stack) | Yes (pcall) | Yes (try/catch) |
| Threads | Yes | Yes | Post-MVP proposal | No | Yes (via OS) | Yes (channels) | No (coroutines) | Yes (lightweight) |
| GC | Yes | Yes | Post-MVP proposal | No | Yes | Yes | Yes | Yes |
| SIMD | No (auto-vectorized by JIT) | Yes (Vector<T>) | Yes (128-bit) | No | No | No | No | No |
| Closures | Yes (invokedynamic) | Yes (delegates) | No (import functions) | No | Yes | Yes | Yes (upvalues) | Yes (funs) |
| 64-bit integers | Yes (long) | Yes (int64) | Yes (i64) | No | Yes (long) | Yes (big) | Yes (5.3+) | Yes (bignum) |
| Double-precision float | Yes | Yes | Yes (f64) | No | Yes | Yes | Yes | Yes |

*JVM: tail call optimization is not guaranteed by the spec, though some JVMs implement it.

Q3VM's limitations are the most severe — no 64-bit integers, no doubles, no dynamic allocation, no threads, no exceptions, no closures. This is by design: Q3VM runs Quake 3 game logic (AI, scoring, game rules), not general-purpose computation. The simplicity that makes it auditable in an afternoon makes it unsuitable for anything beyond its intended domain.

Wasm MVP's missing features (GC, exceptions, threads, tail calls) are all addressed by post-MVP proposals in various stages of standardization. The Component Model and WASI proposals aim to add capability-based system access. The roadmap is well-defined, but "post-MVP" means production applications today must work around these limitations — typically by compiling a GC runtime or exception handler into the Wasm module itself, increasing module size.

### Ecosystem and Portability

| System | Source Languages | Target Platforms | Standardization |
|--------|-----------------|------------------|-----------------|
| JVM | Java, Kotlin, Scala, Clojure, Groovy, ~350+ | Any with JVM | JVM Spec (Oracle) |
| CIL | C#, F#, VB.NET, ~50+ | Windows, Linux, macOS, mobile | ECMA-335, ISO/IEC 23271 |
| Wasm | C, C++, Rust, Go, AssemblyScript, ~40+ | Browsers, WASI runtimes, embedded | W3C standard |
| LLVM IR | C, C++, Rust, Swift, Zig, ~20+ | ~20+ native targets | None (LLVM project) |
| Dalvik/ART | Java, Kotlin | Android | Google (de facto) |
| Lua | Lua only | Anywhere with C compiler | Lua.org (de facto) |
| Dis | Limbo only | Inferno OS | Vita Nuova (defunct) |
| BEAM | Erlang, Elixir, Gleam, LFE | Any with BEAM | Ericsson (de facto) |
| Q3VM | C (via LCC, restricted subset) | Quake 3 engine, standalone projects | GPL 2 (id Software, ioquake3, jnz/q3vm) |

The JVM and .NET CIL have the richest multi-language ecosystems. Wasm is catching up rapidly — its language-agnostic design and browser deployment make it the most broadly targeted new format. LLVM IR is not a deployment format but has the widest *input* language support.

Dis is the cautionary tale of ecosystem lock-in. It is technically elegant — memory-to-memory three-address, channel concurrency, hybrid GC — and it is effectively dead because Limbo was its only source language and Inferno was its only platform.

Q3VM tells a more nuanced story. The format is tied to a restricted subset of C compiled by LCC, and it originated inside a single game engine. But the bytecode format's simplicity — 60 opcodes, no GC, no type system, auditable in an afternoon — has given it a second life. The standalone jnz/q3vm project extracts the VM from ioquake3 into a single embeddable C file with no game dependencies. Other independent implementations exist — OrangeTide/stackvm implements all 60 opcodes, loads `.qvm` files, and provides a syscall environment in 1,374 lines of C with no code golf tricks. Q3VM demonstrates that a bytecode format can outlive its original application if it is simple enough to reimplement from scratch.

## Design Philosophy

The eleven systems in this survey embody five distinct philosophies about what a bytecode VM should be:

**The platform VM** (JVM, CIL): A complete runtime environment with garbage collection, type safety, class loading, reflection, and a rich standard library. The bytecode is the contract between languages and the platform. The goal is a universal execution substrate. Complexity is high but amortized across a vast ecosystem.

**The sandbox VM** (Wasm, Q3VM): A minimal, secure execution environment for untrusted code. The bytecode provides strong isolation guarantees with minimal runtime services. The program manages its own memory, handles its own errors, and interacts with the host only through explicit imports/traps. Complexity ranges from trivial (Q3VM) to moderate (Wasm).

**The compiler IR** (LLVM): A representation designed for analysis and transformation, not execution. Exposes maximum information to the optimizer at the cost of portability and security. Not a VM in the traditional sense.

**The language VM** (Lua, Smalltalk, Dis, BEAM): A runtime tailored to a specific language or language family. The bytecode reflects the source language's semantics — Lua's tables, Smalltalk's message sends, Dis's channels, BEAM's processes. Efficiency comes from fitting the abstraction to the language, at the cost of multi-language versatility.

**The portability VM** (UCSD P-code): A lowest-common-denominator instruction set designed to run on any hardware. Performance is sacrificed for reach. This philosophy succeeded brilliantly in the 1970s and lives on in Wasm's design, though Wasm achieves both portability *and* near-native performance.

## Choosing a Bytecode System

For an engineer making an architectural decision, the choice reduces to a few questions:

**Do you need to run untrusted code?** Then your choices are WebAssembly or a purpose-built sandbox like Q3VM. The JVM's bytecode verifier provides type safety but not sandboxing. Nothing else in this survey offers meaningful isolation.

**Do you need multi-language support?** JVM and CIL have the richest ecosystems. Wasm is language-agnostic by design and growing rapidly. LLVM is the multi-language answer for AOT compilation.

**Do you need minimal implementation complexity?** A Q3VM-compatible interpreter can be built in under 1,500 lines of C. Lua's interpreter core is ~1,800 lines. A Wasm MVP interpreter is 2,000–4,000 lines. Everything else requires significantly more.

**Do you need peak performance?** JVM with HotSpot C2, .NET with RyuJIT, or LLVM-generated native code. All approach or match C/C++ performance for sustained workloads.

**Do you need fast startup?** Q3VM, Lua, and Wasm with streaming compilation start in under 5 milliseconds. The JVM and .NET require AOT compilation (GraalVM Native Image, NativeAOT) to match this.

**Do you need a specific concurrency model?** BEAM for actor-based concurrency with fault tolerance. Dis for channel-based concurrency (CSP style). Everything else relies on OS-level threading.

**Do you need to target embedded systems?** Lua and Wasm (via wasm3 or WAMR) have the smallest footprints. Q3VM is tiny but too limited for general use. The JVM has embedded profiles (Java ME, now GraalVM) but they are not small.

## Conclusion

The fifty-year history of bytecode virtual machines has not produced a winner. It has produced a taxonomy: platform VMs for ecosystems, sandbox VMs for security, compiler IRs for optimization, language VMs for semantics, and portability VMs for reach. Each design makes a bet about which property matters most, and each bet pays off in its intended domain.

What the survey reveals most clearly is the cost of retrofitting. The JVM spent twenty years trying to add sandboxing to a format designed for type safety — and gave up, removing `SecurityManager` entirely. Wasm's structured control flow makes verification trivial because it was designed that way from the start; the JVM's arbitrary `goto` made verification expensive because it was not. Q3VM's 60-opcode instruction set is both its greatest strength (auditable in an afternoon) and its greatest limitation (no 64-bit integers, no doubles, no dynamic allocation). These are not bugs — they are the consequences of design choices that were correct for their original context and cannot be cheaply changed later.

For anyone building a new bytecode system today, the lessons are:

1. **Decide your threat model first.** If you need to sandbox untrusted code, design the instruction set for it — structured control flow, mandatory validation, no ambient access. You cannot bolt this on later.

2. **Choose stack or register based on your execution model.** If the primary consumer is an interpreter, register-based reduces dispatch overhead. If the primary consumer is a JIT or AOT compiler, the choice barely matters — both get converted to SSA form anyway.

3. **Design for validation from day one.** Single-pass O(n) validation (Wasm's approach) is achievable if you constrain the control flow. Once you allow arbitrary jumps, you are committed to iterative dataflow analysis.

4. **Minimize the instruction set.** Q3VM proves that 60 opcodes suffice for a useful VM. Wasm's MVP has ~172. The JVM's 205 opcodes include many that exist only for code density optimization (`aload_0` vs. `aload 0`). More opcodes means more implementation surface, more validation complexity, and more room for bugs.

5. **The ecosystem matters more than the bytecode.** Dis is technically elegant — memory-to-memory three-address, channel concurrency, hybrid GC — and it is effectively dead because Limbo was its only source language and Inferno was its only platform. The JVM's bytecode format is mediocre (stack-based, no value types until Valhalla, erasure-based generics), but its ecosystem makes it the most commercially important bytecode format ever created.

The right bytecode is the one whose trade-offs match your constraints. This survey aims to make those trade-offs visible.
