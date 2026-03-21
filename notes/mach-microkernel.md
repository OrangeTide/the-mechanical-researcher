# Mach Microkernel

## Overview

- **Origin**: Richard Rashid and Avie Tevanian, Carnegie Mellon University, 1985–1994
- **Classification**: First-generation microkernel
- **Legacy**: XNU (macOS/iOS), GNU Hurd, OSF/1, MkLinux

## History

Built atop 4.3BSD for Unix compatibility. Rashid had previously built the Accent system (1981). Tevanian later became VP of Software Technology at Apple and oversaw Mach's adoption into macOS.

## Five Fundamental Abstractions

1. **Tasks** — resource ownership (address space + port rights + threads)
2. **Threads** — CPU execution within a task
3. **Ports** — unidirectional, secure communication channels (capability-controlled)
4. **Messages** — typed data collections sent between ports (can contain port rights, inline/out-of-line data)
5. **Memory Objects** — VM regions manageable by user-space pagers

Every kernel object represented by a port. All management via message passing.

## IPC

Port-based message passing:
- Tasks acquire send rights to destination ports
- `mach_msg()` sends/receives
- Large messages use VM remapping (COW) to avoid copying
- **MIG** (Mach Interface Generator): stub compiler for procedural wrappers around message APIs

## Memory Management

Innovative VM system:
- Machine-independent VM layer (portable across architectures)
- **External pagers**: user-space servers manage page contents; kernel manages physical pages and page tables
- **Copy-on-write**: shadow objects track modifications, garbage-collected
- Mach's VM system adopted directly by FreeBSD; influenced Linux VM

## Mach 2.5 vs 3.0

| | Mach 2.5 | Mach 3.0 |
|---|----------|----------|
| BSD code | In-kernel (monolithic server) | User-space server (POE/UX) |
| Performance | ~25% slower than native BSD | ~50–67% slower than native |
| Size | Larger than BSD (Mach + BSD) | ~100K–150K LOC kernel |
| Adopted by | NeXTSTEP, OSF/1 | Research, GNU Hurd |

## The "Microkernel Tax"

IPC round-trip: ~100μs (vs L4's ~5μs on same hardware). Root causes:
1. **Cache pollution**: context switches flush caches/TLB
2. **Unnecessary copying** despite COW
3. **Overly general IPC**: complex message format penalizes common case
4. **Kernel bloat**: ~100K+ LOC working set competes for cache
5. **Address space switching**: full switch for every syscall redirect

Chen and Bershad found bulk of overhead was cache capacity misses, not IPC path length.

## Legacy

### XNU (macOS/iOS)
- Hybrid kernel: Mach 3.0 + FreeBSD in single address space
- Mach provides: VM, threading, IPC (Mach IPC still used by macOS userspace, e.g. XPC)
- BSD provides: POSIX, networking, VFS, security
- Avoids performance penalty by keeping everything in-kernel
- Apple added "security exclaves" (2025) — still innovating on the architecture

### GNU Hurd
- Mach 3.0 (GNU Mach fork) as microkernel
- Multi-server Unix. Never reached production quality.
- Complexity + Mach IPC overhead + Linux emergence = stalled development

### OSF/1 → Tru64
- Mach 2.5 for Open Software Foundation
- Became DEC Digital Unix → Tru64 (Alpha processors)

## Why L4 Was Created

Liedtke observed Mach's IPC was slow due to design choices, not inherent microkernel limitations:
1. Over-engineered IPC (complex message types for simple messages)
2. Kernel too large (cache pollution)
3. Not optimized for common case

L4 result: ~5μs IPC (20x faster than Mach) in ~12KB of assembly.

## What Mach Got Right
- Separation of concerns (tasks/threads/ports)
- Portable VM system
- Kernel threads (before POSIX threads)
- IPC as fundamental primitive
- Multiprocessor support from the start
- Copy-on-write semantics

## What Mach Got Wrong
- IPC too complex and slow
- Kernel too large for a "microkernel"
- External pagers over-ambitious
- Port-based security insufficient for practical policies
- Multi-server development too hard
