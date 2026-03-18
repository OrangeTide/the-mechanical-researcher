# XView and OPEN LOOK

## OPEN LOOK GUI Design

OPEN LOOK was a graphical user interface specification developed by
AT&T and Sun Microsystems in the late 1980s as a competitor to OSF/Motif.
It defined a visual language and interaction model for Unix workstations.

### Visual Language

- **Obround buttons** — rounded-rectangle (oval-end) shapes, distinct
  from Motif's beveled rectangles
- **Pushpins** — popup windows have a pushpin icon; clicking the pin
  keeps the window visible after the action completes, unpinning
  allows auto-dismiss
- **Triangle glyphs** — small triangles on buttons indicate that
  clicking will produce a pull-down or pull-right submenu
- **Abbreviated menu buttons** — compact button variant for
  space-constrained areas
- **Clean, uncluttered aesthetic** — focus on content over chrome,
  minimal decoration compared to Motif

### Color and 3D Effects

- OLGX library calculates three shading colors (highlight, shadow,
  background) from a single base color per control area
- Foreground and background colors controllable per widget
- 3D appearance derived from computed shading, not explicit bevel
  textures

### Interaction Model

- Three-button mouse assumed (SELECT, ADJUST, MENU)
- MENU button (right-click) produces context menus anywhere
- ADJUST (middle-click) extends/modifies selections
- Consistent keyboard traversal and accelerators
- Drag-and-drop between applications supported at the toolkit level

## XView Widget Catalog

XView (X Window-System-based Visual/Integrated Environment for
Workstations) was the primary C toolkit implementing OPEN LOOK on X11.

### Top-Level Windows

- **BASE_FRAME** — main application window on the root window
- **COMMAND_FRAME** — dialog/property sheet frame with default panel
- **POPUP_WINDOW** — transient window with pushpin support

### Containers

- **PANEL** — control area that manages panel items (buttons, fields,
  etc.)
- **SCROLLABLE_PANEL** — scrolling panel, subclassed from CANVAS
- **CANVAS** — drawing surface for custom graphics; supports split
  views and direct Xlib calls
- **OPENWIN** — base class for subwindows

### Panel Items (Controls)

Basic:
- **PANEL_BUTTON** — action button with callback
- **PANEL_MESSAGE** — read-only text label
- **PANEL_TEXT** — single-line text input

Choice/Selection:
- **PANEL_CHOICE** — dropdown with selected option displayed
- **PANEL_CHOICE_STACK** — stacked choice presentation
- **PANEL_CHECK_BOX** — checkbox
- **PANEL_TOGGLE** — toggle button
- **PANEL_CYCLE** — cycle through discrete values on click
- **PANEL_ABBREV_MENU_BUTTON** — abbreviated menu button

Numeric/Analog:
- **PANEL_SLIDER** — draggable grip on a track with min/max
- **PANEL_GAUGE** — read-only numeric indicator (thermometer style)
- **PANEL_NUMERIC_TEXT** — numeric-only text input

Complex:
- **PANEL_LIST** — scrolling list with text and glyphs
- **PANEL_MULTILINE_TEXT** — multi-line editable text with scrollbar
- **PANEL_DROP_TARGET** — drag-and-drop target area

### Text and Terminal

- **TEXTSW** — full text editor subwindow with integrated scrollbars,
  multiple views, undo, search, cut/copy/paste
- **TTYSW** — terminal emulator connected to a shell process, with
  cursor control and blinking

### Menus

- **MENU** — popup menu container
- **MENU_ITEM** — individual menu entry
- **PULLRIGHT** — submenu with hover-triggered expansion

### Dialogs

- **NOTICE** — modal popup dialog for alerts and confirmations

### Scrolling

- **SCROLLBAR** — standalone scrollbar for canvases and custom windows

### Inter-Client Communication

- **SELECTION** — clipboard/selection service (PRIMARY, CLIPBOARD);
  follows ICCCM conventions
- **DND** — drag-and-drop between applications with source/target
  protocol and drag preview feedback

### Specialized

- **FILE_CHOOSER** — file selection dialog (added in XView 3.2)
- **COLOR_CHOOSER** — color picker
- **FONT** — font selection and management

## XView API Style

All objects are created with `xv_create()` and configured with
attribute-value lists:

```c
frame = xv_create(NULL, FRAME,
    FRAME_LABEL, "My Application",
    XV_WIDTH,    400,
    XV_HEIGHT,   300,
    NULL);

panel = xv_create(frame, PANEL, NULL);

button = xv_create(panel, PANEL_BUTTON,
    PANEL_LABEL_STRING,  "Save",
    PANEL_NOTIFY_PROC,   save_callback,
    NULL);
```

Attributes are queried with `xv_get()` and modified with `xv_set()`.
The NULL-terminated attribute list pattern is consistent across all
object types.

## Quaoar Compatibility Assessment

### Easy (already supported or straightforward)

PANEL_BUTTON, PANEL_MESSAGE, PANEL_SLIDER, PANEL_GAUGE,
PANEL_CHECK_BOX, PANEL_TOGGLE, PANEL_CYCLE, SCROLLBAR, MENU_ITEM,
NOTICE, BASE_FRAME, PANEL, PANEL_NUMERIC_TEXT — these map directly
to SVG+SMIL declarations or simple event handling.

### Moderate (need small additions)

- **PANEL_TEXT** — single-line `<input>` via `<foreignObject>`
- **PANEL_CHOICE** — dropdown via show/hide of a `<g>` group
- **POPUP_WINDOW** — pushpin is a visual toggle + focusout listener
- **MENU / PULLRIGHT** — cascading submenus need hover timing beyond
  what SMIL provides
- **FILE_CHOOSER** — UI is windows+lists+text, but filesystem browsing
  is application logic
- **COLOR_CHOOSER** — `<input type="color">` via `<foreignObject>`, or
  a custom SVG picker using `data-qu-xy`

### Difficult (architectural challenges)

- **TEXTSW** — full editor with cursor positioning, selection sync,
  multi-view, undo; requires either a JS editor component or enormous
  event traffic for server-side rendering of each keystroke
- **TTYSW** — terminal emulation needs ANSI escape interpretation and
  PTY connection; would require embedding xterm.js or similar
- **CANVAS** — arbitrary Xlib-style drawing; pixel-level ops (XOR
  drawing, readback) have no SVG equivalent
- **DND** — inter-app drag-and-drop needs server-side brokering
  between app sockets
- **SELECTION** — inter-app clipboard needs ownership negotiation
  protocol (Quaoar uses browser clipboard as single mediator instead)
- **Rubber-band selection** — needs continuous mouse tracking; at 60 Hz
  this is ~120 messages/sec, tolerable on localhost but problematic
  over WAN
