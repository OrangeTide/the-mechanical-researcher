# FreeRTOS

## Overview

- **Origin**: Richard Barry, 2003. AWS/Amazon since 2017.
- **Classification**: Real-time scheduling kernel (not a full OS)
- **License**: MIT
- **Deployed**: Billions of devices, most popular RTOS worldwide

## Architecture

Not an OS in the Unix sense — a scheduling kernel providing task management, sync primitives, and memory allocation. No memory protection, no filesystem, no process isolation in base kernel. Single address space. Clean separation: portable C core + architecture-specific port layers.

## Kernel Size

- **4–9 KB binary** (ROM)
- Core: ~4–5 C source files
- Fully statically-allocatable (no dynamic memory required)
- 40+ architecture ports

## Scheduling

Prioritized preemptive with optional time-slicing for equal-priority tasks. Both cooperative and preemptive modes supported.

## IPC

- **Queues**: Fundamental mechanism — semaphores and mutexes built on queues
- **Task notifications**: Lightweight 1-to-1 signaling
- **Stream/message buffers**
- **Event groups**
- All primitives ISR-safe with separate API variants

## Memory

Five heap schemes (heap_1 through heap_5) with different tradeoff profiles. No MMU/MPU in base config (MPU port available).

## Safety

SafeRTOS: formally verified variant for IEC 61508, DO-178C certification.

## Significance

Most widely deployed RTOS. Simple, tiny, well-understood. But no isolation — purely a scheduler with IPC primitives for bare-metal MCUs.
