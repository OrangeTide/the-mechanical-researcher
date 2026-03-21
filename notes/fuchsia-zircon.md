# Fuchsia / Zircon Microkernel

## Overview

- **Origin**: Google, ~2016. Zircon kernel derived from Little Kernel (LK) by Travis Geiselbrecht
- **Classification**: Object-based, capability-oriented microkernel (C++)
- **Deployed**: Google Nest Hub (1st/2nd gen), Nest Hub Max

## Architecture

Zircon is pragmatic — not minimal like L4, but far smaller than Linux. Trades theoretical minimality for practical security and usability.

Kernel handles: thread scheduling, VM management, IPC, interrupt handling, object lifecycle, capability enforcement. Everything else in userspace.

## Core Kernel Objects

### Execution
- **Job**: Group of related processes/child jobs (tree hierarchy)
- **Process**: Isolated address space
- **Thread**: Execution unit within a process

### IPC
- **Channel**: Bidirectional message pipe, two endpoints. Transfers data (≤64 KiB) and handles. Primary IPC mechanism.
- **Socket**: Streaming data transport (no handles)
- **FIFO**: Fixed-size ordered queue
- **Port**: Async notification queue

### Memory
- **VMO (Virtual Memory Object)**: Contiguous virtual memory region. Supports COW cloning, pager-backed, discardable.
- **VMAR**: Address space region where VMOs are mapped

### Synchronization
- Event, Event Pair, Timer, Futex

## IPC: Channels + FIDL

Channels are async, datagram-based, transfer both data and handles (capabilities). FIDL (Fuchsia Interface Definition Language) defines structured protocols on top — code gen for C++, Rust, Dart, Go.

## Scheduling

Weighted Fair Queuing (WFQ). Per-CPU scheduler instances. Deadline scheduling profiles also supported.

## System Calls

~170+ syscalls (target ~100 as API stabilizes). More than L4 (~7) but far fewer than Linux (~450+). Defined in machine-readable IDL, auto-generates bindings.

## Security Model

**No ambient authority**. Processes can only interact with objects for which they hold an explicit handle.
- No global filesystem namespace
- No "root" or superuser
- Handles carry rights bitmask (READ, WRITE, DUPLICATE, TRANSFER)
- Rights can only be attenuated, never escalated

## Component Framework

- Every piece of software is a **component** with a manifest (`.cml`)
- Capabilities explicitly routed through component tree (`use`/`expose`/`offer`)
- Statically auditable access control
- **Starnix**: Linux ABI compatibility in userspace (like WSL1)

## Code Size

~230,000–300,000 LOC C/C++ (kernel proper, excluding tests). Kernel image a few MB.

## Comparison

- **vs L4**: Zircon has 170+ syscalls vs 7. L4 is minimal; Zircon is pragmatic. Both capability-based.
- **vs Mach**: Both keep VM/scheduling in-kernel. Zircon's handle/rights model is simpler than Mach's ports.
- **vs QNX**: Both production microkernels. QNX is POSIX; Fuchsia is capability-first.

## Open Source

All source at fuchsia.dev. BSD/MIT/Apache 2.0 (no GPL). Contributions accepted.
