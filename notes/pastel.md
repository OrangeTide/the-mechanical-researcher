# Pastel

**Designer:** Jeffrey M. Broughton
**Year:** 1982
**Origin:** S-1 supercomputer project, Lawrence Livermore National Laboratory (LLNL)
**Target:** Amber operating system (designed by MIT AI Lab) for the S-1 Mark IIA processor
**Name:** "An off-color Pascal" — a pun reflecting that it extends Pascal but isn't quite Pascal

## Context

The S-1 project (1975–1988) at LLNL built custom supercomputers. The Mark IIA was the
second-generation machine, using ECL-100k chips at 15 MIPS with vector instructions and a
hardware FFT unit. The Mark I used a 36-bit word length with 9-bit bytes based on the PDP-10
instruction set.

The Amber operating system was being implemented in PL/1, but dissatisfaction with PL/1 led
Broughton (project engineer for compilers and OS software) to design Pastel as a replacement
systems programming language. Pascal was already in use on the project — the SCALD
(Structured Computer-Aided Logic Design) CAD system was written in Pascal specifically for
portability across machines like the IBM 370.

## Language Features

Pastel extended standard Pascal with systems-programming capabilities:

| Feature | Description |
|---------|-------------|
| Parametric types | Generic/parameterized type definitions |
| Explicit packing | Control over memory layout and allocation |
| Multiple parameter modes | Beyond Pascal's value/var — additional passing conventions |
| Module definition | Separate compilation units |
| Exception handling | Structured error handling |
| Set iteration | Iteration over set members |
| Loop-exit | Early exit from loops (like `break`) |
| Return statement | Explicit return from procedures/functions |
| Conditional booleans | Short-circuit boolean evaluation |
| Constant expressions | Compile-time expression evaluation |
| Variable initialization | Initial values in declarations |

## Significance: Inspiration for GCC

The Pastel compiler's most lasting impact was as the catalyst for GCC:

1. Richard Stallman's initial plan for the GNU C Compiler was to rewrite the LLNL Pastel
   compiler from Pastel to C, with help from Len Tower and others.

2. Stallman wrote a new C front end for the Livermore compiler, but the Pastel compiler
   required megabytes of stack space — impossible on a 68000 Unix system with only 64 KB.

3. None of the Pastel compiler code ended up in GCC. Stallman started GCC from scratch,
   though he did retain the C front end he had written during the Pastel work.

The connection is inspirational rather than genealogical: Pastel showed Stallman what a
compiler for a systems language should look like, but GCC shares no code with it.

## Language Family Position

Pastel sits in the "extended Pascal for systems programming" niche alongside:
- **Mesa** (Xerox PARC, 1976) — modules, processes, exception handling
- **Modula-2** (Wirth, 1978) — modules, coroutines, low-level access
- **Ada** (DoD, 1980) — generics, exceptions, tasking, packages

Unlike Mesa and Modula-2 which became widely used, Pastel remained tied to the S-1 project.
When the S-1 Mark IIA proved unreliable (weekly failures from wire-wrap connections) and the
project ended in 1988, Pastel had no other user base to sustain it.

## References

- J. Broughton, "THE S-1 PROJECT: Advancing the Digital Computing Technology Base,"
  chapter: S-1 Software Development: Programming Languages Supported.
- C. Frankston, "The Amber Operating System," MIT thesis, 1984.
- M. Smotherman, "S-1 Supercomputer (1975-1988)," archived 2014-01-11.
- Wikipedia: Pastel (programming language), GNU Compiler Collection (history section).
