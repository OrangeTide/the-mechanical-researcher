# ThreadX (Eclipse ThreadX) — Picokernel

## Overview

- **Origin**: William Lamie, Express Logic (1996). Acquired by Microsoft (2019), transferred to Eclipse Foundation (2023)
- **Classification**: Picokernel — non-layered, services plug directly into core
- **License**: MIT (since Eclipse transfer)
- **Deployed**: 12+ billion devices

## Picokernel Architecture

"Picokernel" = kernel services plug directly into the central scheduler with no message-passing layers. Unlike microkernels (layered with IPC), ThreadX uses direct coupling for maximum performance. Essentially a monolithic kernel small enough to warrant a different name.

## Kernel Size

- **ROM**: 2–20 KB depending on features
- **RAM baseline**: ~1 KB + per-thread stacks
- Individual modules: semaphore ~450 bytes, mutex ~1,200 bytes
- Dead-code elimination: only used services linked
- Source: ~185 C files + 30+ architecture-specific assembly ports (55.9% C, 40.1% assembly)

## Scheduling

- **Priority-based preemptive** with 32–1,024 configurable levels
- **Time-slicing** for equal-priority threads (round-robin within a band)
- **Cooperative**: explicit `tx_thread_relinquish()`
- **Preemption-threshold** (unique to ThreadX): thread specifies a threshold priority — only threads above threshold can preempt. Reduces context switches and synchronization needs.

## IPC Mechanisms

- **Message Queues**: Fixed-size (1–16 ULONGs), configurable depth, front-send for urgent messages
- **Counting Semaphores**: ceiling put variant
- **Mutexes**: ownership tracking, priority inheritance, recursive locking
- **Event Flags**: 32-bit groups, AND/OR wait, auto-clear variants

## Memory Management

- **Block Pools**: Fixed-size blocks, O(1) alloc/dealloc, deterministic, no fragmentation
- **Byte Pools**: Variable-size, first-fit, non-deterministic (like malloc)

## API Surface

~80 functions in 9 categories: Thread (18), Queue (11), Semaphore (10), Mutex (8), Event Flags (8), Block Pool (8), Byte Pool (8), Timer (8), System (3).

Error-checking wrappers (`txe_*`) can be compiled out for production.

## Performance

At 200 MHz ARM Cortex-M class:
- Context switch: ~0.6 μs
- Semaphore get/put: ~0.2 μs
- Queue send/receive: ~0.3 μs
- System boot: <120 cycles

## Safety Certifications

IEC 61508 SIL 4, IEC 62304 Class C, ISO 26262 ASIL D, EN 50128 SIL 4, DO-178B/C. MISRA-C compliant.

## Companion Modules

| Module | Purpose |
|--------|---------|
| NetX Duo | TCP/IP stack (IPv4/IPv6) |
| FileX | FAT-compatible filesystem |
| GUIX | Embedded GUI |
| USBX | USB host/device |
| LevelX | Flash wear-leveling |

## What Makes It Interesting

1. 2 KB ROM is not a toy — runs on billions of real devices
2. Preemption-threshold is a genuine scheduling innovation
3. Assembly-per-port philosophy: scheduler hand-written for 30+ architectures
4. Safety certified at highest levels — same binary in pacemakers and jet engines
5. Complete platform (networking, filesystem, GUI, USB) in small footprint
6. Now truly open source under MIT
