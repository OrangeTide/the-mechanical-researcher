---
title: Programmable Display Protocols
date: 2026-03-17
abstract: What would a modern programmable display protocol look like? From PLATO and Tektronix terminals through NeWS, Display PostScript, Plan 9, and Wayland, the history of display systems reveals recurring trade-offs between server intelligence and protocol simplicity that inform four candidate architectures for the future.
category: systems design
---

## Introduction

In 1986, James Gosling built a window system around a radical idea: instead of sending drawing commands to a display server, send *programs*. The server would execute them — handling events, animating widgets, tracking mouse drags — all without a single network round-trip. That system was NeWS, and it lost the protocol wars to X11 despite being technically superior in almost every measurable dimension.

Nearly four decades later, we are still living with the consequences of that outcome. Modern display systems have largely converged on the thinnest possible server: Wayland composites pre-rendered buffers; web browsers execute JavaScript in a sandbox walled off from the rendering pipeline; VNC and RDP stream pixels. The "programmable display server" is a road not taken.

But the pressures that motivated NeWS haven't disappeared — they've intensified. Cloud rendering, edge computing, collaborative applications, and the growing gap between network latency and display refresh rates all point back to the same question Gosling was asking: *what if the display server could think?*

This research surveys the historical systems that explored programmable display protocols, distills the features a modern protocol would need, and evaluates four concrete architectural approaches — each making different trade-offs along the axes that have defined this design space since the 1980s.

<img src="lineage.svg" alt="Lineage and influence graph of display protocol technologies from Xerox Alto to the present">

## Abstract

What would a modern programmable display protocol look like? From PLATO and Tektronix terminals through NeWS, Display PostScript, Plan 9, and Wayland, the history of display systems reveals recurring trade-offs between server intelligence and protocol simplicity that inform four candidate architectures for the future.

## The Historical Landscape

The history of display protocols is a history of arguments about where computation should live. Every system ever built for putting graphics on a screen has had to answer the same question: how much intelligence belongs in the display server, and how much in the client?

The answer has shifted back and forth for sixty years. The earliest networked display systems were necessarily "thick" — terminals with their own processors, running programs downloaded from a host. As networks got faster and CPUs cheaper, the balance swung toward thin displays and rich clients. Understanding the arc requires starting earlier than the 1980s protocol wars.

<img src="timeline.svg" alt="Timeline of display systems from PLATO (1964) to modern compositors, showing both networked display protocols and local GUI milestones">

### Before the Protocol Wars: Terminals That Could Think

**PLATO** (Programmed Logic for Automatic Teaching Operations) was perhaps the first system to demonstrate what a programmable display terminal could achieve at scale. Developed at the University of Illinois beginning in 1960, PLATO IV (1972) introduced custom plasma display terminals with 512×512 pixel resolution, built-in character generators, and — remarkably for 1972 — touch-sensitive screens. The terminals were not passive displays: they contained microprocessors that executed display logic locally, including character rendering, animation, and touch input processing. The host communicated with terminals through a proprietary protocol at 1260 baud, sending high-level instructions ("draw this character at these coordinates", "erase this region") rather than raw pixels.

What made PLATO relevant to display protocol history was TUTOR, the programming language that ran on the host side. TUTOR programs controlled the terminal display through structured commands — `at`, `write`, `draw`, `circle`, `arrow` — that the host translated into terminal instructions. Interactive lessons with animated graphics, touch-driven responses, and real-time feedback ran over what was, by modern standards, a vanishingly thin network link. PLATO demonstrated the core principle that NeWS would later articulate: if the terminal is smart enough, you can achieve rich interactivity over a slow connection by pushing behavior to the display side.

The **Tektronix 4014** (1972) took a different approach to networked graphics. Its storage tube display used a phosphor that retained images until explicitly erased — no refresh circuitry, no frame buffer. Drawing commands arrived as escape sequences over a serial connection: move-to, draw-to, point-plot, and various text modes. The lack of selective erase (you could only clear the entire screen) meant the 4014 was poorly suited for interactive manipulation, but it excelled at what it was designed for: displaying complex scientific and engineering graphics generated by remote programs.

The 4014 became the *de facto* standard graphics terminal on ARPANET. Its simple escape-sequence protocol meant that any program capable of writing to a serial port could generate vector graphics — no special libraries required. Plot libraries like DISSPLA, ISSCO TELLAGRAF, and later GNU `gnuplot` all spoke Tektronix 4014 protocol. When bitmap displays arrived, terminal emulators (notably `xterm` with its Tektronix mode) kept the protocol alive. The 4014's legacy is instructive: a protocol simple enough that *any program can speak it* achieves a penetration that more capable but more complex protocols cannot.

Both PLATO and the Tektronix 4014 explored the design space well before NeWS and X11 formalized the arguments. PLATO showed that programmable terminals with local intelligence could deliver rich interactivity over constrained networks. The 4014 showed that a minimal, universally-speakable protocol could become a dominant standard despite severe functional limitations. These are the same trade-offs that would animate the protocol wars of the 1980s — and the same trade-offs we face today.

### NeWS: The Maximalist Answer

NeWS was the most ambitious programmable display system ever deployed. Clients uploaded PostScript programs to the server, where they ran with direct access to the display, input events, and other server-side resources. A button's hover highlight, a scrollbar's drag tracking, a menu's cascade animation — all executed inside the server with zero network traffic.

The design exploited a deep insight about interactive graphics: most UI work is *local*. A slider doesn't need to consult the application on every mouse-move event — it needs to update its visual position, and only notify the application when the user releases it. By running the slider's logic server-side, NeWS reduced a stream of hundreds of mouse-motion events to a single "value changed" message.

NeWS extended PostScript with lightweight processes, monitors for synchronization, a canvas hierarchy for windowing, and an event interest system where server-side procedures fired in response to input patterns. Clients registered *interests* — declarative descriptions of which events to handle — and the server dispatched accordingly, executing PostScript handlers without crossing the network.

The cost was formidable. PostScript is a stack-based language that most application developers found alien. Debugging server-side PostScript was painful — errors in uploaded code could hang the display server or corrupt other clients' state. Security was essentially nonexistent; any client could upload code that interfered with any other client's windows. And Sun controlled the implementation, which made the rest of the Unix vendor ecosystem nervous.

NeWS lost to X11 in what Richard Gabriel might call a textbook case of "worse is better." X11 was simpler, open, and good enough. The X Consortium provided neutral governance. Motif and later GTK/Qt gave developers C-level toolkits that were easier to work with than raw PostScript. By 1993, Sun conceded, shipping CDE on X11 as its primary desktop.

### Display PostScript: The Pragmatic Middle

Adobe's Display PostScript took a less radical approach. Where NeWS made PostScript the programming language for the entire window system, DPS used it only for rendering. Applications communicated through *wraps* — C functions generated by Adobe's `pswrap` tool that, when called, sent pre-compiled PostScript fragments to the server for execution.

The DPS server maintained persistent execution contexts per client, each with its own graphics state — transformation matrices, clipping paths, colors, fonts. Incremental updates were natural: a client sent a small PostScript program to modify a specific region of its window, and the server applied it within the existing context. Binary object sequences reduced wire overhead by roughly 50% compared to ASCII PostScript.

NeXTSTEP was DPS's showcase. The entire window system was built on it, and the result was remarkable for its time: true WYSIWYG across screen and printer, resolution-independent rendering, and a unified imaging model that made Interface Builder's visual design tools possible. Steve Jobs chose DPS specifically because it collapsed the gap between screen and print — a selling point for NeXT's target market in desktop publishing and higher education.

DPS's limitations were practical rather than fundamental. Adobe charged significant licensing fees. The PostScript interpreter was entirely software-rendered, with no path to hardware acceleration. On NeXTSTEP's 25 MHz 68030 hardware, scrolling and window dragging were noticeably sluggish compared to contemporary X11 systems that could blit bitmaps directly.

When Apple acquired NeXT in 1997, DPS's legacy shaped what came next. Quartz 2D preserved the imaging model — paths, affine transforms, compositing, graphics state objects — while replacing PostScript with PDF as the underlying formalism. The `CGContext` API maps almost one-to-one onto PostScript operators (`CGContextMoveToPoint` ≈ `moveto`, `CGContextStrokePath` ≈ `stroke`), but executes as C function calls rather than interpreted code. Apple kept what worked (the imaging model) and discarded what didn't (the interpreter, the licensing, the Turing-completeness in the display server).

### The Blit and Plan 9: Programmability Through Composition

Rob Pike's work at Bell Labs took a different path entirely. The Blit terminal (1982) was a programmable bitmap display with its own 68000 processor. The host Unix system could download small C programs to the terminal, which would then execute locally — handling interactive graphics and text editing without host round-trips. This was NeWS's core insight, arrived at independently and four years earlier. Like PLATO's plasma terminals, the Blit demonstrated that pushing computation to the display side could transform a network-constrained connection into a responsive interactive experience.

Plan 9's window system, 8½ (and later rio), took the idea further by making it *compositional* rather than language-based. Each window appeared as a file system — `/dev/cons`, `/dev/mouse`, `/dev/draw` — and the window manager was an ordinary user process that multiplexed these file system interfaces. Any process could act as a window manager by mounting the appropriate interfaces. The display was programmable not because it ran a special language, but because it exposed standard interfaces that ordinary programs could interpose on.

The draw device (`/dev/draw`) provided a 2D imaging protocol based on Porter-Duff compositing operations. Applications communicated by writing fixed-format binary messages to the device file — each message began with a single-byte opcode followed by binary parameters. The `draw` command (`d`) performed a general compositing operation: source image, mask image, destination image, destination point, source point, mask point — a single operation expressive enough to implement fills, copies, transparency, and masked blits. Line drawing (`l`, `L`), ellipses (`e`), and string rendering (`s`) provided higher-level primitives. Images were allocated server-side (`b` to allocate, `f` to free) and referred to by integer IDs — the same retained-mode pattern that appears in every successful display protocol.

The elegance was in the uniformity. The entire graphics system reduced to compositing rectangles from one image onto another, with an optional mask — the twelve Porter-Duff operators (clear, SoverD, SatopD, SinD, etc.) as the compositing algebra. There were no special-case drawing modes, no modal state, no graphics context objects. A fill was a compositing operation from a solid-color source. A copy was a compositing operation with a full mask. Text rendering composited glyph images from a font cache. This minimalism made the protocol small enough to implement correctly (the draw device is roughly 2000 lines of C) and general enough to handle all 2D rendering needs.

Where X11 required the server to implement dozens of drawing operations (lines, arcs, polygons, fills, stipples, tiles, text in multiple encodings) across multiple visuals and color depths — each with modal graphics context state — Plan 9's draw device offered one operation that could express all of them. The contrast with X11 is instructive: X11's protocol has over 120 request types; Plan 9's draw device has roughly 20 message types, most of which are resource management (allocate image, load font, set clipping rectangle). The actual drawing vocabulary is tiny.

Plan 9's approach remains influential as a design philosophy: programmability emerges from composing simple, orthogonal mechanisms rather than from embedding a powerful language in a privileged server. The file system interface made network transparency a solved problem — `import` a remote machine's `/dev/draw` and you had a remote display, using the same protocol (`9P`) that moved files, with the same authentication and encryption. No separate display protocol was needed.

### An Aside: The MGR Window System

Bellcore's MGR (1984) deserves mention as an overlooked middle path. Where NeWS required clients to write PostScript and Plan 9 exposed file system interfaces, MGR used *escape sequences* — the same mechanism that VT100 terminals used for cursor positioning and text attributes, extended to support bitmap graphics, windows, and menus.

An MGR client was any program that could write to a terminal. Drawing a line meant printing an escape sequence; creating a menu meant printing another. The entire protocol ran over pseudo-terminals, which meant standard Unix tools — `cat`, pipes, `ssh` — worked as transport without modification. A shell script could create windows and draw graphics. The barrier to entry was essentially zero.

MGR ran on Sun workstations with as little as 1 MB of RAM, serving multiple concurrent clients with bitmap graphics, stacking windows, and mouse input. It supported network transparency through the simple expedient of running over TCP — no special protocol needed, because the protocol *was* the terminal data stream.

The lesson from MGR is that protocol simplicity and ecosystem integration can matter more than expressive power. MGR could never match NeWS's rendering sophistication, but it could be adopted incrementally by any program that could print to stdout. This principle — that the protocol should meet developers where they are — informs the design of the proof-of-concept that follows.

### The Modern Landscape

Today's display systems have largely migrated to the opposite end of the spectrum from NeWS.

#### Wayland: The Triumph of Buffer Passing

Wayland represents the logical endpoint of a trend that began with X11's DRI (Direct Rendering Infrastructure) and Xgl: if clients are already rendering everything themselves using OpenGL or Vulkan, why maintain a server-side drawing protocol at all? Kristian Høgsberg's answer, when he started Wayland in 2008, was that you shouldn't. The Wayland protocol is deliberately minimal: clients render into buffers (shared memory, GBM, or DMA-BUF backed), attach them to a `wl_surface`, report which regions changed (`damage`), and `commit`. The compositor takes the buffer, composites it with other clients' buffers, and presents the result. There is no `DrawLine`, no `FillRectangle`, no `RenderString` — no drawing protocol whatsoever.

The argument for this model is compelling in the local case. Modern GUI toolkits — GTK, Qt, Flutter, Chromium — already perform all rendering client-side using GPU-accelerated pipelines. X11's server-side drawing operations had become vestigial; the Xrender extension and Cairo's client-side rendering made them unnecessary for everything except legacy applications. By eliminating the drawing protocol entirely, Wayland removes an entire class of server complexity, reduces the attack surface, and lets clients use whatever rendering technology they choose. The compositor's job shrinks to what a compositor actually needs to do: combine surfaces, manage input focus, and apply whole-surface transforms.

But the buffer-passing model makes an implicit trade-off that becomes explicit in several scenarios:

**Network transparency.** Wayland has none. The protocol assumes shared memory or DMA-BUF — mechanisms that require the client and compositor to share a physical address space or GPU. Remote display over Wayland requires external solutions (RDP, VNC, PipeWire screen capture) that stream pixels, losing all structural information. This is a deliberate design choice, not an oversight — the Wayland developers argue that network transparency is better handled at a higher layer — but it means Wayland cannot serve the use case that motivated X11, NeWS, and the Tektronix 4014: a remote application displaying on a local screen.

**Resolution independence.** When a client renders to a buffer, it commits to a specific pixel resolution. Wayland's `wp_fractional_scale` and `wp_viewport` protocols mitigate this by letting the compositor scale buffers and by informing clients of the desired scale factor, but the fundamental issue remains: the compositor receives pixels, not geometry. It cannot re-render a client's output at a different resolution without the client's cooperation. A vector-based protocol — even a simple one — would let the display server re-render at any resolution without client involvement.

**Accessibility.** An opaque buffer reveals nothing about its contents. Screen readers cannot inspect a Wayland surface to discover that it contains a button, a menu, or a text field — they must rely on a separate accessibility protocol (AT-SPI over D-Bus) that the client maintains independently. This creates a parallel data path: the visual representation goes through Wayland as pixels, while the semantic representation goes through AT-SPI as structured data. A display protocol that transmits structured scene descriptions would unify these paths, making the scene graph and the accessibility tree aspects of the same data.

**Bandwidth.** A full-screen 4K buffer at 32 bits per pixel is roughly 33 MB per frame. Damage tracking reduces this — only changed regions are transmitted — but even a modestly interactive application (text editor, spreadsheet) may damage a significant fraction of its surface on each keystroke as it re-renders text, updates scrollbars, and repaints selection highlights. A structured protocol that says "change this text node's content to X" transmits bytes, not megapixels.

The contrast with a vector-based display protocol is not as stark as it initially appears, however. Both models must ultimately produce pixels on a display; the question is where the rasterization happens and what crosses the interface boundary. Wayland says: rasterize client-side, pass pixels. A vector protocol says: pass geometry, rasterize server-side. The web platform shows that these aren't mutually exclusive — HTML Canvas and WebGL are buffer-passing within a system that also supports structured SVG and DOM. A modern display protocol might similarly support both: structured scene graph operations for UI, with an escape hatch to buffer-passing for content that doesn't fit the vector model (video, 3D viewports, custom rendering).

#### The Web, Scenic, and Arcan

The web platform occupies a fascinating middle position. The browser is, in effect, a display server that interprets a rich declarative language (HTML/CSS/SVG) and executes uploaded programs (JavaScript). CSS Houdini's paint worklets are strikingly close to NeWS's model: executable code uploaded to the rendering engine, running in a restricted context with direct access to drawing primitives. WebGPU compute shaders extend this further, allowing client-uploaded programs to execute on the GPU itself.

Google's Fuchsia operating system introduced Scenic, a scene-graph-based compositor where clients submit structured scene graph fragments rather than rendered buffers or drawing commands. Clients interact through FIDL (Fuchsia Interface Definition Language) — a typed IPC protocol with operations like `CreateResource`, `SetTranslation`, `SetShape`, `SetMaterial`, and `AddChild`. The compositor retains a tree of nodes — shapes, transforms, materials, clips — and performs rendering itself using Escher, a Vulkan-based renderer. This is neither NeWS's "upload programs" model nor Wayland's "upload pixels" model, but a middle ground: upload *structured data* that the server renders. Scenic evolved from a full 3D scene graph (GFX) to a simplified 2D-focused API (Flatland), suggesting that even purpose-built scene graph protocols benefit from simplification under real-world pressure.

Arcan is perhaps the most ambitious modern attempt at a programmable display server, directly invoking the spirit of NeWS. Its compositor logic is written entirely in Lua — window management policies, visual effects, input routing, and custom UI elements are all scripts running inside the display server. Arcan's A12 network protocol provides built-in network transparency with adaptive encoding: structured commands for UI elements, video codec for media content, with per-frame compression selection based on content type. Combined with its shmif client protocol (which supports structured content types, bidirectional communication, and state serialization), Arcan demonstrates that a modern programmable display server is technically viable — the question is whether it can achieve the ecosystem breadth that NeWS could not.

Figma's multiplayer architecture provides a production-scale data point from a different angle. Figma maintains its own scene graph in WASM memory (a C++ renderer compiled to WebAssembly) and synchronizes it across clients using operational transforms streamed over WebSocket in a compact binary format. The client renders to Canvas using a tile-based pipeline with incremental dirty-region updates. Figma is not a display protocol in the traditional sense, but it demonstrates that structured scene graph synchronization over WebSocket can work at scale with high interactivity — exactly the core mechanism a modern display protocol would need.

On the rendering side, the Vello project (from the Google Fonts team) has demonstrated that GPU compute shaders can perform high-performance 2D vector rendering — processing path segments in parallel, binning them into tiles, and computing pixel coverage entirely on the GPU. Compiled to WASM with a WebGPU target, Vello makes the browser a viable platform for rendering complex vector scenes at frame rates far beyond what SVG's DOM-based pipeline can achieve. This matters for a display protocol because it decouples the question of *what the protocol carries* (structured scene data) from *how it renders* (potentially a GPU-accelerated WASM engine rather than the browser's native SVG renderer).

<img src="spectrum.svg" alt="Architectural spectrum from thick server (NeWS) to thin server (VNC), showing where each system falls">

## Design Requirements

Drawing from the historical record, we can identify a set of capabilities that a modern programmable display protocol should provide. These are not features to be implemented all at once, but rather a minimum viable set that addresses the failure modes of past systems while preserving their strengths.

**Resolution-independent vector imaging.** The PostScript/PDF/SVG imaging model — paths, bezier curves, affine transforms, gradient fills, compositing — has proven durable across four decades. Any modern protocol should speak this language natively. Bitmap-only protocols (VNC) and buffer-only protocols (Wayland) discard semantic information that the server could exploit for caching, scaling, and bandwidth optimization.

**Incremental, streaming updates.** DPS's strongest feature was its ability to send small PostScript fragments that modified an existing graphics state. A modern protocol must support fine-grained mutations — inserting a node, changing an attribute, removing a subtree — without retransmitting the entire scene. This is what makes the protocol viable over constrained networks.

**Server-side state retention.** The display server must maintain a persistent representation of the scene between updates. This enables the server to re-render at different resolutions, apply compositor effects (shadows, transparency, animations), and avoid redundant data transfer. Plan 9's draw device, Fuchsia's Scenic, and every browser's DOM all demonstrate the value of retained-mode graphics.

**Declarative animation and transition.** NeWS showed that interactive behaviors can execute server-side to eliminate round-trips. But uploading Turing-complete programs raises security and complexity concerns. A modern protocol should support *declarative* animations and transitions — descriptions of motion that the server executes autonomously. SVG's SMIL animations and CSS transitions are existence proofs that this works.

**Sandboxed server-side computation.** This is the NeWS feature that no successor has satisfactorily replaced. Some class of computation *must* run server-side for latency-critical interactions — hit testing, constraint resolution, scroll physics, gesture recognition. The key is sandboxing: the uploaded code must be memory-safe, time-bounded, and incapable of accessing other clients' state. WebAssembly provides the execution model; capability-based security provides the access model.

**Structured event flow.** NeWS's event interest system was elegant: clients declared patterns describing which events to receive, and the server dispatched accordingly. A modern protocol should support event filtering, coalescing, and server-side preprocessing — reducing raw input streams to semantic events before they cross the wire.

**Accessibility as a first-class concern.** No historical display protocol addressed accessibility adequately. A modern protocol that transmits structured scene descriptions (rather than pixels) has a natural advantage: the scene graph *is* the accessibility tree, or can be mapped to one without screen-scraping heuristics.

## Candidate Architectures

With these requirements established, we can evaluate concrete architectures. Each approach makes different trade-offs along the fundamental axes: server complexity vs. network efficiency, security vs. programmability, and adoption feasibility vs. architectural purity.

<img src="approaches.svg" alt="Four architectural approaches: SVG Streaming, Scene Graph Protocol, Reactive Dataflow, and WASM Programmable Compositor">

### Approach A: SVG Streaming Protocol

The most conservative approach builds directly on SVG, the web's existing vector graphics standard. An SVG streaming protocol would define a wire format for incremental mutations to an SVG document — insert element, set attribute, remove element, reorder children — transmitted over a persistent connection.

The client maintains the authoritative application state and generates SVG mutations. The server (a browser, a dedicated renderer, or a compositor) maintains a retained SVG DOM and applies mutations as they arrive. SMIL animations handle declarative motion server-side. CSS custom properties provide a thin parameterization channel — the client can update a handful of numeric values and let CSS rules propagate the visual consequences.

**Strengths.** Adoption is the overwhelming advantage. SVG is a W3C standard implemented in every browser. The tooling ecosystem — editors, debuggers, accessibility tools — already exists. The gap between "SVG streaming protocol" and "what browsers already do" is small enough that a polyfill could demonstrate the concept. Accessibility comes nearly for free: SVG elements carry ARIA roles and the DOM is already exposed to screen readers.

**Weaknesses.** SVG's DOM is not designed for high-frequency updates. Each mutation triggers style recalculation, layout, and repaint through the browser's rendering pipeline — a pipeline optimized for document rendering, not interactive graphics. At thousands of mutations per second, this becomes a bottleneck. SVG also lacks a mechanism for server-side computation: there is no way to upload a hit-testing function or a scroll-physics routine. The protocol would be purely declarative, with all logic remaining client-side.

The wire format presents a design choice. XML-based diffs are human-readable but verbose. A binary encoding (similar to DPS's binary object sequences) would be more efficient but would sacrifice SVG's inspectability. A hybrid approach — binary transport with XML-equivalent semantics — is possible but adds complexity.

**Best suited for:** applications where visual fidelity and accessibility matter more than interactive performance — dashboards, data visualizations, document rendering, collaborative whiteboards with moderate update rates.

### Approach B: Scene Graph Protocol

This approach defines a custom binary protocol for submitting and mutating a retained scene graph on the compositor. The scene graph consists of typed nodes — shapes, groups, transforms, clips, materials, text runs — organized in a tree. Clients submit subtrees and subsequent mutations; the compositor retains the tree and renders it.

This is closest to what Fuchsia's Scenic does. The protocol is structured: each operation targets a node by ID and specifies a typed mutation (set transform, change fill color, append child). The compositor owns the rendering pipeline and can optimize aggressively — batching draw calls, caching rasterized subtrees, applying resolution-appropriate level-of-detail.

**Strengths.** A purpose-built binary protocol can be extremely bandwidth-efficient. Node-level granularity allows the compositor to cache and invalidate precisely. The typed node model enables the compositor to understand the *structure* of the scene, not just its pixel output — enabling semantic zoom, compositor-driven animation, and principled accessibility tree generation. GPU acceleration is straightforward because the compositor controls the rendering pipeline end-to-end.

**Weaknesses.** A new protocol means a new ecosystem — renderers, debuggers, profilers, accessibility bridges — all must be built from scratch. This is the cost that killed Fresco/Berlin in the 1990s and has slowed Scenic's adoption outside Fuchsia. The protocol must be versioned carefully; adding new node types or mutation operations is a compatibility event. And like Approach A, there is no mechanism for server-side computation beyond what the compositor natively supports.

Interoperability with the web is a challenge. A scene graph protocol could be bridged to SVG or Canvas, but the impedance mismatch would lose much of the performance advantage. Applications targeting this protocol would need dedicated client libraries rather than leveraging existing web APIs.

**Best suited for:** operating system-level display servers where the protocol can be mandated (as in Fuchsia), embedded systems with constrained bandwidth, and remote display scenarios where structured scene data dramatically outperforms pixel streaming.

### Approach C: Reactive Dataflow Protocol

Rather than sending imperative mutations, this approach has the client declare *relationships* — reactive bindings and constraints that the server maintains autonomously. A slider's thumb position is bound to a numeric value; the value is constrained to a range; the track fill width is derived from the value. The client sends the constraint graph; the server solves it continuously.

This draws inspiration from constraint-based systems like TeX's box-and-glue model, Apple's Auto Layout (based on the Cassowary constraint solver), and reactive programming frameworks. The protocol transmits a dataflow graph: nodes are values, edges are derivation functions. The display server evaluates the graph, renders the visual result, and re-evaluates when inputs change — whether from client updates or from user input processed server-side.

**Strengths.** Reactive dataflow naturally handles the interactions that are most painful over a network: dragging, scrolling, resizing, animation. The server maintains the constraint graph and can resolve user input locally without round-trips. The declarative nature makes the protocol inspectable and debuggable — the constraint graph is a meaningful artifact, not an opaque stream of mutations. Animations are implicit: changing a constraint's input triggers re-evaluation, and the server can interpolate between states.

**Weaknesses.** Constraint solving is computationally expensive. General constraint systems can have exponential worst-case behavior; even Cassowary (linear arithmetic constraints) adds meaningful overhead per constraint. A display server running constraint solvers for hundreds of clients must either limit expressiveness or accept variable frame rates. The protocol is also unfamiliar — there is no large developer community experienced with constraint-based UI beyond Auto Layout, and even Auto Layout is widely regarded as difficult to debug.

The expressive boundary is the critical design challenge. Which constraints can the server solve? Linear arithmetic? Affine transforms? Arbitrary functions? Each extension makes the server more powerful but harder to implement, harder to sandbox, and harder to reason about. A reactive dataflow protocol risks becoming either too limited to be useful or too complex to be tractable.

**Best suited for:** form-heavy applications with structured layouts, responsive design across heterogeneous displays, and scenarios where the server must adapt the UI to local conditions (screen size, accessibility settings, user preferences) without client involvement.

### Approach D: WASM Programmable Compositor

This is the direct descendant of NeWS: clients upload executable code to the display server, where it runs with access to drawing primitives and input events. The critical difference from NeWS is the execution environment. Where NeWS used PostScript (Turing-complete, no memory safety, no isolation), this approach uses WebAssembly — a sandboxed, memory-safe, time-boundable bytecode format designed for exactly this kind of untrusted-code execution.

A client submits a WASM module along with a capability manifest declaring what resources it needs: a drawing surface of specified dimensions, input events of specified types, a timer, access to specific shared state. The compositor validates the manifest against security policy, instantiates the module in a sandbox, and begins dispatching events to it. The module renders into its allocated surface using a drawing API exposed through WASM imports. The compositor composites all clients' surfaces into the final display.

**Strengths.** This is the only approach that fully addresses server-side computation. Hit testing, scroll physics, gesture recognition, text input handling, and custom animation curves can all execute server-side at native speed. WASM's sandbox provides memory safety and can be time-bounded (fuel metering) to prevent runaway modules from starving other clients. The capability model provides principled security — a module can only access resources explicitly granted by its manifest. And WASM is language-agnostic: clients can write their rendering code in Rust, C, Go, or any language with a WASM target.

**Weaknesses.** Complexity is the dominant concern. The compositor becomes a WASM runtime, a capability manager, a resource allocator, and a rendering engine — a substantial surface area for bugs and security vulnerabilities. The WASM module itself is opaque to the compositor: unlike a scene graph or SVG DOM, the compositor cannot inspect the module's visual output for accessibility, cannot apply compositor-level effects (shadows, transparency) to sub-elements, and cannot rerender at a different resolution without the module's cooperation.

Debugging is harder than in any other approach. When a visual glitch appears, is it in the client's application logic, the WASM module's rendering code, the compositor's surface management, or the WASM runtime itself? Each layer has its own tooling and failure modes.

The "opaque surface" problem deserves emphasis. NeWS's great advantage over X11 was that the server *understood* what was being drawn — it could apply server-side logic to the scene's structure. A WASM module that renders to a bitmap surface loses this advantage. The mitigation is a hybrid model: the module submits structured scene graph data *and* receives a drawing API for custom rendering within specific nodes. This is essentially how browsers work today (DOM for structure, Canvas for custom rendering), but formalized as a protocol.

**Best suited for:** latency-critical interactive applications (games, creative tools, collaborative editors), scenarios where server-side computation provides a measurable UX improvement, and systems where the compositor's trust boundary is well-defined.

## Trade-off Analysis

The four approaches occupy different positions in the design space, and no single approach dominates across all dimensions.

| | SVG Streaming | Scene Graph | Reactive Dataflow | WASM Compositor |
|---|---|---|---|---|
| **Network efficiency** | Moderate (XML overhead) | High (binary, structured) | High (only changed inputs) | Variable (depends on module) |
| **Server-side computation** | None | Limited (built-in only) | Constraint solving | Full (sandboxed WASM) |
| **Accessibility** | Excellent (DOM-native) | Good (structured tree) | Good (constraint graph) | Poor (opaque surfaces) |
| **Security model** | Trivial (no code exec) | Trivial (no code exec) | Moderate (constraint scope) | Complex (WASM sandbox + capabilities) |
| **Adoption path** | Easy (extend existing web) | Hard (new ecosystem) | Medium (novel but declarative) | Hard (new runtime) |
| **Interactive latency** | High (client round-trip) | High (client round-trip) | Low (server-side solving) | Lowest (server-side code) |
| **Debugging** | Easy (browser DevTools) | Moderate (new tooling) | Hard (constraint debugging) | Hardest (multi-layer) |
| **GPU acceleration** | Limited (browser pipeline) | Natural (compositor-owned) | Possible (solver output) | Natural (module or compositor) |

The most revealing axis is the tension between **interactive latency** and **accessibility**. Approaches that move computation server-side (C, D) reduce latency but make the display server's output harder to inspect semantically. Approaches that keep the display server purely declarative (A, B) are inherently inspectable but cannot eliminate client round-trips for interactive behaviors.

This tension echoes the original NeWS vs. X11 trade-off, but modern technology offers partial resolutions that weren't available in the 1980s. WASM sandboxing addresses NeWS's security problems. Capability-based access control addresses its isolation problems. And a hybrid approach — structured scene graph *plus* sandboxed computation within designated nodes — could address the accessibility gap by ensuring that the overall scene structure remains inspectable even when individual elements are rendered by opaque code.

## A Pragmatic Synthesis

The historical record suggests that architectural purity is less important than ecosystem fit. NeWS was technically superior to X11 but lost because X11 was simpler, open, and had more vendors behind it. DPS was elegant but lost to bitmap-based rendering because hardware got fast enough to make brute force viable. Plan 9's compositional approach was the most principled of all but never reached critical mass outside Bell Labs.

A modern programmable display protocol that aims for adoption should probably be a **layered architecture** rather than a monolithic one:

**Layer 1 — Structured scene graph.** A retained tree of typed nodes (shapes, text, groups, clips, transforms) with a binary wire protocol for incremental mutations. This provides the baseline: resolution-independent rendering, compositor-level effects, accessibility, and efficient bandwidth usage. This layer can function standalone as a display protocol — equivalent to Approach B.

**Layer 2 — Declarative behaviors.** Animations, transitions, constraints, and event filters that execute server-side without uploaded code. This layer reduces round-trips for common interactive patterns (scrolling, resizing, hover effects, layout reflow) while remaining inspectable and safe. Built atop the scene graph as annotations on nodes.

**Layer 3 — Sandboxed computation.** WASM modules that execute within designated scene graph nodes, receiving drawing APIs and input events through capabilities. This layer is optional — clients that don't need server-side computation never touch it. Clients that do get NeWS-class latency reduction within a modern security model.

This layering has a direct historical parallel. It's how the web platform evolved: HTML provides the scene graph (Layer 1), CSS provides declarative behaviors (Layer 2), and JavaScript provides programmable computation (Layer 3). The web's success suggests that this layered approach — where each layer is independently useful and adoption is incremental — is more viable than any monolithic design.

The key difference from the web platform would be that all three layers are designed as a *protocol* from the start, rather than evolving organically from a document format. The scene graph is binary and structured, not XML. The declarative behaviors are defined in terms of scene graph operations, not text styling. And the sandboxed computation layer is integrated with the compositor's rendering pipeline, not walled off in a separate VM.

## Proof of Concept: Quaoar

The preceding analysis is necessarily abstract — four architectures evaluated against seven requirements across eight dimensions. To ground it, consider a concrete application of these ideas: an **SVG-based thin client** running in a web browser, serving the same role that X terminals and NeWS terminals once served.

### The Model

The architecture mirrors X11's `DISPLAY` model, but with modern materials:

A user opens a browser tab pointing at a remote host. The browser loads a small JavaScript (or WASM) client — the **display server** — which opens a WebSocket connection back to the host. Remote applications on that host connect to the display server through a local socket or environment variable (analogous to `$DISPLAY`), and send scene graph operations over the WebSocket. The browser-side display server maintains a retained SVG DOM, applies mutations as they arrive, and routes user input events back to the applications.

The key insight, borrowed directly from NeWS: **widget libraries on the application side should push behavior to the display server**. When an application creates a button, the widget library doesn't just send "draw a rectangle with text" — it sends a self-contained declaration of the button's visual states (idle, hover, pressed, focused, disabled), its transitions between states, and its event bindings. The display server handles hover highlighting, press animation, and focus indication locally, only sending a "clicked" event back to the application when the user completes the interaction.

This is what NeWS did with PostScript programs uploaded to the display server. The difference is in the mechanism: instead of uploading Turing-complete PostScript, the widget library sends declarative descriptions — SVG elements with CSS state rules, SMIL or Web Animations API transitions, and event filter specifications. The display server executes these using the browser's native capabilities, which are already optimized for exactly this kind of work.

### What Travels Over the Wire

The protocol would define a small set of operations, encoded in a compact binary format:

**Scene graph mutations:** insert element (with full attribute set), remove element, set attribute, reorder children, set CSS custom property. Each element gets a protocol-level ID for subsequent reference. Subtrees can be sent as pre-composed fragments — a widget library would define a button template once, then instantiate it by reference with parameter bindings.

**Behavior declarations:** CSS rules scoped to a subtree, animation definitions (keyframes, timing, triggers), event interest registrations (which events to capture, whether to coalesce, whether to handle locally or forward to the application). A scroll container, for example, would declare scroll physics, overflow behavior, and scroll-position-to-transform bindings — the display server handles smooth scrolling locally, only notifying the application when it needs to load more content.

**Resource management:** font declarations, image data (or URLs for images the display server should fetch), gradient and pattern definitions. These are defined once and referenced by ID, avoiding redundant transmission.

**Events flowing back:** semantic events (button clicked, text entered, scroll position changed, selection made), not raw input. The display server coalesces mouse-move events, handles keyboard repeat, and resolves hit testing locally. The application receives high-level events with the relevant context attached.

### The Widget Library

On the application side, a widget library provides the programming interface. An application developer writes code like:

```
window = display.window("My Application", width=800, height=600)
toolbar = window.toolbar()
toolbar.button("Save", icon="disk", on_click=save_handler)
toolbar.button("Undo", icon="arrow-left", on_click=undo_handler)

canvas = window.canvas()
canvas.on_draw(render_document)
```

The widget library translates this into protocol messages. `toolbar.button()` generates an SVG subtree (rectangle, icon, label), CSS rules for hover/active/focus states, animation definitions for press feedback, and an event interest for click events. The entire button — visuals, behavior, and interaction — crosses the wire as a single declarative bundle.

For custom drawing (the `canvas.on_draw` case), the widget library provides a drawing API that generates SVG elements — paths, shapes, text, groups with transforms. The application builds its scene using this API, and the library batches the resulting SVG mutations into efficient protocol messages. Incremental updates after the initial draw send only the changed elements.

### What This Gains Over Pixel Streaming

The advantage over VNC or RDP-style pixel streaming is significant for the class of applications this targets — productivity tools, development environments, dashboards, administrative interfaces:

**Bandwidth.** A button widget is perhaps 200 bytes of protocol data. Its pixel representation at 2x DPI is thousands of bytes even with aggressive compression. An application with 50 widgets on screen might require 10 KB of protocol data for the initial layout, versus hundreds of KB for a compressed framebuffer. Subsequent updates are even more efficient — changing a label is a single set-attribute operation.

**Latency perception.** Because hover effects, press animations, focus indicators, scroll physics, and text cursor blinking all execute locally in the browser, the UI *feels* local even over a high-latency connection. The user sees immediate feedback for their interactions; only the semantic result ("user clicked Save") needs to round-trip to the server. This is exactly the advantage NeWS demonstrated over X11 in the 1980s.

**Resolution independence.** The SVG scene scales perfectly to any display DPI. A user on a 4K display and a user on a 1080p display receive the same protocol data; the browser handles rendering at the appropriate resolution. No server-side awareness of client display characteristics is needed.

**Accessibility.** The SVG DOM in the browser is exposed to screen readers, supports keyboard navigation, and respects user preferences for reduced motion, high contrast, and font size. The application developer gets accessibility largely for free from the widget library's semantic SVG output.

**Searchability.** Text in the remote application is real text in the browser's DOM. The user can Ctrl+F to search, select and copy text, and use browser translation tools — none of which work with pixel-streamed remote desktops.

### What This Loses

The model has genuine limitations:

**Arbitrary rendering.** Applications that need pixel-level control — image editors, 3D viewports, video players — cannot be served purely through SVG. The protocol would need an escape hatch: a "surface" element where the application streams compressed image data (H.264, WebP) for content that doesn't fit the vector model. This is the same hybrid approach that RDP and SPICE use — structured commands for UI chrome, pixel streaming for media content.

**Performance ceiling.** SVG DOM performance in browsers tops out at roughly 5,000–10,000 elements before style recalculation and layout become bottlenecks. A complex application (a large spreadsheet, a densely populated IDE) might exceed this. Mitigations include viewport-aware culling (only elements in view are in the DOM), element recycling for scrolling lists, and canvas-rendered regions for dense data.

**Application ecosystem.** This model requires applications to be written against the widget library, or for existing GUI toolkits to gain a new backend that speaks the protocol. This is the same adoption challenge that faced NeWS (requiring PostScript UI code), Broadway (requiring GTK), and every other remote display system. The most practical path is to target an existing toolkit — a Qt or GTK backend that outputs protocol messages rather than rendering locally — so that existing applications can participate without modification.

### Historical Precedent

This scenario is not as novel as it might appear. GTK's Broadway backend already does something similar — it renders GTK applications to HTML5 Canvas commands streamed over WebSocket, allowing any GTK application to display in a browser. Broadway proves the model works but uses Canvas (immediate-mode, pixel-oriented) rather than SVG (retained-mode, structured). An SVG-based approach would inherit Broadway's demonstrated viability while gaining the bandwidth, accessibility, and resolution-independence advantages of structured vector graphics.

NoMachine's NX protocol took a related approach for X11 — proxying and compressing X11's structured drawing commands rather than streaming pixels. NX achieved order-of-magnitude bandwidth reductions over raw X11 forwarding for typical desktop applications, demonstrating the value of preserving semantic structure in the wire protocol.

The Arcan display server's A12 network protocol is perhaps the most forward-looking precedent. A12 supports adaptive encoding — structured commands for UI elements, video codec for media content — with built-in encryption and content-type awareness. Combined with Arcan's Lua-scriptable compositor, A12 demonstrates that a modern network display protocol can be both structured and adaptive.

What none of these precedents attempted is the NeWS-style behavior offloading — pushing interactive widget logic to the display server. That is the distinctive feature of the SVG thin client model, and it's enabled by a coincidence of modern capabilities: browsers already implement a rich, optimized rendering engine with declarative animation, event handling, and accessibility support. The display protocol merely needs to expose these capabilities to remote applications in a structured way.

### Quaoar: A Proof of Concept

To test whether the SVG thin client model is viable beyond paper analysis, we built a minimal working prototype called **Quaoar** (named after the trans-Neptunian object). The system consists of four components:

**quaoar-server** — a C daemon (~300 lines) that bridges WebSocket and Unix socket connections. It accepts one WebSocket connection from a browser and multiplexes Unix socket connections from applications. On the Unix side, messages use djb netstring framing (`length:data,`) — simple, self-delimiting, and trivial to implement in C. On the WebSocket side, the built-in framing handles delimitation. The server performs the HTTP upgrade handshake itself (including a minimal inline SHA-1 for the `Sec-WebSocket-Accept` computation), making it proxy-agnostic — it works standalone for development and behind nginx, caddy, haproxy, or even stunnel for production TLS termination.

**libquaoar** — a C client library (~385 lines) that connects to the Unix socket and provides a widget API. An application creates windows and widgets with simple function calls, registers event callbacks, and runs an event loop. The library handles netstring framing, message formatting, and event dispatch. Widget convenience functions (buttons, labels, scrollbars) emit complete SVG markup with SMIL animations for hover and press states — the display server receives self-contained visual declarations rather than abstract widget types. The API:

```c
qu_ctx *ctx = qu_connect(NULL); /* reads QUAOAR_DISPLAY env */

int win = qu_window(ctx, "Notepad", 80, 60, 480, 340);
int btn = qu_button(ctx, win, "Save", 10, 10, 70, 28);
int ta  = qu_textarea(ctx, win, 10, 48, 460, 272);

qu_on_event(ctx, btn, on_save, NULL);
qu_on_event(ctx, ta,  on_text, NULL);

struct pollfd pfd = { .fd = qu_fd(ctx), .events = POLLIN };
while (poll(&pfd, 1, -1) >= 0)
    if (qu_process(ctx) < 0) break;
```

**quaoar-client.html** — the browser-based display server (~320 lines of JavaScript). It connects via WebSocket, maintains an SVG scene, and acts as a generic SVG insertion engine: it parses incoming SVG markup (via `DOMParser`) and inserts it into the document. Because the C library emits complete SVG+SMIL declarations, the client needs no widget-specific rendering code — button hover effects, scrollbar drag behavior, and press animations all execute through the browser's native SVG/SMIL engine with zero network round-trips. Only semantic events ("button clicked", "text changed", "window closed") are sent back to applications. A status overlay shows connection state and provides a hamburger menu for connect/disconnect.

**notepad** — a sample application (~50 lines of C) that creates a window with Save and Load buttons and a text area. It demonstrates the complete round-trip: the widget library sends scene graph operations to the browser, the browser handles all interactive behavior locally, and the application only sees high-level events like "save button clicked" or "text content changed."

The full source is in the `demo/` directory. Build with `make`, run `quaoar-server`, open `quaoar-client.html` in a browser, and launch `notepad`. Deployment behind a reverse proxy is documented in the [README](demo/README.md).

The protocol uses a header+payload format: the first line is space-delimited metadata, and everything after the newline is raw payload (SVG markup, text content, etc.). This avoids the double-escaping problem that plagues JSON-wrapped SVG — the SVG passes through untouched:

```
window 1 80 60 480 340
Notepad
```

```
svg 2 1
<g transform="translate(10,10)" style="cursor:pointer">
  <rect width="70" height="28" rx="4" fill="#5a5a62">
    <set attributeName="fill" to="#6a6a72"
         begin="mouseover" end="mouseout"/>
    <set attributeName="fill" to="#4a4a52"
         begin="mousedown" end="mouseup"/>
  </rect>
  <text x="35" y="18" fill="#eee" font-size="12"
        text-anchor="middle">Save</text>
</g>
```

```
listen 2 click
```

The button's hover highlight and press depression are SMIL `<set>` animations — they execute in the browser's SVG engine with no JavaScript and no network traffic. The display client doesn't know what a "button" is; it just inserts SVG and wires up event listeners as instructed.

Events flowing back are equally simple:

```
1 event 2 click
1 event 3 text
user typed this
```

The server prepends the application ID (first token) so the display client can route events to the correct Unix socket connection. Payloads (like text content) appear after the newline.

The prototype validates the core thesis: a browser-based display server receiving structured SVG scene operations over WebSocket can provide a responsive interactive experience for remote applications, with behavior offloading that eliminates round-trips for common interactions. At ~1000 lines of C and ~320 lines of JavaScript — with no external dependencies — the implementation cost is modest.

## Conclusion

The dream of a programmable display server — computation living where the pixels are — has resurfaced in fragments across the modern landscape. CSS Houdini paint worklets upload rendering code to the browser. WebGPU compute shaders execute client-authored programs on the GPU. Fuchsia's Scenic retains a server-side scene graph. Arcan scripts its compositor in Lua. Each is a partial answer to the question NeWS asked in 1986.

What emerges from this survey is that the question was never really "should the display server be programmable?" — every successful display system has been programmable to some degree. The question is *how much* programmability, *what kind*, and *with what safety guarantees*. NeWS answered "total programmability, through a general-purpose language, with no safety guarantees" — and lost. The web answered "total programmability, through a sandboxed language, with significant safety guarantees" — and won, but not as a display protocol; as something much larger and stranger.

A modern programmable display protocol would thread the needle: structured scene data for inspectability and accessibility, declarative behaviors for common interactions, sandboxed computation for everything else. The pieces exist. The imaging model is SVG/PDF's, proven across four decades. The scene graph structure is Scenic's, designed for compositors. The declarative behaviors are CSS's, battle-tested in billions of documents. The sandboxed computation is WASM's, designed for untrusted code. What's missing is not technology but *assembly* — the work of combining these pieces into a coherent protocol with a single wire format, a single security model, and a single compositor architecture.

Whether that assembly happens depends less on technical merit than on the same forces that shaped the protocol wars of the 1980s: openness, governance, ecosystem breadth, and the willingness of enough vendors to commit to a shared standard. NeWS teaches us that being right is not enough. X11 teaches us that being simple and open can compensate for being wrong. The web teaches us that layered, incrementally-adoptable architectures outlast monolithic designs. A modern programmable display protocol should learn from all three.
