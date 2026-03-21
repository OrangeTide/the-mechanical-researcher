# RISC-V Kernel Development Notes

## QEMU `virt` Machine

Best target for kernel dev. Provides PLIC, CLINT, VirtIO, optional PCI. Supports M+S+U modes.

Boot bare-metal:
```sh
qemu-system-riscv64 -machine virt -cpu rv64 -smp 1 -m 128M -nographic -bios none -kernel kernel.elf
```
With `-bios none`, kernel starts in M-mode at reset vector. With `-bios default`, OpenSBI handles M-mode, drops to S-mode at 0x80200000.

## RISC-V Privilege Levels

| Level | Name | Encoding | Purpose |
|-------|------|----------|---------|
| M | Machine | 3 | Highest. Always present. Direct hardware. Handles reset, NMI. |
| S | Supervisor | 1 | OS kernel. Virtual memory (page tables). Optional (S extension). |
| U | User | 0 | Unprivileged apps. Optional. |

Transitions: M→S via `mret` (set `mstatus.MPP`=01, `mepc`=entry). S→U via `sret`. U→S via `ecall` (cause=8).

## Sv39 Page Tables (RV64)

39-bit VA, three-level, 4 KiB pages. PTE format:
- Bits 53:10 = PPN, bits 7:0 = D,A,G,U,X,W,R,V
- R=W=X=0 + V=1 = pointer to next level. Any R/W/X set = leaf PTE.
- Superpages: level 2 leaf = 1 GiB, level 1 leaf = 2 MiB

Enable: write `satp` = (SV39_MODE << 60) | (root_pt >> 12), then `sfence.vma`.

## Context Switch

**Voluntary (callee-saved only)**: 14 registers (ra, sp, s0-s11). 14 sd + 14 ld = **28 instructions**.

**Full preemptive (from trap)**: 31 GPRs + sepc + sstatus + optional satp/sfence.vma = **~70-75 instructions**.

**RV32 (Hazard3)**: 31 sw + 31 lw + mepc + mstatus = **~66-68 instructions**.

## System Calls

`ecall` instruction triggers synchronous exception. Convention: `a7` = syscall number, `a0`-`a5` = args, return in `a0`. Handler reads `scause` (=8 for U-mode ecall), advances `sepc` by 4.

## Interrupt Handling

Key CSRs: `mtvec`/`stvec` (trap vector), `mcause`/`scause` (cause), `mepc`/`sepc` (exception PC), `mstatus`/`sstatus` (status), `mie`/`sie` (interrupt enable), `mip`/`sip` (interrupt pending).

`mtvec` modes: Direct (all traps → base) or Vectored (interrupts → base + 4*cause).

### PLIC (QEMU `virt`, base 0x0C000000)

External interrupt controller. Per-source priority, per-context enable/threshold, claim/complete protocol. UART0 = source 10.

### CLINT (base 0x02000000)

Per-hart timer (`mtime`/`mtimecmp`) and software interrupts (`msip`). Timer interrupt is primary preemption mechanism.

## RP2350 Hazard3 Cores

**Architecture**: RV32IMC (no atomics, no FPU, no S/U modes, no MMU).

- M-mode only. No virtual memory. No PMP.
- Up to 150 MHz. 3-stage pipeline.
- **No atomic instructions** — use 32 hardware spinlocks in SIO block.
- Switch to RISC-V: `cmake -DPICO_PLATFORM=rp2350-riscv ..` (SDK 2.0+)

### RP2350 Memory Map

- 0x00000000: 16 KiB boot ROM
- 0x10000000: up to 16 MiB external flash (XIP)
- 0x20000000: 520 KiB SRAM (8×64 KiB + 2×4 KiB)
- 0x40000000: APB peripherals
- 0x50000000: AHB-Lite (SIO: spinlocks, divider, GPIOs)
- 0xD0000000: SIO

### RP2350 Peripherals

30 GPIO, 2× UART, 2× SPI, 2× I2C, 12× PWM, 4× ADC (12-bit), 3× PIO blocks (4 state machines each), USB 1.1, 16 DMA channels, 2× 64-bit timers, 32 HW spinlocks, HSTX (DVI/HDMI), TRNG, SHA-256 accelerator.

## Olimex RP2350-PC

Open-source hardware board with RP2350. Adds over standard Pico 2:
- **16 MiB flash** (vs 4 MiB)
- **8 MiB QSPI PSRAM** (game-changer for kernel dev)
- **HDMI output** via HSTX — graphical console on real monitor
- **MicroSD card** — filesystem on real storage
- **Audio DAC**, **LiPo battery**, USB-C
- Open-source schematics

Makes RP2350 into a real computer: display, storage, input, audio.

## Two Development Paths

| Aspect | QEMU `virt` (rv64) | RP2350 Hazard3 (rv32) |
|--------|--------------------|-----------------------|
| Privilege | M + S + U | M only |
| MMU | Sv39 | None |
| Memory | Configurable GBs | 520 KiB (+8 MiB PSRAM on Olimex) |
| Interrupts | PLIC + CLINT | RP2350 IRQ controller |
| Atomics | Yes | No (use HW spinlocks) |
| Best for | Full OS with VM/processes | RTOS-style, bare-metal, drivers |

Progressive tutorial: start bare-metal on QEMU `virt` in M-mode (like RP2350), add S-mode with page tables (QEMU only), then port to Olimex RP2350-PC for physical hardware with HDMI.
