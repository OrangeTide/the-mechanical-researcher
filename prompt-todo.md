# Research Project Ideas

- Virtual Machine Bytecodes — compare Dalvik, LLVM, Dis VM, Java bytecode,
  WASM, Quake QVM / stackvm
- Minimal Compilers — using c4.c as inspiration. Design compilers for stackvm.
  C, Pascal, and Tiny BASIC.
- X11/Wayland GUI Widget Toolkits — availability, features, pros/cons.
  Focus on C/C++ (native or wrapper).
- TUI Text User Interfaces — comparisons, availability, project status.
  Then a bare-bones embeddable single-file C header TUI library.
- X11 Window Managers — cover twm through modern tiling WMs. Ancestry,
  timeline, active status, project links (wayback if needed).

## Article Conventions

- Use "Revised: YYYY-MM-DD" for updated articles (not "Version 2" or "Updated")
- Multiple revisions: "Revised (2): date"
- Always cite sources — mandatory for any quoted or referenced material

## Microkernel Article Notes

- Introduction needs work: establish Mach's performance issues before
  introducing L4
- Explore embedded RTOS for comparison: NuttX, eCos, FreeRTOS, LynxOS,
  Mbed OS, Neutrino, pSOS, RT-Thread, ThreadX, VxWorks, VRM
- Also: XINU, xv6, XNU
- Hypervisors (Xen, ESX) intentionally out of scope
- Kernel writing guide: use RISC-V (QEMU) for platform details, not x86-64/ARM
  - Physical platform: Raspberry Pi Pico 2 in RISC-V mode (RP2350)
  - Alternative: Olimex RP2350pc (https://github.com/OLIMEX/RP2350pc)
- Differentiator: runtime verification (like Linux kernel rv)
  https://www.kernel.org/doc/html/latest/trace/rv/runtime-verification.html
