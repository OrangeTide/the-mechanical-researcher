# Rust/Leptos vs Node: Architectural Differences

## Compilation Model

- Node frameworks (React, Svelte, etc.) ship JavaScript to the browser.
  SSR is a JS process rendering HTML strings.
- Leptos compiles to **native code for the server** and **WASM for the
  client** from the same codebase. The server binary is a real Axum/Actix
  app — no V8 runtime, no event loop overhead.

## Reactivity

- React uses a virtual DOM diffing model. Svelte compiles away the vDOM
  but still generates JS.
- Leptos uses **fine-grained reactivity** (signals, derived signals,
  effects) — closer to SolidJS. When a signal changes, only the specific
  DOM node that depends on it updates. No vDOM, no diffing, no component
  re-renders.

## Type Safety

- Node stacks rely on TypeScript (optional, erasable). Server/client
  boundaries are loosely typed (tRPC helps but is bolted on).
- Leptos `#[server]` functions are **type-checked across the boundary at
  compile time**. If your server function returns `Vec<Post>`, the client
  call is statically verified to expect that type. Serialization is
  derived, not hand-written.

## Memory and Concurrency

- Node is single-threaded with async I/O. Scaling means multiple
  processes or worker threads.
- The Leptos server is a multi-threaded Rust binary. No GC pauses,
  predictable latency, lower memory footprint under load.

## Hydration

- Most JS frameworks hydrate by re-executing components on the client and
  reconciling with server HTML.
- Leptos supports **partial hydration** and is moving toward **islands
  architecture** — only interactive parts ship WASM. Static content stays
  as plain HTML with zero client cost.

## The `view!` Macro

The `view!` macro takes RSX-like syntax and expands it at compile time
into direct DOM construction calls (or HTML string building on the
server). It is not a template engine at runtime — it is a compiler pass.

### Benefits

- **Zero runtime overhead** — the macro expands to exactly the DOM
  operations needed. No template parsing, no vDOM allocation, no diffing.
- **Compile-time validation** — mismatched tags, wrong attribute types,
  passing a `String` where a `Signal<String>` is expected — all caught
  before the program runs.
- **Fine-grained updates baked in** — the macro knows which parts are
  static and which depend on signals. Static parts are created once;
  reactive parts get surgical `.set_text_content()` or
  `.set_attribute()` calls wired to specific signals.

### Problems

- **Compiler errors are rough** — when the macro fails, you get errors
  pointing at expanded code, not your source. Rust's proc-macro error
  reporting is improving but still worse than a TypeScript error in JSX.
- **IDE support is weaker** — rust-analyzer can struggle with
  macro-heavy code. Autocomplete inside `view!` is hit-or-miss. Syntax
  highlighting sometimes breaks.
- **Learning curve** — it looks like JSX but the rules are different.
  Closures for reactive values (`move || signal.get()`) feel foreign to
  someone coming from React. The distinction between static values and
  reactive closures is a constant source of beginner bugs.
- **Compile times** — proc macros add to Rust's already slow
  compilation. Large `view!` blocks expand to significant generated code.
  Splitting into small components helps but adds boilerplate.

### Net Assessment

The macro is a genuine architectural win — it enables Leptos to match or
beat JS framework performance while keeping a component-based DX. The
problems are real but they are **tooling problems** (IDE, error messages,
compile speed), not fundamental design flaws. They will improve as Rust's
macro ecosystem matures. If you are already committed to Rust on the
server, the `view!` macro is a reasonable trade for type-safe,
zero-overhead reactive UI. If you are not already in the Rust ecosystem,
the tooling friction is a legitimate reason to stay with a JS framework
where the DX is more polished.
