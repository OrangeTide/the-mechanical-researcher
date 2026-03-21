# TRON (The Real-time Operating System Nucleus)

## Overview

- **Origin**: Ken Sakamura, University of Tokyo, 1984
- **Classification**: Real-time kernel specification (not a microkernel in the L4/Mach sense)
- **IEEE Milestone**: May 2023
- **Market share**: 60%+ of Japan's embedded RTOS market for 26 consecutive years

## Architecture

TRON is a **specification family**, not a single kernel. Vendors implement independently.

Sub-architectures:
- **ITRON** — industrial real-time (dominant success)
- **BTRON** — business/personal computers
- **CTRON** — mainframes/communications
- **MTRON** — network interconnection

### Key Design: "Weak Standardization"

Only functional API semantics are standardized. Implementation details are unspecified — each vendor optimizes for target hardware. Functions are independent; kernels supplied as link-time libraries.

This contrasts with POSIX-style strong standardization. No binary portability, but extreme efficiency.

## Variants

| Variant | Year | Target | Notes |
|---------|------|--------|-------|
| ITRON1 | 1987 | 8/16-bit MCUs | First spec |
| uITRON 2.0 | 1989 | 8/16-bit | Small-system variant |
| uITRON 3.0 | ~1993 | 8-32-bit | Unified small/large |
| uITRON 4.0 | 1999 | 8-32-bit | Current standard; added mutexes, data queues |
| T-Kernel | 2002 | 32-bit + MMU | Subsystem/device driver management |
| T-Kernel 2.0 | ~2011 | 32-bit + MMU | Enhanced spec |
| uT-Kernel 3.0 | 2019 | IoT edge | IEEE 2050-2018 compliant |

## IPC and Synchronization

- **Semaphores**: `cre_sem`, `sig_sem`, `wai_sem`
- **Event Flags**: multi-bit condition notification
- **Data Queues**: fixed-size word transfer
- **Mailboxes**: zero-copy message passing via memory address
- **Mutexes**: priority inheritance/ceiling
- **Message Buffers**: variable-length messages

Mailbox IPC is notable: only the address is sent — receiver accesses message directly. Zero-copy within shared address space.

## Scheduling

- Priority-based preemptive (T-Kernel: priorities 1-140)
- FIFO within same priority
- Task states: RUNNING, READY, WAITING, SUSPENDED, DORMANT, NON-EXISTENT

## Memory Management

- **uITRON**: No MMU required. Fixed-size and variable-size memory pools. Single address space.
- **T-Kernel**: Optional MMU support. Per-task address spaces. System memory management for middleware isolation.
- **uT-Kernel**: No MMU, no virtual memory.

## Kernel Size

- Minimal uITRON 4.0: **~2.4 KB ROM**
- Library-linked: only used functions included
- So simple that "many users developed their own versions for in-house use"

## Applications

- Toyota engine ECUs
- Japanese cellphones (historically most)
- Digital cameras, rice cookers, A/V equipment
- Factory automation, process control
- IoT edge nodes (uT-Kernel 3.0)

## Open Source

- uT-Kernel 3.0: T-License 2.2, github.com/tron-forum/mtkernel_3
- TOPPERS/ASP: open source uITRON 4.0 implementation
- eCos includes uITRON compatibility layer

## Significance for This Survey

TRON demonstrates that a deliberately underspecified API standard with the right abstractions can achieve wider deployment than precisely specified kernels. Trade-off: no binary portability, no isolation guarantees, but extreme efficiency for co-designed embedded systems.
