---
title: Virtual Machine Bytecodes
date: 2026-03-22
revised: 2026-04-19
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
| CPython | 1990 | Guido van Rossum | Python reference interpreter | Stack, fixed 2-byte wordcode (3.6+) |
| V8 Ignition | 2016 | Google | JavaScript baseline tier | Register + accumulator |
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

### Address Spaces: Byte Offsets vs. Instruction Ordinals

A subtler encoding choice — and one that aged poorly for Q3VM — is what address space branch and call targets live in. Most modern VMs pick one of two answers and apply it uniformly:

- **Byte offsets** (JVM, .NET CIL, Wasm, Dalvik). The program counter is a byte index into the code segment. Branch targets are byte offsets (JVM, Wasm) or relative byte deltas (CPython 3.11+, Lua). The loader does no address translation; the disassembler, the debugger, and the interpreter all agree on what an "address" means. CPython went from variable-width to fixed 2-byte "wordcode" in 3.6 specifically to make `pc++` trivial and dispatch branch-predictor-friendly.
- **Structured branch depths** (Wasm). Wasm goes a step further: branches name a relative depth into the structured control stack rather than any raw offset. This sidesteps the question entirely but only works because Wasm forbids arbitrary `goto`.

Q3VM picked a third, rarer option: branch and call targets are **instruction ordinals** — the index of the Nth opcode in the program, not its byte offset. `q3asm` emits these ordinals into the `.qvm` file directly. Inside a single run this is self-consistent (PC is also an ordinal), but it forces the loader to expand the variable-length on-disk encoding into a fixed-width array — typically 8 bytes per opcode (opcode word + parameter word), an ~8x memory blowup over the file size, plus rounding up to a power of two and padding the tail with `BREAK` so a stray PC can be masked safely. The expansion exists so the interpreter can do `pc++` and `code[pc]` without ever parsing instruction boundaries at runtime.

The cost is paid in three places:

1. **Memory.** A 10 KB code segment becomes ~80 KB live, plus pow2 padding.
2. **Tooling coherence.** The on-disk format and any disassembler naturally speak byte offsets; the interpreter speaks ordinals. Anything that straddles both — stack traces, breakpoints, source-line tables — must maintain a parallel map. In practice Q3VM tooling rarely does, so disassembly addresses and runtime PC values do not line up with what `q3asm` emitted.
3. **Forecloses optimizations.** Threaded dispatch, superinstructions, and JITs all want to see the real byte stream (or their own IR built from it), not a fixed 8-byte intermediate.

The choice is partly an artifact of Q3VM's lineage: it was designed as a backend for LCC, whose IR is instruction-oriented, and `q3asm` followed the path of least resistance. 1999-era Java made similar fixed-width-immediate choices, but Sun used byte offsets for branch targets, sparing the JVM this particular wart. The mainstream picked byte offsets because they compose with everything else — file offsets, memory addresses, debugger line tables, exception tables. Instruction ordinals only make sense inside the interpreter.

The lesson generalizes: pick one address space for code and use it everywhere — file, memory, debugger, interpreter. A format that uses a different address space at runtime than on disk pushes a translation table onto every tool that touches it.

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

## Garbage Collection Root Identification

Every garbage-collected virtual machine faces the same foundational problem: when the GC runs and must trace reachable objects, which stack slots and registers contain pointers? A wrong answer is catastrophic — the GC may free an object that is still reachable (corruption or crash), or incorrectly retain an object as reachable (memory leak). The solution chosen shapes the entire runtime architecture, and the trade-offs between techniques recur across all GC'd VMs.

### Precise Maps: Stack Maps and GC Info

The safest approach — and the one chosen by production systems — is *precise* GC root identification via compiler-emitted metadata. The compiler knows which locals and temporaries are pointers because it has type information. At every point in the code where a GC could occur (a "GC-safe point"), the compiler emits a metadata table describing the stack layout.

**HotSpot's OopMaps** are the canonical example. The JVM's C++ runtime records, for each GC-safe point in a method, exactly which stack slots and registers hold object pointers. An OopMap is a compact bitfield — one bit per stack slot, 1 = pointer, 0 = non-pointer. When the GC runs and pauses a thread, it consults the OopMap for the current PC to determine which slots to trace. A method may have dozens of OopMaps, one for each loop back-edge and method call site. During a GC pause, tracing a single frame involves a table lookup by PC, then walking the bitfield.

The cost is metadata overhead: a typical method accumulates 10–50 OopMaps depending on branching and call frequency. HotSpot mitigates this by compressing OopMaps — storing deltas from the previous map (usually most bits are unchanged) and using variable-byte encoding. The JVM also uses "stack map frames" in the class file itself (an inheritance from verification), which the C2 optimizing compiler reads to avoid re-scanning the method.

**.NET's GCInfo** follows a similar pattern. The runtime stores GC information per method, encoding which registers and stack locations hold GC references. The encoding is more compact than OopMaps — .NET uses run-length encoding over the metadata, exploiting the fact that most stack slots are non-pointers. When an exception is thrown, .NET's exception unwinding machinery consults GCInfo at each frame to correctly reconstruct the set of roots.

**Wasm has no GC-safe points yet** — the original MVP forbids garbage collection in the core spec. The exception handling proposal adds explicit try/catch, but the post-MVP GC proposal (currently in development) will require similar metadata. Proposals discuss "shadow stacks" (explicit per-function lists of pointer-typed locals) or stack map tables embedded in the module, generated at compile time.

The fundamental insight: precise identification requires the compiler to emit metadata. This is why GC'd VMs must be *GC-aware* at compile time. A naïve interpreter that accepts untrusted bytecode has no way to know which slots are pointers.

### Conservative Scanning: Boehm-Demers-Weiser

Conservative GC avoids the metadata burden entirely: scan all reachable memory (stack, registers, heap) as if every word is a potential pointer. If a word's numeric value falls in the range of any allocated object, treat it as a pointer. This might lead to false retention — a 64-bit integer that *happens* to have the same bit pattern as an object address prevents that object's reclamation — but it is memory-safe (no false collection) and requires no compiler cooperation.

**Early Smalltalk systems** (Smalltalk-80 and variants) relied on conservative scanning. The image file contains objects; the interpreter walks the C call stack and heap looking for values that could be pointers. Smalltalk's object headers contain a size field, so the GC can verify that a candidate pointer points to a valid object boundary.

The **Boehm GC** library, deployed in countless C/C++ programs as the standard conservative GC for unmanaged languages, uses the same strategy: mark phase scans the heap and stack for any 32-bit or 64-bit values that look like pointers; a pointer is "valid" if it falls within a known allocation block and is suitably aligned. On modern 64-bit systems, false retention is uncommon — the probability that a random 64-bit integer happens to lie within an allocated object is small enough that most runs are unaffected.

**V8's early implementation** used conservative scanning for the C++ stack. The engine has roots in the C++ call stack (local variables in C++ code that hold `Handle<Object>` references), and early V8 scanned the entire C++ stack as if it were all pointers. This is correct but can cause false retention of any object whose address happens to appear in C++ code's temporaries or spilled registers. Modern V8 uses explicit GC root tracking (roots are registered with the heap), but older code and some embedders still rely on conservative stack scanning.

The trade-off is precision for simplicity: conservative scanning works without compiler modification, but pays a cost in false retention and unmoving objects (because you cannot update pointers whose locations you do not know with certainty).

### Tagged Values: Self-Describing Data

A third approach eliminates the need for separate metadata by making every value self-describing. Each value carries a type tag — a few bits that identify whether the word is an integer, pointer, float, etc.

**OCaml's runtime** uses immediate tagging. Every value is one 64-bit word (on a 64-bit system). If the low bit is 1, the value is an integer and the remaining 63 bits are the int's magnitude. If the low bit is 0, the value is a pointer to a block on the heap. The GC walks pointers by looking only at words with the tag bit clear. This design is elegant and eliminates the need for metadata — the GC's rules are built into the data representation. The cost is losing one bit of integer range (63-bit integers instead of 64-bit) and the cost of untagging/retagging on arithmetic operations.

**Lua's tagged unions** follow the same principle: each value is a struct containing a type tag (integer, float, string, table, function, etc.) and a union holding the actual value. Pointers are tagged `TSTRING`, `TTABLE`, etc.; the GC sees the tag and knows whether to trace the pointer. Non-pointer values are tagged `TINT` or `TFLOAT` and are left alone.

**V8's SMI (Small Integer) representation** is a middle path: small integers (-2^30 to 2^30-1) are stored with a special bit pattern (low bit = 1); larger integers, floats, and all objects are heap-allocated. The GC uses the tag bit as a first-pass filter: bit 1 set means "this is a small int, not a pointer." For untagged values, V8 must use additional metadata (stack maps), but many hot paths work purely with SMIs and avoid metadata lookups.

Tagged representations are ideal for dynamically typed languages where value types are already tracked at runtime. Statically typed bytecodes (JVM, .NET) could theoretically use tagging but do not — the cost of tagging and untagging every operation outweighs the metadata cost.

### Handles and Indirect References

Some VMs trade direct pointer access for trackable references. The **Java Native Interface (JNI)** does not expose raw pointers to Java objects; instead, it uses local and global handles. A handle is an opaque 64-bit value that the JVM resolves to the actual object pointer. When the GC runs, it updates handle tables but not the handles themselves — the handles remain valid across GC.

The benefits: references remain valid even if the object is moved (the handle is updated, but the handle value is not). The costs: every access through a handle requires a dereference (the handle is a pointer to a pointer), and the handle table must be maintained.

Older **Smalltalk implementations** used a similar indirection: a "stable pointer" or "object ID" that remained valid across GC, with the actual object location tracked in a separate table. This was slower than direct pointers but allowed object relocation without invalidating C code's references.

Modern VMs largely abandoned this approach in favor of precise metadata, because the extra dereference is a hot-path cost that outweighs the flexibility.

### The Implementation in Context

The choice of root identification technique cascades through the VM design:

- **Precise maps (HotSpot, .NET, ART)**: Requires GC-aware compilation. Metadata overhead is modest on modern systems (~10–20 bytes per method). Enables fast, low-latency GC with concurrent collection. Best for long-running server workloads.
  
- **Conservative scanning (Boehm, early Smalltalk)**: Works without compiler cooperation. False retention can accumulate on long-running programs. Best for short-lived programs or embedded systems where the compiler cannot be modified.

- **Tagged values (OCaml, Lua)**: Self-describing values require no separate metadata. Works perfectly for dynamically typed languages. Cost is paid in tagging/untagging overhead, not metadata overhead. Best for languages where type tags already exist.

- **Handles (JNI, older Smalltalk)**: Trade direct access for guaranteed reference validity. Indirection cost is high; only justified when C code and objects must coexist. Modern VMs minimize JNI use for this reason.

The HotSpot/LLVM gc.statepoint infrastructure (used by many modern systems) unifies these approaches: the compiler emits maps at safepoints, but the maps are generated using LLVM's GC framework, allowing different backends to choose their own encoding (precise, conservative, hybrid). This is the modern consensus — precision via metadata, delivered by the compiler infrastructure.

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

## Exception Handling at the Bytecode Level

Exception handling in bytecode VMs appears simple at the language level — "throw" an exception, "catch" it in a handler — but its implementation cascades into the entire execution model. The choice between table-driven unwinding, explicit save-and-restore, and process-level recovery determines whether exceptions have zero cost in the non-throwing path, whether you can safely unwind destructors, and whether the VM trusts the bytecode or the host to manage exceptional control flow.

### Table-Driven Unwinding: The JVM and .NET Model

The most widely deployed exception model is table-driven unwinding. Each method carries an exception table — a list of bytecode ranges and their corresponding handlers. When an exception is thrown (via the `athrow` instruction in the JVM or `throw` in .NET CIL), the runtime walks the call stack, checking each frame's exception table to find a handler for the exception's type.

In the **JVM**, the exception table lives in the `Code` attribute of a method. Each entry specifies:
- `start_pc`, `end_pc`: bytecode range where the exception applies
- `handler_pc`: bytecode address of the handler
- `catch_type`: constant pool index for the exception type

When an exception is thrown, the JVM searches the exception table of the currently executing method. If the current PC falls within a range and the thrown exception matches (or is a subclass of) the `catch_type`, execution transfers to `handler_pc`. The JVM clears the operand stack and pushes the exception object, as required by the JVM spec (section 4.10.1, "Structured Operations and Opcode Constraints"). If no handler matches in the current method, the exception propagates to the caller, repeating the search.

**.NET CIL** implements the same idea through the exception handling table in the method's metadata. The table specifies try blocks, catch handlers, and finally blocks. The runtime unwinds the call stack, consulting the exception table at each frame. A key difference: .NET supports *finally* blocks, which are guaranteed to execute during unwinding (the CLR will execute any finally code before leaving a try block, even if an exception is thrown). This is more complex than the JVM's exception tables, which have no special finally semantics — finally blocks in Java are compiled into bytecode duplicated after both the normal return path and the exception path.

The critical advantage of table-driven unwinding: it is a **zero-cost abstraction on the non-throwing path**. No instructions are executed, no state is maintained, and no registers are clobbered during normal execution. The exception table is only consulted if an exception actually occurs. For code paths that rarely throw (most user code), this is the ideal design.

The limitation: exception tables require the bytecode format to define them, and the loader must validate them. If a handler_pc is out of range or the exception type is invalid, the loader catches this during class/module loading, not during execution.

### Explicit Save-and-Restore: Lua and Early Interpreters

**Lua's `pcall` (protected call)** implements exception handling via explicit save-and-restore. Before calling a protected function, the runtime saves the call stack pointer and exception handler. If an exception occurs inside the pcall, a `longjmp` returns to the saved point, restoring all state. If the function completes normally, the handler is popped.

This approach is simpler to implement than table-driven unwinding — no exception tables, no loader validation — but it has a cost: every protected call must save state (allocate a handler frame, push a `setjmp` context). The instruction footprint is:

```
enter_pcall label              -- save handler context, jumps to label on error
function_call ...
leave_pcall                    -- pop handler context
...
label:
handle_exception ...           -- error path
```

The cost is paid on every entry to the protected region, not just on throws. For Lua, where most functions are not protected, this is acceptable. For a VM where exceptions are expected to be common (the JVM), it would be unacceptable — the per-call overhead would be substantial.

Lua's mechanism is also less flexible than table-driven unwinding: the handler is a linear execution path (the code at `label`), not a jump table. Multiple catch types (catch `IOException`, catch `RuntimeException`) require explicit type checking in bytecode, not a metadata-driven dispatch. This is why Lua has no built-in exception type system — `pcall` returns `(success, value_or_error)`, leaving error handling to the caller.

### Two-Phase Unwinding: DWARF and the Itanium ABI

The C++ standard library's exception handling, as specified by the Itanium C++ ABI, uses a more sophisticated scheme: **two-phase unwinding**. Phase 1 walks the call stack *without* unwinding, looking for a handler. Phase 2 unwinds and invokes destructors for each exiting scope. This design allows the runtime to detect if an exception will be caught before unwinding anything — important for correctness if a destructor throws an exception.

The mechanism uses **DWARF** (Debugging With Attributed Record Formats), which encodes information about stack frames, register locations, and exception handling state in a compact language-independent format. When an exception is thrown, the unwinder reads DWARF tables to determine:
1. Which frame is the landing pad (handler)?
2. How to restore the caller's registers from the current frame?
3. Which destructors must be called as the frame exits?

The runtime then rewinds the stack, executing destructors in the correct order, and finally transfers control to the handler.

**LLVM's gc.statepoint infrastructure** (used by Java AOT compilers and some research VMs) applies similar two-phase logic: the first phase determines if a GC-safe exception handler exists; the second phase unwinds, running the GC from each frame if needed, and then transfers to the handler.

This is more complex than simple table-driven unwinding, but necessary in C++ to guarantee destructor semantics and correct exception safety. It is overkill for languages like Java that have no destructors.

### WebAssembly's Exception Handling Proposal

The WebAssembly exception handling proposal (currently standardized, but not part of MVP) integrates exceptions as first-class control flow constructs, consistent with Wasm's structured control model. Rather than arbitrary jumps to exception handlers, exceptions use `try`/`catch`/`end` blocks that pair syntactically with the code they protect.

```wasm
try $label
  call $risky_function  ;; may throw
  ...
catch $exn             ;; catches exception
  local.get $exn       ;; push exception on stack
  ... handle it ...
end
```

The exception is represented as a tag (a module-level constant) and a payload (values carried by the exception). Like `block` and `loop`, a `try` block can be exited via `throw` or `rethrow` instructions, and the structure ensures that exception handlers are always reachable from the protected code.

The advantage: exception handling is part of the type system. The validator ensures that a `throw` instruction exits the correct number of blocks and that handlers are only reached via explicit exception paths. This prevents "jumping" to a handler from an unprotected context. The disadvantage: Wasm's post-MVP exception proposal required adding new instructions and type rules, increasing the spec and implementation complexity.

### BEAM's Let-It-Crash Model

**Erlang and the BEAM VM** use a fundamentally different exception model: exceptions exist, but the primary recovery mechanism is *process-level* supervision, not exception handling within a process. When a process crashes (an unhandled exception kills the process), a supervisor process (linked to the crashing process) detects the failure and decides whether to restart it, escalate it, or handle it differently. Individual try/catch blocks exist in BEAM bytecode, but they are subordinate to the process supervision hierarchy.

This reflects Erlang's core design principle: "let it crash." Rather than writing defensive code with lots of exception handlers, Erlang encourages programmers to fail fast and let the supervisor handle recovery. The VM supports try/catch via the catch instruction and EXC_HANDLER entries in the code — similar to the JVM model — but the exception is secondary to the process level.

The practical consequence: exceptions within a process follow the JVM-style table-driven model, but *crashing* a process is an entirely different event, handled by linking/monitoring between processes. A monitor can observe crashes asynchronously and take action without being on the call stack.

### The Cost-Benefit Trade-Off

| Method | Throwing Cost | Non-Throwing Cost | Complexity | Supports Destructors? |
|--------|---------------|-------------------|------------|----------------------|
| Table-driven (JVM, .NET) | Stack walk + table lookup | Zero | Moderate | No (Java); Yes (.NET, C++) |
| Explicit save/restore (Lua) | longjmp | Per-call handler setup | Simple | No |
| Two-phase (DWARF/C++) | Full unwinding + destructor calls | Zero | High | Yes |
| Structured (Wasm) | Defined by control flow | Zero | High | N/A |
| Process-level (BEAM) | Process death; restart elsewhere | Zero | Language-dependent | N/A |

The consensus for general-purpose VMs (JVM, .NET): table-driven unwinding is the right choice. The non-throwing path has zero overhead, the throwing path is efficient (a stack walk with a table lookup per frame), and the mechanism is verifiable (the loader validates exception tables). 

For embedded languages (Lua) that expect few exceptions, explicit save-and-restore is acceptable. For systems languages (C++) that must run destructors during unwinding, two-phase unwinding is necessary. Wasm's structured approach guarantees safety but requires careful design to avoid forcing exception handling into the control flow type system. BEAM's process-level model is unique to Erlang's architecture and cannot be adopted directly by other VMs.

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

## Cross-Cutting Topics: Tail Calls, Coroutines, and Dynamic Dispatch

This section covers three design questions that cut across multiple bytecode systems and shape how they handle unbounded recursion, suspension and resumption, and method resolution at the bytecode level.

### Tail Calls at the Bytecode Level

A tail call is a function call where the caller does not need to return control to its caller after the callee finishes — the callee can instead return directly to the caller's caller, reusing the stack frame rather than pushing a new one. This is essential for languages like Scheme (where the language spec requires unbounded recursion to be expressible as iteration) and useful for techniques like CPS transformation and trampolined interpreters where the stack would otherwise grow linearly with loop depth.

**Wasm's tail calls** are standardized via the `return_call` and `return_call_indirect` instructions, which replace the current frame with a call to the target function rather than pushing a new frame. [The proposal has progressed through the WebAssembly standardization process and reached Phase 3 implementation](https://github.com/WebAssembly/tail-call), with support in modern runtimes like V8. This enables Wasm modules compiled from Scheme and other languages requiring tail call optimization to run with constant stack depth.

**The JVM's failure to standardize tail calls** is instructive. Tail-call support has been proposed multiple times, with [a draft specification defining a `tailcall` prefix opcode](https://cr.openjdk.org/~jrose/draft/vm-tailcall-jep.html) that could precede any invoke bytecode. The JSR 292 Expert Group concluded that tail calls could be implemented securely and performantly, with formal proofs that JVM-style access checks are compatible with tail call elimination. Yet the feature never shipped. The real obstacle was not technical but architectural: stack frames carry security context — method access checks, caller-sensitive operations, and exception handler tables all depend on the frame chain. The JVM team perceived tail call elimination as breaking the security model, even though the research contradicted this. Instead, Kotlin and Scala emulate tail calls via whole-method trampolines or loops, accepting code size penalties to avoid the language change.

**.NET CIL's `tail.` prefix**, [defined in ECMA-335](https://ecma-international.org/publications-and-standards/standards/ecma-335/), offers a middle ground: a hint instruction that can precede call operations. The runtime is allowed (but not required) to implement the tail call optimization. This creates a portability issue — code written assuming tail call semantics may not run correctly on a conforming but lazy implementation. The safer languages (C#, F#) have compiler-enforced conventions to avoid relying on the optimization.

The design tension is clear: Wasm designed for it from the start with no legacy baggage. The JVM designed against it (for security) and cannot retrofit it without compatibility trauma. CIL compromised with a hint, sacrificing guaranteed semantics for flexibility.

### Coroutines, Generators, and Stack Switching

How should a bytecode VM support suspending execution, saving state, and resuming later? The design space ranges from language-level bytecode support to pure library implementations that cooperate with the runtime.

**Python's generator model** is the simplest: the `YIELD_VALUE` opcode saves the entire execution frame to the heap (frame objects are allocated on the heap in CPython, not the native stack, allowing them to outlive function calls). When `yield` executes, it returns a value to the caller and marks the frame as suspended. On the next `next()` call, the frame is restored from the heap and execution resumes from the saved program counter. This is cheap and transparent — the VM does minimal work, and the bytecode overhead is a single opcode. [CPython's documentation details how frame objects include both the variable-size local array and operand stack](https://github.com/python/cpython/blob/main/InternalDocs/interpreter.md).

**Lua's coroutine model** is more sophisticated: [each coroutine has its own separate Lua stack](https://www.lua.org/pil/9.1.html), completely independent from the main thread stack. The `coroutine.resume()` function switches stacks, and `coroutine.yield()` suspends the current coroutine, saving its position within the Lua stack. The implementation allocates a separate `lua_State` structure for each coroutine, with its own execution stack pointer. Lua coroutines are symmetric (any coroutine can suspend) but not preemptive — they are not lightweight threads, just a way to structure a single-threaded program.

**The Erlang/OTP approach** is different again: each process (Erlang's term for lightweight actor) has its own heap and stack, collected independently with per-process garbage collection. Process switching is preemptive and scheduled by the BEAM VM — one process yielding does not require explicit `yield` calls in the code. This is more powerful than Lua's symmetric coroutines (true concurrency) but requires per-process memory management.

**Wasm's stack switching proposal**, [currently in active development, aims to add typed continuations to Wasm](https://github.com/WebAssembly/stack-switching) — separate stacks per continuation, with explicit suspension/resumption points. Unlike Python or Lua, this is designed to support advanced control flow like async/await, generators, and lightweight threads within the same Wasm instance. The proposal is not yet standardized but is being prototyped in Wasmtime and other implementations.

The design principle: bytecode-level support (Python's YIELD_VALUE) is invasive but free from a runtime overhead perspective. Library-level support (Lua, Erlang) requires more infrastructure but is more flexible — a single language VM can express many concurrency models.

### Calling Conventions and Dynamic Dispatch

How do arguments reach a called function, and how does the VM decide which function to call? This seemingly mechanical question determines the bottleneck in code that makes many polymorphic calls.

**Stack-based argument passing** (JVM, CPython, CIL) pushes arguments onto the operand stack before the call instruction and the callee pops them. **Register-based passing** (Dalvik, Lua) places arguments in designated registers. **Frame-managed passing** (Dis) stores arguments in memory offsets relative to the frame pointer. Each has trade-offs for code density and dispatch overhead — register passing avoids stack churn but requires more explicit bookkeeping.

**Polymorphic inline caches (PICs)**, pioneered in the Self language, are the defining optimization technique for dynamic dispatch. At each call site, the VM maintains a cache mapping the receiver object's type (or shape/hidden class) to the target method. On a cache hit, the call is nearly free — just a memory load and a jump. On a miss, the VM performs a full method lookup and updates the cache. [Self's original 1991 paper by Hölzle, Chambers, and Ungar](https://bibliography.selflanguage.org/pics.html) showed that even with polymorphic message sends, a median speedup of 11% was achievable with per-call-site type caching. This technique became foundational in every high-performance dynamic language engine: V8, BEAM, Lua's JIT (via trace specialization), and the JVM's JIT compilers all implement variants.

**The JVM's `invokedynamic` instruction** ([introduced in JSR 292](https://wiki.openjdk.org/display/HotSpot/Method+handles+and+invokedynamic), shipped in Java 7) takes a different approach. Rather than the VM deciding what to call, `invokedynamic` delegates to user-provided code — a bootstrap method. On the first execution of an `invokedynamic` instruction, the VM invokes the bootstrap method, passing the static call site metadata (method name, expected signature). The bootstrap method returns a `CallSite` object containing a `MethodHandle` — the actual target method. The VM caches this result and subsequent invocations bypass the bootstrap entirely. This was revolutionary because it let dynamic language implementors (JRuby, Nashorn, Jython) write their own dispatch logic without modifying the JVM. It also enabled Java's lambda expressions (`(x) -> x + 1` compiles to an `invokedynamic` call) and `switch` on `String`. JSR 292 is arguably the most consequential JVM addition since Java 1.0 — it shifted the VM from "static multi-dispatch engine" to "programmable dispatch substrate."

**Wasm's typed function tables** offer a lower-level approach. Functions are stored in a table indexed by integer. A `call_indirect` instruction specifies the index and the expected function signature. At runtime, the VM fetches the function at that index and traps if its signature does not match the expected type. Type checking is a single pointer comparison (if signatures are interned) or a structural equality check. This is more expensive than a direct call but prevents the sandbox-breaking issue of calling a function with the wrong signature.

The core tension: static dispatch (direct calls) is fastest but inflexible. PIC-based dynamic dispatch adds minimal cost for monomorphic call sites (one receiver type) but scales gracefully with polymorphism. User-controlled dispatch (invokedynamic) sacrifices some automatic optimizations but puts language implementors in control. Wasm's tables trade dispatch speed for safety.

## Other Notable Bytecode Systems

The thematic comparisons above focus on eleven systems chosen for the breadth of design space they cover, but several more bytecode VMs are widely deployed enough to warrant individual treatment, and two historical systems deserve mention as predecessors.

### CPython

CPython is the reference implementation of Python, and its bytecode is the bytecode most programmers encounter without realizing it — every `.pyc` file in `__pycache__/` is CPython bytecode. It is a stack machine with no type information in the instruction stream, no verifier, and no sandbox. Loading bytecode from an untrusted source can crash or compromise the interpreter.

Since Python 3.6, instructions use a fixed-width 2-byte "wordcode" format: 1 byte for the opcode and 1 byte for an argument. Earlier versions used a variable-length encoding where some instructions had no argument and others had a 2-byte argument; the switch to fixed-width simplified the dispatch loop in `ceval.c` and made `pc += 2` trivially branch-predictable. Arguments larger than 255 are encoded by chaining `EXTENDED_ARG` prefix instructions, each contributing 8 more bits to the next instruction's argument. The interpreter uses computed goto dispatch where supported, falling back to `switch` on MSVC.

The most interesting recent change is PEP 659's specializing adaptive interpreter, shipped in Python 3.11. Hot instructions are *quickened* — replaced in place with specialized variants that assume specific operand types. `BINARY_ADD` becomes `BINARY_ADD_INT` after observing two integer operands; if a later execution sees a non-integer, the instruction *despecializes* back to the generic form. Inline caches storing specialization data are embedded *in the bytecode stream itself* as `CACHE` pseudo-instructions, keeping the hot data in the same cache lines as the executing opcode rather than in a parallel metadata array. The result is a 25–50% speedup over Python 3.10 without a JIT compiler — proof that traditional interpreter techniques still have headroom when applied carefully. CPython 3.13 added an experimental JIT (copy-and-patch style) on top of this, but the adaptive interpreter remains the default execution tier.

CPython's opcode count grows with each release as specialization variants accumulate; recent versions define over 180 distinct opcodes, the majority of which are specialized forms of a much smaller "user-visible" set documented in the `dis` module.

### V8 Ignition

V8 — Chrome's and Node.js's JavaScript engine — interprets bytecode through Ignition before any JIT tier kicks in. Ignition was introduced in 2016, replacing Full-codegen (a non-optimizing baseline JIT) with a true bytecode interpreter. The motivation was memory: storing bytecode for cold functions costs far less than storing baseline-compiled native code, and Ignition's bytecode is the canonical representation that all higher tiers (Sparkplug, Maglev, TurboFan) consume.

Ignition is a register machine with an *implicit accumulator*. Most instructions read or write the accumulator implicitly, eliminating a register operand from the encoding. `Add r2` adds register `r2` to the accumulator and stores the result in the accumulator — a single explicit register operand instead of three. This trick yields code denser than a pure register machine without paying the dispatch cost of a stack machine.

The implementation technique is what makes Ignition unusual. Bytecode handlers are not written in C++; they are written in CodeStubAssembler (CSA), a portable macro-assembler that TurboFan compiles to native code at V8 build time. A single handler specification produces optimized native dispatch routines for every architecture V8 targets (x86-64, ARM64, ARM, MIPS, RISC-V) — combining the portability of writing-once with native-speed handlers. Bytecode flows from Ignition into TurboFan's graph builder directly, so hot functions never need to be re-parsed from JavaScript source when promoted to the optimizing tier.

V8's full execution stack today is four tiers: Ignition (interpreter) → Sparkplug (non-optimizing baseline JIT, added 2021) → Maglev (mid-tier optimizing JIT, added 2023) → TurboFan (top-tier optimizing JIT). Ignition remains the foundation — every function starts there, and the bytecode is the durable representation across tiers.

### eBPF

eBPF (extended Berkeley Packet Filter) is the in-kernel sandbox VM that runs untrusted user-supplied bytecode inside the Linux kernel — and increasingly Windows, via [eBPF for Windows](https://github.com/microsoft/ebpf-for-windows). Introduced in Linux 3.18 (2014) as a generalization of the original BPF packet filter, it now powers XDP packet processing, kprobes and tracepoints for system observability, cgroup-attached network policy, and a growing list of kernel security hooks. The architecture is RISC-like: eleven 64-bit general-purpose registers, a 512-byte stack, fixed-width 64-bit instructions, and a calling convention deliberately aligned with x86-64 so the in-kernel JIT can map eBPF registers directly to native registers.

What sets eBPF apart from every other VM in this survey is its *verifier*. Before any program is allowed to run, the kernel statically analyzes it using abstract interpretation: register types, value ranges (tracked as both intervals and bit-level *tnums*), pointer provenance, and stack contents are propagated along every reachable path. The verifier proves that all memory accesses are in-bounds and type-correct, that control flow terminates (originally by forbidding loops entirely; since Linux 5.3, by accepting bounded loops whose induction variables it can prove monotonic), and that the program does not exceed configured complexity limits (currently up to one million verified instructions). Programs are forbidden from invoking arbitrary syscalls; instead they call a whitelisted set of *helper functions* and access a small set of typed map structures shared with user space. Per-architecture in-kernel JITs (x86-64, ARM64, PowerPC, s390x, MIPS64) translate the verified bytecode to native code, with built-in mitigations for Spectre (Retpolines) and JIT spraying (constant blinding).

This is a categorically different verification posture from the JVM or Wasm. Those systems verify *type safety* — they prove that the program will not corrupt the VM's invariants. eBPF additionally proves *termination and complete memory safety* of arbitrary loaded code, which is what makes it acceptable to load into ring 0 in the first place. The cost is a much smaller language: no recursion, bounded program length, restricted helper API, and a verifier rejection rate that programmers learn to design around. The benefit is the only widely-deployed example of a programmable in-kernel VM.

Sources: [A thorough introduction to eBPF (LWN)](https://lwn.net/Articles/740157/), [the eBPF verifier docs](https://docs.ebpf.io/linux/concepts/verifier/), [BPF Design Q&A in the kernel tree](https://docs.kernel.org/bpf/bpf_design_QA.html).

### OCaml Bytecode

OCaml is the most significant production example of a statically-typed functional language with both a bytecode interpreter and a native code compiler. The bytecode VM, ocamlrun, executes programs compiled by ocamlc; the native compiler ocamlopt produces standalone binaries. Both backends compile from the same *lambda* intermediate representation, so a program's behavior is identical between modes — developers trade compilation speed (bytecode) for runtime performance (native) without changing source code. The bytecode VM descends from Xavier Leroy's 1990 ZINC abstract machine, which adapted Krivine's machine for a strict, ML-style language.

The defining representational choice is *uniform tagged values*. Every OCaml value occupies exactly one machine word. The low bit acts as a type tag: if it is 1, the upper 63 bits are an integer; if it is 0, the word is a pointer to a heap-allocated block. The GC walks pointers by inspecting only the words whose tag bit is clear, eliminating the need for separate stack maps or boxed integers. The cost is a one-bit reduction in integer range (63-bit ints on a 64-bit system) and the overhead of tag/untag operations on arithmetic. The instruction set is around 150 opcodes — small for a typed language — with specialized opcodes for closure creation, pattern matching, and tagged arithmetic that exploit the runtime representation directly.

The bytecode interpreter uses threaded code (computed goto on GCC, switch dispatch elsewhere) and is unusually compact: ocamlrun is a few thousand lines of C, comparable to Lua's interpreter. OCaml's continued use of bytecode in production — most OPAM packages distribute as bytecode, and ocamldebug requires it — demonstrates that a typed FP runtime can ship a serious bytecode tier without sacrificing language semantics. Sources: [Real World OCaml — The Compiler Backend](https://dev.realworldocaml.org/compiler-backend.html), [Leroy's ZINC paper](https://xavierleroy.org/talks/zam-kazam05.pdf).

### Ruby YARV

YARV (Yet Another Ruby VM) is the bytecode interpreter merged into CRuby for the 1.9 release in 2007, replacing the AST-walking interpreter that preceded it and delivering an immediate severalfold speedup on most workloads. It is a stack machine of around 100 opcodes, with the usual mix of arithmetic, local variable access, method calls, and Ruby-specific operations (block invocation, instance variable access, dynamic method dispatch). The dispatch loop uses computed goto where available. YARV's instruction sequences are deeply tied to Ruby semantics — every `send` opcode performs full polymorphic method lookup, hash literal opcodes carry frozen-string flags, and the bytecode preserves enough information to print accurate line-level backtraces.

Ruby's JIT story is unusual. The first attempt, MJIT (Ruby 2.6, 2018), generated *C source code* from hot methods, invoked the system C compiler in a background thread, and dlopen'd the resulting `.so` file. This was easy to implement and produced reasonable code but suffered from slow warmup (multi-second compilation latency) and modest steady-state gains. Shopify's [YJIT](https://github.com/Shopify/yjit), now part of upstream Ruby since 3.1 (2021), took a radically different approach: *Lazy Basic Block Versioning* (LBBV), a technique developed in [Maxime Chevalier-Boisvert's PhD thesis](https://dl.acm.org/doi/10.1145/3486606.3486781). Rather than compiling whole methods, YJIT compiles individual basic blocks lazily, generating a fresh version of each block per observed type combination. This sidesteps the need for static type inference: types are discovered by runtime, and the JIT specializes blocks accordingly, falling back to the interpreter when an unexpected type arrives. Warmup is near-instant, memory overhead is small, and production Rails workloads see 15–30% throughput gains. Ruby 3.4 introduces ZJIT, which layers an SSA IR over YARV bytecode for broader optimizations than LBBV alone supports. Sources: [Shopify Engineering on YJIT](https://shopify.engineering/ruby-yjit-is-production-ready), [Ruby's JIT journey (Codemancers)](https://www.codemancers.com/blog/rubys-jit-journey).

### PHP: Zend Engine and HHVM

The Zend Engine has been PHP's runtime since version 4 (2000). It compiles PHP source to a stack-based bytecode of roughly 200 opcodes and dispatches them through a handler table. For more than a decade PHP had no JIT — a deliberate decision rooted in PHP's request-per-process model (every request started cold, so steady-state JIT gains never materialized) and the difficulty of type-specializing dynamically typed code. The intermediate fix was *opcache*, shipped in PHP 5.5 (2013): compiled bytecode is cached in shared memory across worker processes, eliminating the per-request parse-and-compile cost. Opcache alone roughly doubled real-world throughput.

PHP 8.0 (2020) finally added a JIT, built on [DynASM](https://luajit.org/dynasm.html) — the same dynamic assembler library LuaJIT uses. It ships in two modes: a function JIT that compiles whole functions on first call, and a tracing JIT (the default) that profiles bytecode execution and emits type-specialized native code for hot traces. The win is largest on compute-bound code; for the typical web workload dominated by I/O and database calls, gains are modest, validating the original "no JIT needed" position even as the option becomes available. The [PHP RFC by Stogov and Suraski](https://wiki.php.net/rfc/jit) documents the engineering rationale.

Facebook's [HHVM](https://github.com/facebook/hhvm) explored a parallel design space. After the failed HPHPc experiment (2010–2013, a static PHP-to-C++ AOT compiler that struggled with PHP's dynamism), Facebook pivoted to a JIT-based bytecode VM. The original HHVM JIT compiled *tracelets*: maximal type-specialized linear bytecode sequences extracted by inspecting live VM state at the point of compilation. Tracelets enabled aggressive type assumptions but limited inter-block optimization. Over 2014–2015 Facebook generalized to *region-based compilation*, expanding the unit of compilation from straight-line traces to arbitrary control-flow regions including loop bodies. This unlocked a cumulative ~15% CPU reduction across Facebook's web fleet. HHVM's PHP support was eventually dropped in favor of Hack, but its trajectory — AOT → tracelet JIT → region JIT — is one of the most thoroughly documented evolutions of a production bytecode runtime. Sources: [Redesigning the HHVM JIT (Facebook Engineering)](https://engineering.fb.com/2016/09/22/networking-traffic/redesigning-the-hhvm-jit-compiler-for-better-performance/), [HHVM region JIT blog post](https://hhvm.com/blog/2017/02/17/region-jit.html).

### Honorable Mention: Forth and Threaded Code

Forth, designed by Charles H. Moore in 1968 and described in his [1970 paper](https://www.ultratechnology.com/4th_1970.pdf), predates most of the systems in this survey by a decade and pioneered the dispatch technique that bytecode VMs eventually rediscovered. A Forth program *is* a sequence of addresses — a "thread" — pointing at executable routines. There is no separate interpreter loop; each routine ends by fetching the next address from the thread and jumping to it. This *threaded code* model comes in four variants — direct (raw machine addresses, fastest, machine-specific), indirect (an extra dereference, used by Moore's Nova implementation, enables shared definitions), subroutine (a sequence of native `CALL` instructions, lets the CPU's return stack carry the thread), and token (table indices, fully portable but adds a lookup per word). Empirical comparisons across 2000s-era CPUs show no universal winner; the right variant depends on the host's branch predictor and indirect-jump latency.

Threaded code's influence is broader than Forth itself. Smalltalk-80's bytecode dispatch, PostScript's stack-based execution model, and Open Firmware's portable FCode all descend from it. Modern computed-goto interpreters (CPython 3.11+, Lua, Q3VM) are the same idea reformulated in C: jump directly from one handler to the next without returning through a centralized loop. The terminology has drifted — what bytecode VMs call "direct-threaded dispatch" is what Moore called the entire execution model — but the core insight, that eliminating the dispatch loop is the largest single interpreter optimization available without leaving portable code, is unchanged. Sources: [Threaded Code (Ertl, TU Wien)](https://www.complang.tuwien.ac.at/forth/threaded-code.html), [Threaded Code (Wikipedia)](https://en.wikipedia.org/wiki/Threaded_code).

### Honorable Mention: What Bytecode Replaced — GW-BASIC and Tokenization

Worth noting for context: microcomputer BASIC interpreters of the late 1970s and early 1980s — Microsoft BASIC (1975), GW-BASIC (1983), and their many derivatives — are sometimes lumped in with bytecode VMs, but they are not. They used *tokenization*: source keywords (`PRINT`, `FOR`, `IF`) were replaced at line-entry time with single-byte tokens (`0x91`, `0x82`, `0x8B`) to save memory in 4 KB ROMs. Variable names stayed as ASCII strings, line numbers stayed as ASCII branch targets, and the interpreter parsed each line at runtime — looking up variables in a symbol table, scanning forward to match `FOR`/`NEXT`, searching the line index on every `GOTO`. There is no program counter indexing into a compiled instruction stream; there is a pointer walking partially-parsed source.

Bytecode VMs are precisely what replaced this model. The defining move is doing operand resolution *once*, ahead of execution: variable references become slot indices, branches become offsets or ordinals, and the runtime executes a pre-compiled instruction stream rather than re-parsing source on every iteration. Smalltalk-80 (1980) is the cleaner ancestor of the modern bytecode VM; BASIC tokenization is interesting only as the popular technique that bytecode rendered obsolete.

### Honorable Mention: Wirth's P-machine (P2/P4)

UCSD P-code, covered earlier, is the most famous Pascal bytecode — but it descended from earlier work at ETH Zürich. Niklaus Wirth's original P-machine, described in his 1976 book *Algorithms + Data Structures = Programs*, is a stack machine of striking minimalism: just **eight instructions** (`lit`, `opr`, `lod`, `sto`, `cal`, `int`, `jmp`, `jpc`). The `opr` instruction is parameterized — its argument selects among arithmetic, comparison, and runtime operations — so the eight-instruction count understates the actual semantic surface, but the dispatch loop is tiny.

The Pascal-P series of compilers (P1 in 1973, P2 around 1974, P4 as a successor) were portable bootstrap kits: a Pascal compiler written in Pascal that emitted P-code, distributed with a P-code interpreter in a few thousand lines of Pascal. Anyone implementing a P-code interpreter on their hardware got a working Pascal compiler nearly for free. Kenneth Bowles's UCSD team adapted the P2 compiler around 1974–1975, switched it to a byte-oriented encoding, and built the UCSD P-System around it — the version that shipped on the Apple II, IBM PC, and dozens of other machines.

The P-machine's design influence is everywhere: the JVM's frame-and-operand-stack structure, Smalltalk's bytecode dispatch, and the general assumption that "portable compiled language = stack VM + interpreter in C" all trace back to Wirth's eight-instruction sketch. The lesson — that a tiny instruction set can carry a high-level language, if you accept dispatch overhead — remains the foundational bet of every interpreter in this survey.

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
