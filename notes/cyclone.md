# Cyclone

**Designers:** Trevor Jim, Greg Morrisett, Dan Grossman, Michael Hicks, James Cheney, Yanling Wang
**Institutions:** AT&T Labs Research, Cornell University, University of Washington, University of Maryland
**Period:** 2001–2006
**Final release:** 0.9 (February 2006)
**Implementation:** ~35K LOC, self-hosted (Cyclone compiler compiles itself), compiles to C via GCC backend
**Wikipedia:** https://en.wikipedia.org/wiki/Cyclone_(programming_language)

## Overview

Cyclone is a type-safe dialect of C designed as a practical language for writing systems
software without sacrificing the low-level control that C programmers expect. Rather than
designing a new language from scratch, Cyclone starts from the complete C specification
and adds the minimum type system machinery needed to prevent undefined behavior —
buffer overruns, dangling pointers, null dereferences, format string attacks, and
uninitialized memory access.

The language was developed iteratively: the team first catalogued every source of
unsafety in C, then designed the least-invasive restrictions and extensions to eliminate
each one. The result looks and feels like C but provides the memory safety guarantees
typically associated with languages like Java or ML.

## Safety Restrictions on C

Cyclone removes or restricts several C features that enable undefined behavior:

| Feature | Restriction |
|---------|------------|
| `NULL` dereference | Never-NULL pointer types (`@`, `*` with `@notnull`); nullable pointers require null-check before dereference |
| Buffer overrun | Fat pointers (`?`) carry bounds; array subscript checked at runtime |
| Dangling pointers | Region type system tracks pointer lifetimes; compiler rejects programs that use freed memory |
| Unions | Only "tagged" unions allowed (discriminated, like ML datatypes) |
| Casts | Restricted; no arbitrary pointer-to-pointer casts that break type safety |
| `va_arg` / varargs | Replaced with type-safe varargs using fat pointers |
| `setjmp`/`longjmp` | Restricted to prevent stack-smashing |
| `goto` | Restricted to prevent bypassing initialization |
| `sizeof` on expressions | Disallowed where it would reveal representation of abstract types |
| Pointer arithmetic | Only on fat pointers with bounds checks |

## Pointer System

Cyclone's pointer system is the core of its type safety. Every pointer carries three
pieces of information in its type: a **region** (where it points), an **aliasability
qualifier** (how many references exist), and a **bounds** qualifier (what range of
memory is accessible).

### Pointer Kinds

| Syntax | Name | Bounds | Nullable | Notes |
|--------|------|--------|----------|-------|
| `T *` | Thin pointer | Single element | Yes | Like C; default is nullable |
| `T @` | Never-NULL thin | Single element | No | Compiler inserts null checks at creation |
| `T ?` | Fat pointer | Runtime bounds | Yes | Carries `(base, bound, current)` triple |
| `T *@numelts(n)` | Bounded thin | Compile-time `n` | Yes | Zero-cost bounds via type, not runtime triple |

The full pointer type syntax is:

```
T * @aqual('q) @region('r)
```

where `'q` is an aliasability qualifier and `'r` is a region. For example,
`int *@aqual(\U) @region(\H)` is a unique pointer to a heap-allocated integer.

### Aliasability Qualifiers

| Qualifier | Meaning |
|-----------|---------|
| `\A` | Aliasable — any number of pointers may reference this object |
| `\U` | Unique — exactly one pointer references this object |
| `\RC` | Reference-counted — runtime count tracks references |
| `top` | Top of qualifier lattice — subsumes all others; used for reading but not writing |

The subtyping lattice: `\A` ≤ `top`, `\U` ≤ `top`, `\RC` ≤ `top`. There is no
subtyping between `\A`, `\U`, and `\RC` themselves.

## Region-Based Memory Management

Cyclone's memory management system offers six region varieties, giving programmers a
spectrum from fully automatic (GC) to fully manual (unique pointers), with several
points in between.

### Region Varieties

| Variety | Lifetime | Allocation | Deallocation | Cost |
|---------|----------|------------|--------------|------|
| Stack (`'r`) | Lexical scope | Automatic | End of scope | Zero overhead |
| LIFO arena (`region r`) | Lexical scope | `rmalloc(r, ...)` | End of scope | Small arena overhead |
| Dynamic arena (`uregion_key_t`) | Programmer-controlled | `rmalloc(r, ...)` via `open` | Explicit `free_ukey(k)` | Key management overhead |
| Heap (`\H`) | GC-managed | `malloc(...)` | Garbage collector | GC pauses |
| Unique (`\U`) | Programmer-controlled | `unique_malloc(...)` | `free(p)` consumes pointer | Must track uniqueness |
| Reference-counted (`\RC`) | Programmer-controlled | `rc_malloc(...)` | `drop(p)` decrements count | Count overhead per alias |
| Reap (`reap<'r> h`) | Arena + per-object free | `rmalloc(r, ...)` | `rfree(r, p)` or end of scope | Arena + free-list overhead |

### Stack Regions

Every local variable and block scope creates an implicit region. Pointers into stack
frames are safe because the type system tracks which region each pointer references and
prevents returning pointers to expired frames:

```c
int *bad(void) {
    int x = 42;
    return &x;  // REJECTED: pointer into local region escapes
}
```

### LIFO Arenas

A lexically-scoped arena with bump-pointer allocation. All objects allocated into the
arena are freed simultaneously when the scope exits:

```c
region r {
    int *@region(r) p = rmalloc(r, sizeof(int));
    *p = 42;
}  // all memory in region r freed here
```

### Dynamic Arenas

Arenas whose lifetime is not tied to a lexical scope. A unique key controls access:

```c
uregion_key_t<'r> key = new_ukey();
// ... pass key around ...
{
    open(key) {
        // inside open block, can allocate into 'r
        int *@region(r) p = rmalloc(r, sizeof(int));
    }
}
free_ukey(key);  // consumes the unique key, freeing the arena
```

The `open` construct temporarily borrows the unique key, creating a lexical scope where
the dynamic region is accessible. This prevents use-after-free: once `free_ukey`
consumes the key, no code can open the region again.

### Unique Pointers

Unique pointers (`*\U`) guarantee exactly one reference to an object. This enables
safe explicit deallocation — when `free(p)` is called, the type system ensures no
other pointer aliases `p`:

```c
int *\U p = unique_malloc(sizeof(int));
*p = 42;
free(p);     // p is consumed; any subsequent use is a compile error
```

The **atomic swap** operator `:=:` enables manipulating unique pointers stored in
aliasable data structures. It swaps two pointer values in a single operation, maintaining
the uniqueness invariant:

```c
int *\U temp = NULL;
temp :=: container->field;  // atomically swap temp and field
// now temp holds the old value, field holds NULL
```

### The `alias` Construct

The `alias` construct provides temporary borrowing — it takes a tracked pointer (unique
or reference-counted) and creates a temporary aliasable reference in a fresh lexical
region:

```c
int *\U p = unique_malloc(sizeof(int));
alias <'r> int *@region('r) q = p;
{
    // inside this block, q is a regular aliasable pointer into region 'r
    // p is temporarily inaccessible (consumed for the duration)
    printf("%d\n", *q);
}
// p is restored after the block
```

### Reaps

Reaps combine arena allocation with optional per-object deallocation. Objects in a
reap can be individually freed with `rfree`, and the entire reap is freed when the
scope exits. The `single('r)` constraint ensures that pointers into the reap cannot
outlive it:

```c
reap<'r> h {
    int *@region('r) p = rmalloc(r, sizeof(int));
    int *@region('r) q = rmalloc(r, sizeof(int));
    rfree(r, p);  // free just p; q remains valid
}  // q and any remaining allocations freed here
```

### General Allocation Function

The fully general allocation function is:

```c
void *@aqual('q) @region('r) rqmalloc(region_t<'r> r, aqual_t<'q> q, sizeof_t<T> sz);
```

This takes a region handle, aliasability qualifier handle, and size, returning a pointer
with the requested region and aliasability. The specialized forms (`rmalloc`, `malloc`,
`unique_malloc`, `rc_malloc`) are conveniences that fix some of these parameters.

## Region Type System

### Region Subtyping

Region subtyping is based on the "outlives" relation. If region `'r1` outlives `'r2`
(i.e., `'r1` is still live whenever `'r2` is), then `T *@region('r1)` is a subtype of
`T *@region('r2)`. This is sound because a pointer into a longer-lived region is
always valid where a pointer into a shorter-lived region would be.

### The `regions_of` Operator

Rather than using explicit effect variables (as in Tofte-Talpin), Cyclone uses a type
operator `regions_of(τ)` that computes the set of regions accessible through a type.
This is used in function types to express which regions a function might access:

```c
void f(int *@region('r) p; regions_of('r) ⊆ 'e);
```

This avoids the "region polymorphism explosion" problem where functions would need
many region parameters just to express which regions they access.

### Formal Soundness

The core type system has been proven sound (preservation + progress) via a formal
Core Cyclone calculus. The ESOP '06 paper presents λ^rgnUL, a substructural calculus
that unifies regions, linearity (unique pointers), and locks under a single framework,
proving that "linear regions are all you need" — unique pointers and reference counting
can be encoded using linear capabilities over regions.

## Parametric Polymorphism

Cyclone supports ML/Haskell-style parametric polymorphism with separate compilation:

```c
`a id(`a x) { return x; }

void *`a List::map(`b f(`a), List<`a> lst);
```

Type parameters use backtick syntax (`` `a ``, `` `b ``). Instantiation is implicit —
the compiler infers type arguments at call sites.

### Kind System

Two kinds control which types can be used as type arguments:

| Kind | Name | Meaning |
|------|------|---------|
| `A` | Any | Types of any size, including aggregates |
| `B` | Boxed | Types that fit in a machine word (pointers, ints) |

By default, type variables have kind `B`. This matters because polymorphic functions
must know calling conventions — a function polymorphic over kind `A` types cannot pass
them in registers, while kind `B` types always fit in a word.

### Existential Types

Cyclone supports existential types for abstracting over type parameters in data
structures, useful for closures, callbacks, and abstract data types:

```c
struct Closure {
    <`a>
    `a env;
    int (*f)(`a);
};
```

The type variable `` `a `` is existentially quantified — each `Closure` instance hides
its own environment type. Opening the existential (pattern matching to extract `env`
and `f`) gives a fresh abstract type that can only be used consistently.

### Mutable Existential Packages

Mutable existential types (where the packed fields can be updated) present a soundness
challenge. Cyclone uses the "no aliases at opened type" approach: when unpacking an
existential, the original package cannot be accessed until the unpacked scope ends.
Reference patterns (`*id`) acquire field addresses safely within this discipline.

### Polymorphic References

The interaction of polymorphism with mutable references (the classic ML `ref` problem)
is solved by forbidding type instantiation expressions from appearing as left-values.
This prevents writing a value of type `T1` through a reference typed at `T2`.

## Type-Safe Multithreading

The TLDI '03 paper extends Cyclone with type-safe multithreading:

### Lock Types

Each lock has a **lock name** `'l` in the type system. Shared data must be protected
by a specific lock, and the type system tracks which locks are held:

```c
lock_t<'l> my_lock;
int *@region('l) shared_data;  // data protected by my_lock
```

Accessing `shared_data` requires holding `my_lock`. The type system verifies this
statically.

### Sharability

A new kind axis (S = sharable, U = unsharable) prevents thread-unsafe types from
being shared:

| Kind | Meaning |
|------|---------|
| `S` | Sharable — can be passed between threads |
| `U` | Unsharable — confined to a single thread |

Types containing thread-local pointers (stack references, non-locked heap pointers) are
kind `U`. Only `S`-kinded types can be passed to `spawn`.

### Primitives

- `spawn(f, arg)` — create a thread running `f(arg)`; `arg` must be `S`-kinded
- `sync(lock) { ... }` — acquire lock, execute body, release; within the body, the
  lock name `'l` is in scope, allowing access to `'l`-protected data
- `nonlock(lock)` — assert that a lock is not held; prevents deadlock from
  re-acquisition

## Comparison with C and Other Languages

### Porting Effort

Benchmarks from porting real C programs to Cyclone:

| Program | C LOC | Lines Changed | % Changed | Category |
|---------|-------|--------------|-----------|----------|
| Boa (web server) | 6,700 | ~350 | 5% | Basic safety only |
| BetaFTPD | 1,500 | ~270 | 18% | Basic safety only |
| Epic (compression) | 3,100 | ~230 | 7% | Basic + manual MM |
| KissFFT | 600 | ~30 | 5% | Basic safety only |
| CycScheme | 4,700 | ~850 | 18% | Basic + manual MM |
| MediaNet (overlay) | 15,000 | ~2,300 | 15% | New Cyclone code |
| 8139too (Linux driver) | 1,990 | ~200 | 10% | Kernel driver |
| i810_audio (Linux driver) | 3,300 | ~400 | 12% | Kernel driver |
| pwc (Linux driver) | 2,400 | ~300 | 12% | Kernel driver |

Most changes fall into predictable categories: adding pointer annotations, inserting
null checks, replacing unions with tagged unions, and wrapping arrays with fat pointers.

### Performance

| Program | Cyclone vs. C | Notes |
|---------|--------------|-------|
| Boa (web server) | 0–2% slower | I/O-bound; safety checks negligible |
| BetaFTPD | ~5% slower | I/O-bound |
| Cfrac (bignum math) | ~66% slower | Compute-bound; bounds checks on inner loops |
| Epic (compression) | ~15% slower | Compute-bound |
| KissFFT | ~10% slower | Compute-bound |
| CycScheme | ~30% slower | Interpreter overhead |

The primary overhead comes from bounds checking on fat pointers in tight loops. For
I/O-bound programs, the overhead is negligible. Bounded thin pointers
(`*@numelts(n)`) can eliminate fat pointer overhead when array sizes are statically
known.

## Key Design Decisions

**Why start from C, not design a new language?** The team believed that C programmers
would not adopt a new language (Java, ML) for systems work because those languages
remove essential capabilities: manual memory layout, stack allocation, and low-level
hardware access. By starting from C and adding safety, they preserved the mental model
and interoperability.

**Why not just GC?** Conservative GC alone is insufficient for systems code that needs
deterministic memory management, real-time constraints, or operates without an OS (e.g.,
kernel drivers). Cyclone offers GC as one option among six, letting programmers choose
the right strategy per data structure.

**Why regions instead of Rust-style ownership?** Cyclone predates Rust by nearly a
decade. Its region system (based on Tofte-Talpin) was the state of the art for static
memory safety in the early 2000s. Rust's borrow checker was significantly influenced by
Cyclone's work on regions and unique pointers.

**Why existential types instead of virtual dispatch?** Cyclone avoids adding
object-oriented features. Existential types provide the same capability (type-abstract
callbacks, closures, polymorphic containers) within the C-like value semantics model.

## Influence

Cyclone's ideas directly influenced:

- **Rust** — borrow checker, ownership types, region-like lifetimes, `Option` instead
  of nullable pointers
- **Checked C** (Microsoft) — bounds-checked pointers for C
- **Deputy** (Berkeley) — dependent types for C safety annotations
- **CCured** — type inference approach to C safety (a contemporaneous alternative)

## Source Papers

1. Trevor Jim et al., "Cyclone: A Safe Dialect of C" — C/C++ User's Journal overview (2002)
2. Dan Grossman et al., "Region-Based Memory Management in Cyclone" — PLDI 2002
3. Michael Hicks, Greg Morrisett, Dan Grossman, Trevor Jim, "Experience with Safe Manual Memory Management in Cyclone" — ISMM 2004
4. Dan Grossman, "Type-Safe Multithreading in Cyclone" — TLDI 2003
5. Dan Grossman, "Quantified Types in an Imperative Language" — ACM TOPLAS 2006
6. Michael Hicks, Greg Morrisett, Dan Grossman, Trevor Jim, "Safe and Flexible Memory Management in Cyclone" — University of Maryland CS-TR-4514 (2003)
7. Dan Grossman et al., "Cyclone: A Type-Safe Dialect of C" — C/C++ Users Journal (2005)
8. Matthew Fluet, Greg Morrisett, Amal Ahmed, "Linear Regions Are All You Need" — ESOP 2006
9. Nikhil Swamy, Michael Hicks, Greg Morrisett, Dan Grossman, Trevor Jim, "Safe Manual Memory Management in Cyclone" — Science of Computer Programming (2006)
