# Component Pascal

**Designers:** Oberon microsystems (ETH Zürich spin-off), building on N. Wirth and H. Mössenböck's Oberon-2
**Released:** 1997 (originally named Oberon/L; the Oberon/F IDE shipped in 1994)
**Target:** Native code x86 (BlackBox) and x64 (Herschel), .NET CLR and JVM (Gardens Point)
**Paradigm:** Imperative, structured, object-oriented, component-oriented
**Website:** https://blackboxframework.org/
**Language report:** https://blackbox.oberon.org/cp-lang.pdf
**Change list:** C. Pfister, "What's New in Component Pascal?", March 2001
**Wikipedia:** https://en.wikipedia.org/wiki/Component_Pascal

## Core Concept

Component Pascal is a superset of Oberon-2 except for some minor points, aimed squarely
at building **safe, separately deployable software components**. The design bet is that a
very small language plus strictly enforced module boundaries is enough for
component-based development, with no separate interface definition language, no
hand-written header files, and no manual memory management. Interfaces are extracted from
the module source by the compiler into symbol files, never maintained by hand.

Pfister states the motivation directly: the revision was driven by experience with the
BlackBox Component Framework, and the goal was to give a framework architect the means to
control the integrity of a large component-based system. The additions are graded by
audience. They are most visible to framework designers, less visible to framework
extenders, and least visible to ordinary clients, so each group carries only the
complexity its job requires. A pure implementation language would not need the new
attributes at all. A component-oriented *design* language does.

The whole language needs only 34 grammatical productions, one more than Oberon-2. Nearly
everything Component Pascal adds is a *restriction* the compiler can check across module
boundaries, not new syntax.

## What It Adds Over Oberon-2

### Record Modifiers

A record type may be attributed to state architectural decisions explicitly, so the
compiler can check that implementations and clients comply. The four combinations:

| Modifier | Extension | Allocation | Record assignment |
|----------|-----------|------------|-------------------|
| none ("final") | no | yes | yes |
| `EXTENSIBLE` | yes | yes | no |
| `ABSTRACT` | yes | no | no |
| `LIMITED` | no\* | no\* | no |

\* except in the defining module

Records are final by default, which inverts the norm in object-oriented languages. A
final type lets its implementor analyze the type completely, and change it, without
breaking clients. Only `EXTENSIBLE` and `ABSTRACT` records can be extended at all.

Extensible records can neither be copied nor passed as value parameters, since a value
parameter implies a record assignment. Final types can still be extensions of other
records and can still have methods.

`ABSTRACT` types are the primary means of modelling a component interface. They denote
interfaces of objects, as distinct from implementation constructs. Types of *values* can
never be abstract, only types of variables, and only referential ones: pointers, or `VAR`,
`IN`, `OUT` parameters. Since `NEW` produces a value of the argument's type, `NEW` cannot
be applied to an abstract type. An abstract type is not forced to be fully abstract, and
may carry methods of every kind.

`LIMITED` types are a special case of final types where allocation and extension are
confined to the defining module and copying is forbidden outright. The payoff is
initialization control: the defining module can guarantee every instance a client can see
already satisfies the type's invariants. No lazy initialization schemes, no delayed
runtime errors from missing initialization, and invariants over hidden fields cannot be
broken by copying. Instances are handed out by factory procedures. Because clients cannot
substitute their own extensions, a `LIMITED` type also protects non-extensible services,
a real-time kernel being Pfister's example, from being called with illegal types.

```
RecordType = [EXTENSIBLE | ABSTRACT | LIMITED] RECORD ["(" QualIdent ")"] FieldList {";" FieldList} END.
```

### Type-Bound Procedure Attributes

Methods are attributed on the same principle, and are also final by default:

| Attribute | Meaning |
|-----------|---------|
| `NEW` | Required when introducing a method; forbidden on one that extends an inherited method. |
| `EXTENSIBLE` | May be redefined in extensions. Without it the method is final. |
| `ABSTRACT` | Signature only, no procedure body. Extensions must implement it. |
| `EMPTY` | Concrete and callable, but does nothing until an extension implements it. |

```
TBProc = PROCEDURE Receiver IdentDef [FormalPars] [Attribution].
Attribution = ["," NEW] ["," (EXTENSIBLE | ABSTRACT | EMPTY)].
```

Final methods may be bound to any record type. Extensible methods may only be bound to
`EXTENSIBLE` or `ABSTRACT` types. Abstract and empty methods are both special cases of
extensible methods, and neither may be called via a super call. Empty methods may not
return a function result and may not have `OUT` parameters. Since final methods cannot be
overridden, the invariants and postconditions they establish cannot be violated.

`EMPTY` is the interesting one: it represents an *optional* interface. BlackBox's `View`
declares `HandleCtrlMsg` empty, so interactive views implement it and passive views simply
ignore it, with no null checks and no default-method boilerplate.

`NEW` closes the accidental-override hole from both sides. The compiler catches a base
method that was renamed without renaming its extensions, and catches a method introduced
as new when that name already exists in a base type or a subtype. Pfister's stated
motivation is refactoring: these checks are what make it practical to change a framework's
interfaces and regain consistency afterwards. Java added `@Override` for the same problem
years later, and made it optional.

### Implement-Only Export

A method can be exported with the `-` mark instead of `*`, meaning it may be *implemented*
outside the defining module but not *called* from outside. The export mode is fixed when
the method is introduced, and later extensions must use the same mode.

This separates the two faces every framework has: the client interface and the
specialization interface. Implement-only methods are upcalls, invoked by the framework
into code the client supplied. Marking them prevents clients from calling them directly
and violating framework invariants, while still allowing new implementations. This is a
distinctly component-oriented feature with no equivalent in most languages, where
"protected" conflates the two directions.

### Parameter Modes

Four modes, where Oberon-2 had two. Plain value parameters are unchanged, and three
keywords mark the rest:

- `VAR` — input/output, actual argument must be a variable
- `IN` — read-only inside the procedure. Allowed only for record and array types.
- `OUT` — undefined on entry, except pointer and procedure variables which are set to
  `NIL`. `OUT` record parameters require identical actual and formal types.

`IN` and `OUT` are specializations of `VAR`, so constants cannot be passed to them, with
one exception: string constants may be passed to open-array `IN` parameters, since the
compiler already implements them as a kind of read-only variable.

`IN` gives pass-by-reference efficiency for large records and arrays without giving up
the guarantee that the callee does not modify the argument. Where Oberon-2 code used `VAR`
purely for efficiency, `IN` now says what is actually meant. Pfister also notes these
modes matter for the signatures of distributed objects, where the direction of data flow
determines what has to be marshalled.

### Type System Relaxations

- **Covariant pointer function results.** A type-bound function returning a pointer may be
  redefined to return an extended type. This is type safe, since it only strengthens the
  function's postcondition, and it makes interface declarations more precise.
- **Structural pointer compatibility.** Two pointer types with the same base type are now
  compatible, which is what makes signatures like
  `PROCEDURE P (p: POINTER TO ARRAY OF INTEGER)` usable.
- **Function results may be any `Type`, not just an `Ident`**, so
  `PROCEDURE Bla (): POINTER TO Rec` is legal.

### ANYREC and ANYPTR

Every base record is implicitly an extension of `ANYREC`, an abstract empty record that
roots all record hierarchies, even when declared with no explicit base type. `ANYPTR`
corresponds to `POINTER TO ANYREC`. These exist so independently developed frameworks can
interoperate through genuinely generic parameters.

`ANYPTR` carries one method, implement-only and empty:

```
PROCEDURE (a: ANYPTR) FINALIZE-, NEW, EMPTY;
```

Extending it registers a finalizer, called at an unspecified time after the object becomes
unreachable and before deallocation, used to release non-Component-Pascal resources such
as file handles or OS window pointers. Finalization order is undefined and each object is
finalized once. Note that the finalizer is implement-only by construction: you implement
it, the runtime calls it, and no client can invoke it.

### String Support

The `$` operator selects the string *value* held in a character array, as distinct from
the array variable itself. `OpenFile(pathname$)` passes the null-terminated contents
rather than all 2048 characters of the buffer. Strings are null-terminated (`0X`)
sequences in two flavors, Shortstring (Latin-1) and String (Unicode).

Consequences Pfister highlights:

- Fixed-size character array parameters become practical. In Oberon the actual parameter's
  type had to match, so open arrays were used by default. An open array is a contract to
  accept an array of *any* length, which is a weaker and less honest specification than
  `ARRAY 2048 CHAR`.
- `+` concatenates strings, with the target required to be long enough.
- The auxiliary procedure `COPY` is gone. `COPY(a, varpar)` becomes `varpar := a$`.

Assigning a string to a character array is length-checked at runtime rather than
truncating silently. The null terminator keeps C interoperability while `$` and the
runtime check keep the language safe.

## Features Declared Obsolete

Retained for backward compatibility, but discouraged, with support possibly reduced later:

- **Super calls**, because of the semantic fragile base class problem. Pfister recommends
  designing with composition instead of implementation inheritance so they are not needed.
- **Procedure types**, because they are less flexible than objects with methods, they are
  not extensible, and they pose considerable difficulty for safe unloading of code. They
  survive for callbacks and low-level interfacing.

The second reason is the component argument again: a procedure variable is a raw code
pointer, and a module cannot be safely unloaded while one of those is still held.

## Example

`Shape` is an abstract base with an abstract method, `Circle` is a final extension that
implements it, and the radius is exported read-only.

```
MODULE Shapes;

    TYPE
        Shape* = POINTER TO ABSTRACT RECORD END;

        Circle* = POINTER TO RECORD (Shape)
            r-: REAL
        END;

    PROCEDURE (s: Shape) Area* (): REAL, NEW, ABSTRACT;

    PROCEDURE (c: Circle) Area* (): REAL;
    BEGIN
        RETURN 3.14159 * c.r * c.r
    END Area;

    PROCEDURE NewCircle* (r: REAL): Circle;
        VAR c: Circle;
    BEGIN
        ASSERT(r > 0, 20);
        NEW(c); c.r := r;
        RETURN c
    END NewCircle;

END Shapes.
```

## Retained From Oberon-2

`LOOP`/`EXIT`, `WITH` with type guards, `MODULE` as the unit of compilation and
information hiding, the `-` read-only export marker on variables and fields, garbage
collection, and the predeclared procedures `ABS`, `CHR`, `LEN`, `MAX`, `MIN`, `ORD`,
`SIZE`, `ASSERT`, `DEC`, `HALT`, `INC`, `NEW`.

## What It Deliberately Lacks

Worth recording, because the omissions are as much of the design as the additions:

- **No exception handling.** There is no try, catch, throw or recovery construct. Failure
  is expressed with `ASSERT(cond, n)` and `HALT(n)`, plus implicit traps from failed type
  guards, `NIL` dereferences and range violations. Recovery is a runtime concern rather
  than a language one: BlackBox catches a trap, unwinds, reports it, and keeps the rest of
  the system alive. For a component system this is coherent, since a fault in a loaded
  component should not require every caller to have written handlers for it.
- **No operator overloading.** Operators are predefined for a fixed set of type
  combinations. Active Oberon and Zonnon both went the other way.
- **No generics.** Procedures are explicitly typed. Genuinely generic parameters go
  through `ANYPTR` and type guards.

### Types

Two character types, `SHORTCHAR` (Latin-1, `0X`–`0FFX`) and `CHAR` (Unicode,
`0X`–`0FFFFX`). Four integer types, `BYTE`, `SHORTINT`, `INTEGER` and `LONGINT` at 8, 16,
32 and 64 bits. Two real types, `SHORTREAL` and `REAL`. `SET` is fixed at elements 0..31,
which is the one place the language exposes a machine word width directly.

## Modules as the Component Mechanism

The module is not only the compilation unit, it is the deployment and loading unit. A
module body runs when the module is loaded, after all of its imports have been loaded, and
a `CLOSE` section runs when the module is removed. The language report defines the
sequencing but leaves the loading mechanism implementation-defined.

BlackBox supplies that mechanism: there is no linker. Modules are compiled to `.ocf` files
and brought into memory on demand by a runtime linking loader, in import order. The
compiler writes type descriptors and other metainformation directly into the `.ocf` file,
which is what makes the `Meta` module's reflection and the framework's dynamic command
dispatch work. Combined with `LIMITED` records, implement-only export and read-only export,
this is the whole of what "component" means here: separately compiled, separately loadable
modules whose interfaces the compiler checks and whose internals the loader cannot be
talked into exposing.

## Implementations

### BlackBox Component Builder

The original environment from Oberon microsystems: a native-code compiler, an IDE, a
component library, and a runtime, all in one self-hosted system. Notable properties:

- **Documents as source.** Source files use the `.odc` (Oberon document) format, a rich
  text binary format supporting formatting, conditional folding, and embedded active
  content in the source text. Compiled modules are `.ocf`, symbol files `.osf`. This is a
  real friction point for anyone expecting plain text under version control.
- **Forms-based GUI.** Interfaces are built as editable forms whose fields and command
  buttons are linked by name to exported variables and exported procedures. There is no
  glue code and no separate layout language.

Timeline: Oberon/F 1994, free download June 2004, open-source beta December 2004, v1.5
December 2005, v1.6 at the end of 2013 (the last release from Oberon microsystems).
Volunteers took over in 2014; v1.7 dates from September 2016 and v1.7.2 from November
2019. Windows-centric in practice, with community work on other platforms.

### Gardens Point Component Pascal (gpcp)

An open-source compiler from Queensland University of Technology, led by John Gough,
targeting both the .NET CLR and the JVM. It was one of the language implementations built
for Microsoft's "Project 7," and the first release was mid-2000 at the Professional
Developers Conference where .NET was announced.

Interoperability is handled by symbol-file importers: `PeToCps` reads .NET assemblies and
`J2CPS` reads Java class files, each generating gpcp symbol files so foreign libraries can
be called as if they were Component Pascal modules. `Browse` renders symbol files as
readable, hyperlinked text.

- https://github.com/k-john-gough/gpcp
- https://github.com/pahihu/gpcp-JVM (Java 8/11/17)

### Herschel

A Component Pascal to x64 compiler for BlackBox, emitting ELF on Linux and PE on Windows.
It exists because the original BlackBox compiler is 32-bit only. As of late 2021 it
covered most of the core language including `NEW`, the `IS` type test and `SYSTEM.TYP`
runtime type information. Small project, single primary author.

- https://herschel.oberon.org/

### CPFront

A Component Pascal to C transpiler by Oleg N. Cher, based on Josef Templ's OFront. Useful
where a C toolchain is the only available target.

- https://github.com/Oleg-N-Cher/CPFront
- https://github.com/jtempl/ofront

## Use in Education

BlackBox found a durable niche in teaching. It was presented at SIGCSE as a CS1/CS2
framework, and Project Informatika-21 in Russia promoted Component Pascal and BlackBox as
a first programming language, building on the strong existing Pascal tradition in Russian
schools. The selling points were a GUI simple enough for beginners, memory-safe pointers
with garbage collection, fast native compilation, and free use for education.

## Relevance to Compact Pascal

Component Pascal is the best worked example of "small language, strong module boundary" in
the Pascal family, and several of its decisions transfer directly:

1. **Final by default, for both types and methods.** Extensibility as an opt-in modifier
   costs one keyword and removes a whole class of fragile-base-class problems.
2. **`NEW` on method declarations.** A cheap compile-time check that catches both
   accidental overrides and renamed-base-method breakage. No runtime cost, no annotation
   processor. Pfister's framing is worth keeping: this is a refactoring aid first.
3. **`IN` parameters.** Read-only by-reference is the right default for large aggregates
   and is trivial to check in the front end.
4. **`LIMITED` types.** Compiler-enforced opaque handles with guaranteed initialization,
   at the cost of one keyword and a factory procedure.
5. **Implement-only export (`-` on a method).** Separating "you may implement this" from
   "you may call this" is the cleanest solution to the upcall problem seen in this family.
6. **Null-terminated strings with `$` and length-checked assignment.** Keeps C interop
   while removing silent truncation, and makes fixed-size array parameters honest.
7. **Restrictions over syntax.** Every addition is something the compiler verifies, not
   something the programmer has to write more of. That is the correct trade for a compiler
   meant to stay small.

Two things not to copy without thought. The `.odc` binary source format isolated the
language from ordinary tooling and version control; plain text is not negotiable. And the
absence of exception handling suits BlackBox because the framework owns the trap handler
and the process survives, a property a different failure model does not inherit
automatically.

Pfister's verdict on procedure types is also worth weighing for any design that wants
unloadable code: raw procedure pointers are what make safe unloading hard.

## See Also

- `zonnon.md` — the ETH successor line, which took the opposite view on operator
  overloading and built concurrency into the language
- `other-notable-dialects.md` — short entries for Active Oberon and the wider survey
- `ip-pascal.md` — the practical rather than minimalist branch of the family

## Open Questions

- The distributed `CP-New.pdf` is truncated. Its footers number 13 pages but the file
  holds 10, and two Wayback captures (2011 and 2019) are byte-identical, so the truncation
  is in the published artifact rather than the archive. Missing: section 4 "Specified
  domains of types", section 5 "Miscellaneous", section 6 "Acknowledgements". Section 4
  most likely covers the exact bit widths the language report now fixes for `BYTE`,
  `SHORTINT`, `INTEGER` and `LONGINT`, which Oberon-2 left implementation-defined, but
  that is inference and not read.
- Pfister says Component Pascal is a superset of Oberon-2 "except for some minor points"
  and never enumerates those points. Anything CP *removed* is therefore still unconfirmed.
- The exact `SET` width on the 64-bit implementations (Herschel, gpcp) was not verified.
  The report specifies 0..31.

## References

- C. Pfister, "What's New in Component Pascal?" (changes from Oberon-2 to CP), Oberon
  microsystems, March 2001. Original at `oberon.ch/pdf/CP-New.pdf`, now dead; archived at
  https://web.archive.org/web/20110515111149/http://www.oberon.ch/pdf/CP-New.pdf
- Component Pascal Language Report: https://blackbox.oberon.org/cp-lang.pdf
- Language report (HTML mirror):
  https://haugwarb.folk.ntnu.no/Programming/Oberon/oberon_definitions.html
- H. Mössenböck, N. Wirth, "The Programming Language Oberon-2," ETH Zürich, January 1992:
  https://www.ssw.uni-linz.ac.at/Research/Papers/Oberon2.pdf
- C. Szyperski, "Components and Objects Together," Dr. Dobb's, May 1999:
  https://web.archive.org/web/20180219231833/http://www.drdobbs.com/components-and-objects-together/184415685
- BlackBox Framework Center: https://blackboxframework.org/
- BlackBox Framework Wiki: https://wiki.blackboxframework.org/
- Community forums: https://community.blackboxframework.org/
- Wikipedia, Component Pascal: https://en.wikipedia.org/wiki/Component_Pascal
- Wikipedia, BlackBox Component Builder:
  https://en.wikipedia.org/wiki/BlackBox_Component_Builder
- Gardens Point Component Pascal: https://github.com/k-john-gough/gpcp
- Herschel, Component Pascal x64 compiler: https://herschel.oberon.org/
- CPFront, Component Pascal to C: https://github.com/Oleg-N-Cher/CPFront
- BlackBox source, `System/Mod/Kernel.odc` and `Meta.cp`:
  https://github.com/BlackBoxCenter/blackbox
- "BlackBox: a new object-oriented framework for CS1/CS2," ACM SIGCSE Bulletin 31(1):
  https://dl.acm.org/doi/10.1145/384266.299785
