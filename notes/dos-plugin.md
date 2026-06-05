# Loadable plugins / swappable drivers under Watcom C (16-bit real-mode DOS)

Question: can we have loadable plugins (overlay-style) whose entry points
share the same name, so that calling `gfx_flip` dispatches to whichever driver
(VGA vs EGA) is currently loaded?

Short answer: **not as a linker feature, but yes as a loader feature.** You
can't make `gfx_flip` a single link symbol that resolves to whichever external
file is loaded. DOS real mode has no dynamic linker and no cross-module symbol
back-patching, and the linker forbids duplicate symbols anyway. What you *can*
do is load an external module at runtime and dispatch to it through a pointer
table, which is exactly the `struct gfxdrv gfx` mechanism this project already
uses, just with the struct populated from a file instead of from a built-in
`gfx_*_init()`.

## Why "same name" can't be a link symbol

At link time every public symbol must be unique. If `g_vga.obj` and `g_ega.obj`
both export `gfx_flip`, `wlink` errors on the duplicate. And there is no late
binding: a real-mode `.EXE` has its intra-segment calls resolved/relocated when
DOS loads it. A separately-built file cannot satisfy a call to a host symbol,
and the host cannot hold an unresolved `gfx_flip` waiting to be filled in by a
plugin. So "calling `gfx_flip` transparently goes to the loaded driver" only
works if `gfx_flip` is an indirect call through a pointer we control, not a
direct symbol. That is the whole point of the existing driver struct:
`gfx.flip()` is an indirect call and `gfx_init(want)` decides what `gfx.flip`
points at. Runtime dispatch is already solved; the only open question is whether
the implementation can live in a separately loadable file.

## Watcom overlays: what they are, what they are not

`wlink` does support 16-bit DOS overlays (`FORMAT DOS`, overlay classes). But
overlays are:

- link-time, all built into one `.EXE`, not separate shippable files;
- unique-symbol, so still no two `gfx_flip`s;
- transparent swapping, paging code sections in/out of a shared region via
  thunks to fit a big program in little memory.

Overlays solve "my code is bigger than RAM," not "load a third-party VGA driver
from a separate file." They do not give same-named pluggable entry points.

## The actual DOS mechanism for loadable modules

If we genuinely want a driver shipped as a separate file and loaded at runtime,
the legitimate facility is:

**INT 21h, AH=4Bh, AL=03h ("Load Overlay").** DOS reads an `.EXE`-format file
into a segment we allocate, applies its relocations relative to that segment,
but does NOT create a PSP or start execution. We then far-call into it. This is
the canonical way to load callable code into our own address space.

Plumbing to build around it:

1. **Known entry point.** The module cannot be located by symbol name. Usual
   conventions: (a) the first bytes are a jump table / a single known entry at
   offset 0, or (b) a fixed "init" entry that returns a far pointer to the
   module's dispatch table.
2. **No back-linking to host symbols.** The module cannot call our `pak_read`,
   `dbg_trace`, etc. by name. We pass it a services struct of host function
   pointers at init; it calls back through that. In return it hands us its
   `struct gfxdrv`. We do `gfx = *plugin_table;` and the rest of the engine is
   unchanged.
3. **Everything is far.** The module lives in another segment, so all entry
   points and callbacks must be `__far` regardless of the host's memory model.
   Under the default small model, a loadable backend's driver struct needs far
   function pointers, and the plugin must be compiled with a compatible
   convention.
4. **Model/ABI discipline.** Host and plugin must agree on struct layout and
   calling convention. Easiest: compile both with the same Watcom flags and
   share one header defining the services struct and the driver struct.

(A custom flat `.COM`-style loader, a position-independent blob with a jump
table at offset 0, is the other route, but getting Watcom C to emit
relocation-free code is painful. 4B03h is easier because DOS does the
relocation fixups and the module is just an ordinary `.EXE` built with `wlink`.)

Either way we converge back on the function-pointer struct: the "shared name"
is the struct member `gfx.flip`, and the loader's only job is to fill that
struct from a file.

## Practical take for this project

For VGA/EGA/CGA160 with three in-tree backends, the loadable-plugin machinery
buys almost nothing and costs real complexity: a custom loader, the far-ABI
services struct, model/convention lockstep between two builds, harder
debugging. The existing design (all drivers compiled in, `gfx_init(want)` picks
one, `gfx.flip()` dispatches) already gives "same call goes to the right
driver" with none of that risk.

Loadable drivers earn their keep only for **third-party / after-ship drivers**
dropped in as separate files without rebuilding the EXE, or when memory is too
tight to keep all backends resident. If/when we want that, the design is: keep
`struct gfxdrv` as-is but make its function pointers `__far`, define a
`struct hostsvc` of callbacks, load `VGADRV.EXE` via 4B03h, call its init with
`&hostsvc`, copy the returned `gfxdrv` into the global `gfx`. The engine above
the driver layer does not change.
