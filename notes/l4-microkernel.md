# L4 Microkernel Family

## History and Origin

Created by **Jochen Liedtke** at **GMD** (German National Research Center for Computer Science), circa 1992–1995. Liedtke had previously built the Eumel OS and its successor L3. He observed that IPC was unacceptably slow in existing microkernels (including his own L3 and CMU's Mach), and rebuilt from scratch. The original L4 was written entirely in hand-coded i386 assembly.

Liedtke later moved to IBM Watson, then University of Karlsruhe. He died in 2001. His 1993/1995 IPC papers won the 2015 ACM SIGOPS Hall of Fame Award.

## Core Architecture

L4 embodies the **minimality principle**: a concept belongs in the kernel only if moving it outside would prevent implementing the system's required functionality.

L4 provides exactly **three abstractions**:
1. **Threads** — unit of execution
2. **Address spaces** — isolated virtual memory containers
3. **IPC** — sole mechanism for inter-thread communication

Everything else (drivers, filesystems, networking, paging) lives in user space.

## IPC Mechanism

The breakthrough that revived microkernel research after Mach nearly killed it:

- **Synchronous-only**: Send and receive combined in a single syscall. Sender blocks until receiver is ready. Eliminates buffering.
- **Register-based message passing**: Short messages travel in CPU registers with zero copies.
- **Direct process switch**: Kernel switches directly from sender to receiver, skipping the scheduler.
- **Performance**: ~10μs on contemporary hardware vs ~230μs for Mach and ~20μs for Unix pipes. A 20x improvement over Mach.

## Memory Management

Recursive construction of address spaces:
- Kernel starts with a single address space (sigma0) representing all physical memory.
- User-level **pagers** construct new address spaces using three kernel operations:
  - **Map**: Share a page from one address space to another
  - **Grant**: Transfer page ownership
  - **Flush/Unmap**: Remove mappings
- Page faults delivered to the faulting thread's pager via IPC.
- Entire VM policy is in user space.

## System Call Interface

The original L4 had just **7 system calls**:

| Syscall | Purpose |
|---------|---------|
| `ipc` | Send and/or receive messages |
| `id_nearest` | Find nearest communication partner |
| `fpage_unmap` | Unmap/flush virtual memory pages |
| `thread_switch` | Donate timeslice to another thread |
| `lthread_ex_regs` | Read/write thread registers |
| `thread_schedule` | Set scheduling parameters |
| `task_new` | Create/delete tasks (address spaces) |

For comparison: Linux has 400+ syscalls, Mach had ~130 traps.

## Scheduling

- Fixed-priority preemptive scheduling with 256 priority levels
- Round-robin within each priority level
- Interrupts modeled as threads (kernel sends IPC to handler)

## Code Size

| Implementation | Size | Language |
|----------------|------|----------|
| Original L4 (Liedtke) | ~12KB binary | i386 assembly |
| L4Ka::Hazelnut | ~10,000 LOC | C++ |
| L4Ka::Pistachio | ~15,000 LOC | C++ (portable, multi-arch) |
| seL4 | ~8,700 LOC C + 600 LOC asm | C/Assembly |
| Fiasco.OC | ~20,000–30,000 LOC | C++ |

The entire seL4 kernel is smaller than many single Linux drivers.

## Key Variants

### L4Ka::Pistachio (University of Karlsruhe)
- API version X.2 (v4), 2001. C++, portable (x86, ARM, MIPS, PowerPC, Alpha, Itanium, SPARC).
- Proved portable high-level-language microkernel could match assembly performance. BSD license.

### Fiasco / Fiasco.OC (TU Dresden)
- From 1998. C++, fully preemptible kernel. Low interrupt latency suitable for real-time.
- Forms kernel component of **L4Re** operating system framework. GPLv2.

### OKL4 (Open Kernel Labs / NICTA)
- Forked from Pistachio, optimized for embedded/mobile.
- First L4 with capability-based security (OKL4 2.1, 2008).
- Deployed on **all Qualcomm modem chipsets** — estimated 3+ billion devices.
- Acquired by General Dynamics (2012), later Cog Systems → Riverside Research (2025).

### seL4 (NICTA / Data61)
- ~8,700 LOC C + 600 LOC assembly.
- **World's first OS kernel with machine-checked functional correctness proof** (2009).
- Proofs: functional correctness, integrity/confidentiality, binary correctness (ARM), WCET bounds.
- Capability-based access control (inspired by KeyKOS/EROS).
- GPLv2 kernel, BSD user libs. Governed by seL4 Foundation (under Linux Foundation, 2020).
- Used in DARPA HACMS, military/aerospace, Neutrality's Atoll cloud platform.

## Performance vs. Monolithic Kernels

| Metric | L4 | Mach | Native Linux |
|--------|-----|------|-------------|
| IPC round-trip | ~10μs | ~230μs | ~20μs (pipe) |
| L4Linux overhead (macro) | ~6.3% | ~50%+ | baseline |

L4Linux demonstrated para-virtualized Linux on L4 with ~5% overhead for typical workloads.

## Key References

- Liedtke, "Improving IPC by Kernel Design" (SOSP 1993)
- Liedtke, "On Micro-Kernel Construction" (SOSP 1995)
- Hartig et al., "The Performance of Micro-Kernel-Based Systems" (SOSP 1997)
- Klein et al., "seL4: Formal Verification of an OS Kernel" (SOSP 2009)
- Elphinstone & Heiser, "From L3 to seL4" (SOSP 2013)
