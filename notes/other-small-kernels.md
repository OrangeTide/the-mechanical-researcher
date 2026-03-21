# Other Small Kernels and RTOSes

Notes on kernels not covered in dedicated files. For dedicated coverage see:
l4-microkernel.md, qnx-microkernel.md, mach-microkernel.md, fuchsia-zircon.md,
horizon-kernel.md, threadx-picokernel.md, tron-kernel.md, freertos.md, minix3.md,
exokernel-mit.md, alternative-kernels.md

---

## Embedded / Real-Time Kernels

### VxWorks (1987, Wind River)

- RTOS, monolithic (optional user/kernel separation via RTP in VxWorks 6.x+)
- Priority preemptive, 256 levels, fully preemptible kernel. Deterministic interrupt latency.
- IPC: message queues, pipes, semaphores, signals, sockets, shared memory
- **The** flagship commercial RTOS: Mars Pathfinder (famous priority inversion bug, 1997),
  Spirit, Opportunity, Curiosity, Perseverance, Ingenuity helicopter, Boeing 787, Honda ASIMO
- DO-178C, IEC 62304 certified. Proprietary/commercial.

### NuttX (2007, Apache Foundation)

- RTOS, monolithic with optional "protected build" / "kernel build" (MPU/MMU separation)
- POSIX-compliant (unusually thorough) — VFS, BSD sockets, standard C library
- Min: 32 KB flash / 8 KB RAM. POSIX scheduling policies (FIFO, RR, sporadic).
- Flight OS for PX4/Pixhawk drones. Sony audio, Samsung IoT. Apache 2.0.

### eCos (1997, Cygnus/Red Hat → eCosCentric)

- Configurable monolithic RTOS. CDL configuration system selects individual components.
- Min: few KB upward. Bitmap scheduler (O(1), 32 levels) or multi-level queue.
- 100+ hardware platforms. Modified GPL with linking exception.

### LynxOS (1986, Lynx Software Technologies)

- Hard real-time RTOS, monolithic Unix-like. Full POSIX.1 conformant.
- 256-512 priority levels, fully preemptible kernel. ARINC 653 certified.
- Runs unmodified Linux binaries (LynxOS 5.0+). DO-178B/C avionics, military/defense.

### pSOS (1982, Software Components Group)

- Real-time kernel. Priority preemptive, 256 levels. Non-preemptible kernel.
- Dominant in 1980s-90s telecom (Lucent, Nortel, Ericsson), networking (Cisco).
- Wind River acquired → end-of-lifed for VxWorks. Historical significance only.

### RT-Thread (2006, Bernard Xiong, China)

- Layered RTOS. "Nano" edition: ~3 KB flash / 1.2 KB RAM minimum.
- O(1) bitmap scheduler, 8-256 priority levels. 500+ community packages.
- "RT-Thread Smart" adds user/kernel separation. Apache 2.0.

### Mbed OS (2014, ARM Holdings)

- RTOS for Cortex-M MCUs. Based on CMSIS-RTOS2. Min: ~8 KB RAM, 32 KB flash.
- ARM ended active development July 2022, archived 2023. Apache 2.0.

### Zephyr (2016, Linux Foundation)

- Modular monolithic RTOS. Min: <8 KB flash, <5 KB RAM.
- Scheduling: priority preemptive, EDF, Meta-IRQ. MPU support optional.
- 600+ boards, 15+ architectures. Linux-derived tooling (Kconfig, devicetree). Apache 2.0.

### RIOT OS (2013, FU Berlin et al.)

- Microkernel RTOS. Min: ~1.5 KB RAM, <5 KB ROM (MSP430).
- Tickless preemptive scheduler — extremely energy-efficient.
- Full IPv6/6LoWPAN stack. POSIX-like API. Runs as Linux process for dev. LGPL 2.1.

---

## Microkernel / Research OSes

### HelenOS (2004, Charles University, Prague)

- Multiserver microkernel. 8 CPU architectures.
- Complete OS with GUI, networking, USB, ext2/FAT — all as userspace servers, from scratch.
- Async, connection-oriented IPC. 16 priority queues. BSD license.

### Redox OS (2015, Jeremy Soller)

- Unix-like microkernel in Rust. ~30K LOC kernel.
- IPC via "schemes" — URL-based namespace (`disk:`, `tcp:`, `display:`).
- Own display server (Orbital), filesystem (RedoxFS), C library (relibc). MIT license.

### Xous (2020, bunnie Huang)

- Security-focused Rust microkernel for RISC-V soft-core (VexRiscv) on FPGA.
- Full-stack transparency: open FPGA RTL + firmware + OS. PDDB (plausibly deniable DB).
- IPC with ownership semantics (lend/move/share). Apache 2.0 / MIT dual.

### BareMetal OS (2007, Return Infinity)

- Exokernel in x86-64 assembly. <16 KB binary, 4 MB total RAM.
- Work queue: all cores poll, tasks run to completion. Zero context switch overhead.
- SMP native. BSD-like license.

---

## Educational Kernels

### xv6 (2006, MIT CSAIL)

- Reimplementation of Unix v6 in ANSI C. ~6,000-10,000 LOC.
- RISC-V port (xv6-riscv) now primary version (~2019).
- Round-robin scheduler (no priorities). Pipes + wait/exit. Inode-based FS with journaling.
- Free textbook. MIT 6.S081/6.1810. Adopted by dozens of universities. MIT license.

### XINU (1981, Douglas Comer, Purdue)

- "Xinu Is Not Unix." Few thousand lines of C.
- Priority-based preemptive. Single address space, no filesystem in kernel.
- IPC: single-word async messages, counting semaphores, ports.
- Textbook: "Operating System Design: The XINU Approach." Ported to RISC-V.

---

## Hybrid Kernels

### XNU (1996/2000, Apple via NeXT)

- Mach 3.0 microkernel + FreeBSD + I/O Kit, all in same address space.
- Scheduler: Mach multi-level feedback queue, 128 priority bands. Real-time: EDF variant.
- IPC: Mach ports for cross-process; internally BSD and Mach use direct function calls.
- I/O Kit: Embedded C++ driver framework with OO driver stacking.
- Powers all Apple platforms. Open under APSL as part of Darwin.

### VRM (1986, IBM)

- Virtual Resource Manager for IBM RT PC (ROMP RISC workstation).
- AIX 2.x ran as user-mode guest on top. Message passing between environments.
- Predates Mach. Precursor to IBM's hypervisor work (PowerVM). Historical interest only.
