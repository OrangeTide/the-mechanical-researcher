# Pascal Game GUI v2: one core, four languages

A retained-mode GUI toolkit written in FreePascal, compiled to a shared library
(`libgui.so`) with a flat C ABI, and driven from four languages: Pascal
natively, and C, Lua, and Python through the ABI.

## Layout

```
core/     the Pascal toolkit: gui.pas (widget tree), guitheme.pas (System 7),
          nestheme.pas (NES). Untouched by the ABI.
canvas/   guisdl.pas + sdl2min.pas: the SDL2 canvas and its minimal FFI.
abi/      guiabi.pas (the cdecl exports -> libgui.so) and gui.h (the C header).
ext/      richwidgets.pas (paperdoll + paged inventory, built on the core) and
          demo_native.pas (a native Pascal host, no FFI).
bindings/ c/      example_c.c      the C binding and demo
          lua/    gui_lua.c, demo.lua   the Lua C module and demo
          python/ gui.py, demo.py       the pure-ctypes binding and demo
```

## Prerequisites

- **FreePascal** (`fpc`), 3.2 or newer, to build the library and native demo.
- **libsdl2-dev**, for the built-in canvas and window.
- **A C compiler** (`cc`), for the C example and the Lua module.
- **A Lua 5.x dev package** (any of `lua5.4`, `lua5.3`, `lua5.2`, `lua5.1`), for
  the Lua C module. Optional; the build skips the module if none is found.
- **Python 3**, for the ctypes binding. No build step and no packages needed.

The four bindings are independent. The library and its C, Lua, and Pascal use
need no Python at all; the Python binding is one optional `ctypes` file you can
ignore or delete. Likewise the Lua module is built only if a Lua runtime is
found. `make` and `make check` skip any binding whose runtime is absent.

## Build

```sh
make            # libgui.so, the native Pascal demo, and the C and Lua bindings
make lib        # just the shared library
make clean
```

`libgui.so` is written to this directory. The C and Lua binaries carry an rpath
back to it, so they run in place.

## Run

Each binding builds the same scene and takes the same three modes.

```sh
# native Pascal (uses the core directly, no FFI)
./demo_native                       # render one frame to /tmp (no interactive loop)
./demo_native --shot s7.bmp         # System 7 theme, headless, to BMP
./demo_native --shot-nes nes.bmp    # NES theme, headless, to BMP
./demo_native --selftest            # drag-and-drop on the paperdoll, headless

# C
./bindings/c/example_c              # open a window (needs a display)
./bindings/c/example_c --shot c.bmp
./bindings/c/example_c --selftest

# Lua
cd bindings/lua && lua demo.lua            # needs a display
cd bindings/lua && lua demo.lua --shot l.bmp
cd bindings/lua && lua demo.lua --selftest

# Python (no build step)
cd bindings/python && python3 demo.py            # needs a display
cd bindings/python && python3 demo.py --shot p.bmp
cd bindings/python && python3 demo.py --selftest
```

For the windowed modes without a display, set `SDL_VIDEODRIVER=dummy`. The
`--shot` and `--selftest` modes are fully headless and need no display.

## Verify everything

```sh
make check      # runs the Pascal, Python, and Lua self-tests
```

Each self-test builds the scene, drags an item from one slot to another across
windows with synthetic mouse events, drains the event queue for the drop, checks
that the item's tag moved, then destroys the desktop and confirms a now-stale
handle is rejected cleanly rather than crashing. A passing run prints:

```
drop event delivered: true
item moved src->weapon (tag 101): true
stale handle rejected cleanly: true
SELFTEST PASS
```

## The ABI in one paragraph

Every object is an opaque 64-bit handle made of a table slot and a generation
counter, so a stale or forged handle fails the generation check instead of
dereferencing freed memory. Strings cross as `PChar`, geometry as integers, and
every exported function wraps its body in `try/except` so no Pascal exception can
unwind into the caller. Events are pulled from a per-desktop queue rather than
pushed through callbacks. Foreign code composes the built-in widgets and, for
anything the core does not know, supplies one custom-draw callback that paints
through the same canvas the toolkit uses. The full surface is in `abi/gui.h`.
