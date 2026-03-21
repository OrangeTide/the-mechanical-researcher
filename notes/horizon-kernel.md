# Nintendo Horizon Microkernel

## Overview

- **Origin**: Nintendo. Evolved from 3DS (ARM11) to Switch/NX (AArch64 Cortex-A57)
- **Classification**: True microkernel, capability-based
- **Deployed**: Hundreds of millions of 3DS + Switch consoles
- **Open reimplementation**: Mesosphere (Atmosphère project, C++)

## Architecture

True microkernel. Kernel handles only:
- Thread/process management
- Memory management (page tables, address spaces)
- Scheduling (priority-based preemptive)
- IPC primitives
- Synchronization objects
- Interrupt routing

All drivers in userspace, including Nvidia GPU driver. HOME Menu assets <200 KB.

### ARM Exception Levels
- EL0: Applications, games, sysmodules
- EL1: Horizon kernel
- EL3: Secure Monitor (TrustZone — crypto, power, keys)

## Kernel Object Model

Base class: `KAutoObject`. Key types:
- **KThread**, **KProcess** — execution
- **KServerPort/KClientPort** — IPC port endpoints
- **KServerSession/KClientSession** — IPC sessions
- **KSharedMemory**, **KTransferMemory** — memory sharing
- **KDeviceAddressSpace** — IOMMU/SMMU for DMA
- **KResourceLimit** — per-process quotas
- **KEvent** — signaling

All objects via slab allocators with fixed counts. Handle table: 1024 entries per process.

## System Calls (SVCs)

~103 SVCs, ~45 available to typical userland. Key categories:
- Memory: `SetHeapSize`, `MapSharedMemory`, `CreateTransferMemory`
- Thread: `CreateThread`, `StartThread`, `SleepThread`
- Sync: `WaitSynchronization`, `SignalEvent`, `ArbitrateLock`
- IPC: `SendSyncRequest`, `ConnectToNamedPort`, `ReplyAndReceive`

## IPC Architecture

Three layers:
1. **HIPC** (kernel): Raw transport. Buffer descriptor types A/B/W/X/C for different memory mapping patterns.
2. **CMIF**: Standard protocol with **domains** — multiplex multiple logical sessions through one kernel session handle.
3. **TIPC**: Lightweight protocol for Service Manager only.

Services registered with Service Manager ("sm:") by name (≤8 chars). NPDM metadata whitelists accessible services.

## Security Model (NPDM)

- **ACID** (signed by Nintendo): Maximum permission set
- **ACI0**: Requested permissions
- Final = ACI0 & ACID (intersection — can never exceed signed permissions)
- Per-process SVC access masks (64-bit)
- Service name whitelists (~40 user services for games)
- Memory region controls
- ASLR on all processes

## Memory Management

- Per-process virtual address spaces via MMU
- Slab-only kernel allocation (no general heap)
- Heap must be 2 MB aligned
- Shared/transfer memory for cross-process data
- Device address spaces for DMA via SMMU

## Scheduling

- Priority-based preemptive (priorities 0-63, lower = higher)
- Core affinity per thread
- Switch: 3 cores for games, 1 for system services

## What Makes It Interesting

1. True microkernel purity — even GPU driver in userspace
2. Capability-based security with NPDM intersection model
3. Domain-multiplexed IPC (CMIF) works around session limits elegantly
4. Slab-only kernel allocation — no fragmentation, no general heap
5. Gaming-optimized: minimal overhead, maximum resource availability
6. Successfully ported from ARM11 (3DS) to AArch64 (Switch)

## Open Source Reimplementations

- **Mesosphere** (Atmosphère): C++ clean-room kernel reimplementation, boots most games
- **Exosphere**: Secure Monitor reimplementation
- **switchbrew.org**: Comprehensive reverse-engineered documentation
- **libnx**: Homebrew library with full SVC bindings
