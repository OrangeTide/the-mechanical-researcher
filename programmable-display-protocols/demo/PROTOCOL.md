# Quaoar Wire Protocol

## Architecture

Messages flow between applications and the browser through the Quaoar server, which bridges two transports:

| Layer | Component | Transport | Framing |
|-------|-----------|-----------|---------|
| Application | `libquaoar` | Unix domain socket | djb netstrings (`length:data,`) |
| Bridge | `quaoar-server` | — | translates between formats |
| Display | browser client | WebSocket | standard text frames |

## Message Format

Every message consists of a header line with space-delimited fields, optionally followed by a newline and a payload:

    op field1 field2 ...
    payload (if any)

The message looks different on each side of the server. Applications send raw commands over the Unix socket. The server prepends an integer app ID for multiplexing before forwarding to the browser over WebSocket:

**Unix socket (application side):**

| `op` | `field1 field2 ...` | `\npayload` |
|------|---------------------|-------------|
| command name | space-delimited arguments | optional, after newline |

**WebSocket (browser side):**

| `appid` | `op` | `field1 field2 ...` | `\npayload` |
|---------|------|---------------------|-------------|
| integer, always first | command name | space-delimited arguments | optional, after newline |
| assigned by server | same as Unix side | same as Unix side | same as Unix side |

**Example — creating a window (app 3):**

| Side | Wire Content |
|------|-------------|
| App sends on Unix socket | `window 1 80 60 480 340\nNotepad` |
| Browser receives on WebSocket | `3 window 1 80 60 480 340\nNotepad` |

**Example — click event routed back:**

| Side | Wire Content |
|------|-------------|
| Browser sends on WebSocket | `3 event 2 click` |
| App receives on Unix socket | `event 2 click` |

Applications never see app IDs — the server strips them from incoming browser events and adds them to outgoing application messages.

## System Messages

Negative app IDs are reserved for system-level messages that are not routed to any application:

| Message | Direction | Purpose |
|---------|-----------|---------|
| `-1 hello <path>` | server to browser | Initialization, reports socket path |
| `-1 disconnect <id>` | server to browser | Application disconnected |
| `-1 event 0 paste` | browser to server | Broadcast event to all applications |

## Commands: Application To Display

**window** `id x y w h\ntitle`
Create a window with a titlebar and close button. The payload is the window title.

**svg** `id parent\n<markup>`
Insert arbitrary SVG into a parent widget's content area. The markup is parsed by `DOMParser` and appended to the document. SMIL animations in the markup execute natively in the browser, requiring no server round-trips.

**textarea** `id parent x y w h`
Create a multi-line text input rendered as an HTML `<textarea>` inside a `<foreignObject>`.

**listen** `id event_type`
Register a DOM event listener on a widget. Accepts any DOM event name (e.g. `click`, `mouseover`, `focusout`).

**update** `id key\nvalue`
Update a widget's content. The `key` field determines what is updated:

- `text` — For textareas, sets the `value` property. For SVG elements, sets `textContent` on the first `[data-qu-text]` descendant (or the element itself if it is a `<text>` node).
- Any other key — Sets the named attribute on the first `[data-qu-<key>]` descendant, or on the widget element itself if no such descendant exists.

**remove** `id`
Remove a widget and all its children from the display.

**clipboard** `\ntext`
Write text to the browser clipboard via the Clipboard API.

## Events: Display To Application

**event** `id event_type\nvalue`

An event fired on a widget. The following event types are defined:

| Event | Value | Trigger |
|-------|-------|---------|
| `click` | (none) | User clicked the element |
| `text` | textarea content | Text changed (300 ms debounce) |
| `scroll` | `0.0` to `1.0` | Scrollbar thumb released |
| `xy` | `x y` (each `0.0` to `1.0`) | 2D drag handle released |
| `close` | (none) | Window close button clicked |
| `paste` | clipboard text | User pasted (Ctrl+V) |

Paste events use widget ID 0 and are broadcast to all connected applications.

## Interaction Attributes

The display client recognizes `data-qu-*` attributes on SVG elements to provide client-side interactive behavior without server involvement:

**`data-qu-drag="h|v"` with `data-qu-track="N"`** — Constrained one-dimensional drag. The element's `x` or `y` attribute is updated during the drag, clamped to the range `[0, N]`. When the user releases, a `scroll` event fires with the normalized position.

**`data-qu-xy`** — Two-dimensional position tracking. The element (typically a `<circle>`) is dragged within its parent's bounding box. On release, an `xy` event fires with normalized coordinates. Place `data-qu-xy-area` on a sibling element to override the bounding area.

**`data-qu-text`** — Marks an SVG element as the text target for `update` messages. The element's `textContent` is replaced with the payload.

## Adding New Interaction Types

To support a new interaction pattern (rotation, resize, rubber-banding, etc.):

1. Choose a `data-qu-<name>` attribute.
2. Add a `mousedown` check in the client's pointer handler.
3. Track state during `mousemove`.
4. Send an event on `mouseup`.
5. Applications emit SVG with the new attribute — no protocol changes required.
