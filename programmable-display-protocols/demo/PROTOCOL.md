# Quaoar Protocol

## Framing

Unix socket side: djb netstrings (`length:data,`).

WebSocket side: standard WebSocket text frames. The server translates
between the two.

## Message format

Each message has a header line (space-delimited fields) and an optional
payload after a newline:

    op field1 field2 ...
    payload (if any)

## App ID routing

The server prepends the app ID to messages going to the browser:

    3 window 1 80 60 480 340
    Notepad

The browser prepends the app ID to events going back:

    3 event 2 click

Negative IDs are system channels:

    -1 hello /tmp/quaoar-0       (server → browser)
    -1 disconnect 3              (server → browser)
    -1 event 0 paste             (browser → server, broadcast to all)
    clipboard text to copy       (any app → browser)

## Messages: application → display

**window** `id x y w h\ntitle`
Create a window with titlebar and close button.

**svg** `id parent\n<svg markup>`
Insert arbitrary SVG into a parent widget's content area. The markup
is parsed by DOMParser and inserted into the document. SMIL animations
in the markup execute natively.

**textarea** `id parent x y w h`
Create a multi-line text input (HTML `<textarea>` in a `<foreignObject>`).

**listen** `id event_type`
Register a DOM event listener on a widget. The event type is any DOM
event name (`click`, `mouseover`, `focusout`, etc.).

**update** `id key\nvalue`
Update a widget's content. For text elements, sets the `textContent` of
the first `[data-qu-text]` descendant (or the element itself if it's a
`<text>`). For textareas, sets the `value`.

**remove** `id`
Remove a widget from the display.

**clipboard** `\ntext`
Write text to the browser clipboard.

## Messages: display → application

**event** `id event_type\nvalue`
An event occurred on a widget. Event types:

| Event    | Value                          | Trigger                        |
|----------|--------------------------------|--------------------------------|
| `click`  | (none)                         | user clicked the element       |
| `text`   | textarea content               | text changed (300ms debounce)  |
| `scroll` | `0.0` to `1.0`                 | scrollbar thumb released       |
| `xy`     | `x y` (each `0.0` to `1.0`)   | 2D drag handle released        |
| `close`  | (none)                         | window close button clicked    |
| `paste`  | clipboard text                 | user pasted (Ctrl+V)           |

Paste events use widget ID 0 and are broadcast to all apps.

## Interaction attributes

The display client recognizes `data-qu-*` attributes on SVG elements
for client-side interactive behavior:

**`data-qu-drag="h|v"` `data-qu-track="N"`** — Constrained 1D drag.
The element's `x` or `y` attribute is updated during drag, clamped to
`[0, N]`. On release, a `scroll` event fires with the normalized
position.

**`data-qu-xy`** — 2D position tracking. The element (typically a
`<circle>`) is dragged within its parent's bounding box. On release,
an `xy` event fires with normalized coordinates. Set
`data-qu-xy-area` on a sibling to override the bounding area.

**`data-qu-text`** — Marks an SVG element as the text target for
`update` messages. The element's `textContent` is replaced.

## Adding new interaction classes

To support a new interaction (rotation, resize, rubber-banding):

1. Choose a `data-qu-<name>` attribute
2. Add a mousedown check in the client's mousedown handler
3. Track state in mousemove
4. Send an event on mouseup
5. Applications emit SVG with the attribute — no per-widget changes
