# MIT Exokernel (Aegis/ExOS, XOK)

## Overview

- **Origin**: Dawson Engler, M. Frans Kaashoek, James O'Toole Jr., MIT PDOS group, mid-1990s
- **Classification**: Exokernel with library OS
- **Status**: Research prototype (not actively maintained)

## Architecture

Radical separation: kernel handles **only protection and multiplexing** of raw hardware. Untrusted "library operating systems" (libOSes) implement all higher-level abstractions (VM, filesystems, networking).

Two implementations:
- **Aegis**: Proof-of-concept, limited storage
- **XOK**: More complete system
- **ExOS**: Primary library OS running atop both

## Key Design Principles

1. Separate protection from management
2. Expose hardware as directly as possible
3. Enable application-specific optimizations
4. Use "secure bindings" for resource attachment at load time

## Performance

- Aegis dispatches exceptions in **18 instructions**
- Protected control transfer **7x faster** than best alternatives
- Exception dispatch **5x faster**
- Cheetah web server on XOK: **8x faster** than NCSA/Harvest, **3–4x faster** than IIS on Windows NT

## Kernel Size

Tiny by constraint — implements neither message passing (as microkernels do) nor high-level abstractions (as monolithic kernels do). Exact LOC not published but architecturally guaranteed minimal.

## IPC and Scheduling

No traditional IPC — applications access hardware directly through libOS. Protected control transfer is the closest equivalent. Scheduling policy defined by libOS, not kernel.

## Significance

- Proved application-specific resource management yields dramatic performance gains
- Influenced: Nemesis, Barrelfish (MIT Multikernel), BareMetal OS
- Conceptual ancestor of modern unikernels (MirageOS, Unikraft)
- Demonstrated the exokernel concept but never reached production use
