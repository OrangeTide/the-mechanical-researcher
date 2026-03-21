# MINIX 3

## Overview

- **Origin**: Andrew Tanenbaum, 2005 (reimplementation focused on reliability, not education like MINIX 1/2)
- **Classification**: Microkernel
- **Notable**: Found in Intel Management Engine — arguably most widely deployed microkernel by unit count

## Architecture

Classic microkernel: kernel handles only scheduling, IPC, interrupt handling. Everything else in isolated user-mode processes:
- Device drivers
- File systems
- Network stack
- Memory manager
- **Reincarnation server** — monitors all components, transparently restarts failures

Later adopted NetBSD's userland for POSIX compatibility.

## Key Design Principles

1. Extreme fault isolation (every driver/server in own address space)
2. Self-healing via reincarnation server
3. Minimal TCB: ~20,000 lines (vs millions for Linux/Windows)
4. Principle of least authority

## Kernel Size

~6,000 lines of executable kernel code. TCB ~20,000 lines including critical servers.

## IPC and Scheduling

- Synchronous message-passing (rendezvous)
- Fixed-size messages copied by kernel
- Fixed-priority preemptive scheduler

## Fault Tolerance

In fault injection studies: survived **99% of 200+ injected driver faults** by isolating and replacing faulty components. Reincarnation server restarts crashed drivers without affecting running applications.

## Open Source

BSD license. github.com/Stichting-MINIX-Research-Foundation/minix

## Significance

- Empirically demonstrates microkernel isolation superiority for reliability
- Intel ME deployment makes it ubiquitous (in every Intel CPU since ~2015)
- Excellent teaching tool for microkernel concepts
