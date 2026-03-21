# QNX Microkernel (BlackBerry QNX)

## Overview

- **Origin**: Dan Dodge and Gordon Bell, University of Waterloo, founded Quantum Software Systems 1980. First release 1982.
- **Classification**: True microkernel, POSIX-compliant RTOS
- **Owner**: BlackBerry (acquired 2010 for $200M)
- **Deployed**: 275+ million vehicles, nuclear plants, medical devices

## Architecture

The kernel (`procnto`) contains only four things:
1. **CPU scheduling** — thread-level, POSIX real-time
2. **IPC** — synchronous message passing
3. **Interrupt redirection** — routing to user-space handlers
4. **Timers** — system clock management

Everything else in userspace: filesystems, drivers, networking, process manager, memory manager.

## Kernel Size

~23,000 SLOC of C. Famous demo: complete POSIX OS with GUI, TCP/IP, web browser on a **1.44 MB floppy disk**.

## IPC: Synchronous Message Passing

Three core functions:
- **MsgSend()**: Client sends, blocks until server receives + replies
- **MsgReceive()**: Server blocks waiting for messages
- **MsgReply()**: Server replies, unblocking client

Properties:
- Direct copy between address spaces (no kernel buffering) — approaches memory bandwidth
- Scatter-gather IOV for structured data
- Built-in flow control (sender blocks → no queue buildup)
- **Pulses**: 40-byte async notifications for interrupts/events

Channel/connection setup:
- `ChannelCreate()` — server creates receive channel
- `ConnectAttach()` — client connects to server's channel

## Resource Managers

QNX's key innovation: OS services as ordinary user-space programs that:
1. Register a pathname prefix (e.g., `/dev/ser1`, `/fs/qnx4/`)
2. Receive POSIX I/O messages (open, read, write, close) via IPC
3. Handle and reply

When app calls `open("/dev/ser1")`, C library constructs `io_open` message and sends via IPC. App doesn't know if it's talking to hardware driver, filesystem, or network service.

Benefits: drivers crash without kernel crash, can be restarted, loaded/unloaded at runtime.

## Scheduling

POSIX real-time policies:
- **SCHED_FIFO**: Run until block or preempted
- **SCHED_RR**: FIFO + time-slicing at same priority
- **SCHED_SPORADIC**: Bounded execution within replenishment period

256 priority levels. Fully preemptive. Deterministic context switch times.

### Adaptive Partitioning

- Each partition guaranteed minimum CPU budget (percentage)
- Under normal load: global priority scheduling
- Under overload: partitions guaranteed their budget
- Idle time redistributed to other partitions
- Dynamic creation/destruction at runtime

## Memory Management

- Full virtual memory with per-process address spaces
- Memory protection between processes
- Lazy physical allocation
- Guard pages for stack overflow detection
- ASLR supported
- POSIX shared memory (`shm_open`) for high-bandwidth IPC

## Safety Certifications

| Standard | Level | Domain |
|----------|-------|--------|
| IEC 61508 | SIL 3 | Industrial |
| ISO 26262 | ASIL D | Automotive (highest) |
| IEC 62304 | Class C | Medical (highest) |
| EN 50128 | SIL 4 | Railway |

## Applications

- **Automotive**: BMW, Bosch, Continental, Ford, Honda, Mercedes, Toyota, VW — ADAS, digital cockpit
- **Medical**: Ventilators, surgical systems, patient monitors
- **Industrial**: Factory automation, train control
- **Nuclear**: Power plant control systems
- **Defense/aerospace**: Various classified applications

## System Calls

~100 kernel calls (vs Linux 300+):
- Message passing: MsgSend, MsgReceive, MsgReply, MsgSendPulse
- Channels: ChannelCreate, ConnectAttach
- Threads: ThreadCreate, ThreadCtl
- Sync: SyncMutexLock, SyncCondvarWait
- Interrupts: InterruptAttach, InterruptWait
- Timers: TimerCreate, ClockTime

Higher-level POSIX calls (open, read, write) are C library constructs → message to resource manager. Not kernel calls.

## Open Source History

Briefly open-sourced (2007, Foundry27). Closed again on day of BlackBerry acquisition (2010). Non-commercial license introduced January 2025 for hobbyists.

## Comparison

- **vs Mach**: QNX proved true microkernel could achieve practical real-time performance without Mach's compromises
- **vs L4**: L4 has faster raw IPC (register-passing). QNX is more complete (POSIX, resource managers, certifications)
- **Summary**: Most commercially successful true microkernel. Most certified. Most deployed in safety-critical systems.
