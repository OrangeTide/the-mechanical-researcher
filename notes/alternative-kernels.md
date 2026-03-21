# Alternative Kernel Architectures (Nanokernels, Cache Kernels, Capability Systems)

## Nanokernel vs Microkernel

"Nanokernel" emerged mid-1990s. Meaningful distinction where it exists: a microkernel provides abstractions (threads, address spaces, IPC); a nanokernel provides only mechanism to multiplex hardware without imposing policy. Handles only interrupt dispatch and context switching, pushing everything else up. In practice, the line is blurry — L4 is called a microkernel but is smaller than some "nanokernels." Term largely fell out of academic use by 2010s.

## Stanford Cache Kernel (1994, Cheriton & Duda)

- ~15,000 LOC C. Kernel as stateless cache for OS objects.
- No persistent state in kernel. Caches threads, address spaces, page mappings on behalf of user-space "application kernels."
- When cache full, objects evicted back to application kernel (like hardware cache).
- Crash recovery: reboot kernel, repopulate from application kernels.
- Different application kernels implement different OS personalities simultaneously.
- Never became a product. Influenced exokernel designs and OS virtualization thinking.

## Adeos / I-pipe (2001-2002, Karim Yaghmour)

- ~5,000-8,000 lines (Linux patch). Purest nanokernel example.
- Thin interrupt-virtualization layer. Dispatches interrupts through priority-ordered "domain" chain.
- Primary use: Xenomai/RTAI real-time domain gets first crack at interrupts, Linux in lower domain.
- Linux never actually disables HW interrupts — Adeos virtualizes cli/sti.
- Gave Linux sub-10μs worst-case latency. Superseded by Dovetail and PREEMPT_RT.

## KeyKOS (1983, Key Logic — Norm Hardy, Charlie Landau)

- ~20,000 LOC. Grandfather of capability-based OS.
- Every object accessed through "keys" (capabilities). No ACLs, no superuser, no ambient authority.
- Persistent checkpointing: entire system state periodically saved to disk. Resume from checkpoint on crash.
- Ran on System/370. Small commercial deployment (phone company).
- Key Logic went out of business early 1990s.
- Legacy: directly inspired EROS, CapROS, seL4's capability model. Hardy coined "confused deputy."

## EROS (1991-2005, Jonathan Shapiro, UPenn)

- ~30,000 LOC C. Intellectual successor to KeyKOS.
- First OS to formally prove confinement (untrusted program provably can't exfiltrate data).
- Persistent checkpointing like KeyKOS. IPC within 2x of L4.
- Successor Coyotos never completed. CapROS fork continues.
- Direct influence on seL4's capability design.

## LSE/OS (late 1990s-2000s, Lockheed Martin)

- ~1,000-3,000 LOC. Embedded nanokernel / separation kernel.
- Only: partition scheduling (static TDMA), inter-partition communication, HAL.
- ARINC 653 partitioning. DO-178B/C, Common Criteria assurance.
- Used in actual Lockheed Martin weapon systems and avionics.
- Nanokernel applied to most demanding safety/security-critical use case.

## Symbian EKA2 (2004, Symbian Ltd./Psion)

- Nanokernel ~10K LOC C++/asm; full kernel ~50K LOC. Shipped in 1B+ Nokia phones.
- Two-layer: nanokernel (interrupts, scheduling, sync, MMU) + Symbian OS kernel (processes, drivers, FS, IPC).
- EKA2 innovation: fully preemptive with real-time guarantees. Any thread preemptible including in kernel mode.
- Allowed GSM/3G telephony stack (hard RT deadlines) on same processor as app OS.
- Nanokernel personality-agnostic (could theoretically host different OS on top).
- Died with Symbian/Nokia transition. Remarkably sophisticated for its era.

## Composite (2010-present, GWU, Gabriel Parmer)

- Kernel ~5,000-8,000 LOC C. Everything is a replaceable user-space component.
- Kernel provides only: capability invocation, thread dispatch, interrupt routing, memory mapping.
- Even scheduler is user-level — hierarchical user-level scheduling with capability-based resource delegation.
- "Temporal capabilities" grant bounded time quanta.
- IPC competitive with L4 (sub-microsecond on x86).
- Pushes minimality to logical conclusion: if scheduling is policy, why is it in the kernel?
