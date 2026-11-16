# Retained-Mode GUI Demo (FreePascal + SDL2)

A runnable demo of the toolkit described in the article. It draws a scrolling
city map as the live background, with a multi-page inventory window and an
equipment paperdoll floating on top, both in the System 7 visual idiom. You can
drag items between inventory slots, between bag pages, and on and off the
paperdoll. The paperdoll slots only accept the matching item category.

## What you see

- A scrolling tiled city map (the "game" behind the windows).
- An **Inventory** window with four bag tabs and a grid of item slots, filled
  with a few draggable items.
- An **Equipment** window with a paperdoll figure and category-locked slots
  (helmet on the head, weapon in the main hand, and so on).
- An **Items** window: a Finder-style scrolling pane of freely arranged icons.
  Drag an icon anywhere, scroll with the wheel or scrollbar, and right-click or
  control-click an icon for an Open / Duplicate / Delete context menu.
- Drag an item: press on a filled slot, move, and release over another slot. A
  slot refuses an item of the wrong category, and a drop over nothing returns
  the item to where it started.

## Prerequisites

- **Free Pascal Compiler** (`fpc`), 3.2 or newer.
- **SDL2 runtime and development files.**
  - Debian or Ubuntu: `sudo apt install fpc libsdl2-dev`
  - Fedora: `sudo dnf install fpc SDL2-devel`
  - macOS (Homebrew): `brew install fpc sdl2`

The demo vendors its own minimal SDL2 binding (`sdl2min.pas`), so no FPC SDL2
package is required. It links against the system `libSDL2`.

## Build

```sh
make
```

This compiles `demo` from the five source units. To build by hand:

```sh
fpc -O2 -Mobjfpc demo.pas
```

## Run

```sh
./demo
```

Controls:

- **Left mouse**: drag a window by its title bar, close it with the close box
  (top-left), and drag items between slots.
- **Zoom box** (top-right): maximize the window, click again to restore.
- **Grow box** (bottom-right corner): drag to resize the window.
- **Right-click or control-click an icon** (Items window): open its context menu.
- **Mouse wheel or scrollbar** (Items window): scroll the icon pane.
- **Left / Right arrow**: scroll the city map behind the windows.
- **Esc**: quit.

The zoom and grow boxes are optional per window. The Inventory window enables
both; the Equipment window enables neither, so a fixed utility window does not
have to carry resize or maximize chrome. This is controlled by the window flag
set (`wfZoom`, `wfGrow`) passed at construction.

The map also auto-scrolls so the background is visibly alive under the windows.

## Headless screenshot

To render a single frame to a BMP file without opening a window (useful on a
machine with no display):

```sh
make shot          # writes demo.bmp
# or:
./demo --shot demo.bmp
```

## Source layout

| File | Role |
|---|---|
| `sdl2min.pas` | Minimal vendored SDL2 binding (only the calls the demo uses). |
| `gui.pas` | The toolkit core: widget tree, desktop, window, slots, drag-and-drop, tabs, scroll view, context menu. Backend-agnostic. |
| `guisdl.pas` | SDL2 backend: a concrete `TCanvas`, the System 7 theme, and an embedded bitmap font. |
| `rpgext.pas` | The RPG extensions: `TInventoryWindow` and `TPaperdoll`. |
| `iconview.pas` | The Finder-style extension: `TIconView` and its draggable icons. |
| `demo.pas` | The toy game: scrolling city view, scene setup, and the main loop. |

The core (`gui.pas`) has no SDL dependency. Porting to another backend means
writing one new `TCanvas` subclass, as `guisdl.pas` does for SDL2.

## Notes

The embedded font is a small 5x7 bitmap stand-in, drawn uppercase. A real game
would supply a proper bitmap font through the same `TCanvas.DrawText` seam. Item
"art" is a colored tile with a letter, standing in for real icon bitmaps.
