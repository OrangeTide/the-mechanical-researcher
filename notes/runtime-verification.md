# Runtime Verification for OS Kernels

## Linux Kernel RV Subsystem (since 5.17, 2022)

Developed by Daniel Bristot de Oliveira. Lives under `kernel/trace/rv/`. Uses deterministic automata (DA) as formal model.

### Architecture

1. **Specification automaton** — finite state machine defining legal event sequences. Undefined transitions = violation.
2. **Instrumentation** — hooks into kernel tracepoints feed events to automaton.
3. **Reactor** — what happens on violation: printk, trace event, or panic().

### Da Specification Format

Monitors specified as Graphviz `.dot` files (states as nodes, events as edge labels). `dot2c` tool converts to C transition table: `unsigned char automaton[NR_STATES][NR_EVENTS]`.

### Built-in Monitors

- **wip** (Wakeup In Preemptive): checks wakeup events occur in valid scheduling states
- **wwnr** (Wakeup While Not Running): wakeup only for non-running tasks
- **snep** (Schedule Not in Exception Path): no voluntary schedule in exception handlers

Per-task monitors (each task gets own automaton instance) and per-CPU monitors.

### Overhead

- Transition lookup: O(1) array index
- No dynamic allocation in hot path
- Per-task state: one small integer in `task_struct`
- **< 1% throughput impact** (0.1-1.8% measured). Automata typically < 10 states, fit in cache line.
- Zero cost when disabled (static keys/jump labels).

## Formal Verification vs Runtime Verification

| Aspect | Formal (seL4) | Runtime (Linux RV) |
|--------|---------------|-------------------|
| When | Pre-deployment | During execution |
| Completeness | All executions | Only observed |
| Cost | 11+ person-years, PhD expertise | Days-weeks, engineer-level |
| Maintenance | Must re-verify on changes | Just add/update monitors |
| False negatives | None (for proved props) | May miss unexercised paths |
| Runtime overhead | Zero | Small (< 1%) |

They complement: formal for core invariants, RV for field behavior.

## RV Properties for a Microkernel

### IPC Protocol Correctness

Per-thread automaton: states = {idle, send_blocked, recv_blocked, call_blocked, replying}. Events = {ipc_send, ipc_recv, ipc_call, ipc_reply, partner_ready, msg_delivered}. Catches: sending while already sending, replying without pending call.

### Scheduler Invariants

- Highest-priority-runs: after every context_switch, verify running thread has highest priority among ready threads
- No priority inversion without mitigation
- Bounded scheduling latency

### Capability Lifecycle

States: {empty, valid, revoked}. Events: {cap_grant, cap_invoke, cap_revoke, cap_delete}. Catches: invoking revoked capability, double-free.

### Memory Mapping Consistency

- No double-map of physical frames (unless explicitly shared)
- Map/unmap pairing
- Permission monotonicity (derived mappings never exceed parent)

## Implementation

```c
static const int transition[NR_STATES][NR_EVENTS] = { ... };

static inline void rv_check(struct thread *t, int event) {
    int next = transition[t->rv_state][event];
    if (next < 0) rv_violation(t, event);
    else t->rv_state = next;
}
```

Per-thread: 1 byte per monitor. 100 threads × 3 monitors = 300 bytes. Transition tables: < 64 bytes each, read-only, cache-friendly.

## Existing Work

- **Copilot** (Galois/NASA): RV framework for embedded C, generates constant-time constant-space monitors from Haskell DSL. Used on unmanned aircraft.
- **CertiKOS** (Yale): certified concurrent OS kernel verified in Coq (static, not RV)
- **Hyperkernel** (UW): symbolic execution for simple x86 kernel (automated static)
- **LOLA**: stream-based spec language for aerospace RV

## Novel Angle: Self-Verifying Microkernel

No existing educational kernel includes built-in RV. A microkernel shipping its own verifier:
- Automata serve as executable specifications — students read state machines before reading implementation
- Bugs manifest as violations with clear diagnostics
- Monitors are living documentation that can't go stale
- Progressive: add monitors as subsystems are built
- Bridges to formal methods without theorem provers
- `.dot` → C pipeline is visual and intuitive
