"""
gui.py : a pure-ctypes binding to libgui.so, the Pascal retained-mode GUI.

No build step: this file loads the shared library and declares the C ABI from
gui.h. Handles are plain 64-bit integers; a stale or forged handle is rejected
by the library's generation check, so a mistake here returns an error rather
than crashing the process.
"""

import ctypes
import os

# theme ids
THEME_SYSTEM7 = 0
THEME_NES = 1

# window flags
WF_CLOSE = 1
WF_DRAG = 2
WF_ZOOM = 4
WF_GROW = 8

# event kinds
EV_NONE = 0
EV_BUTTON = 1
EV_TAB = 2
EV_DROP = 3
EV_POPUP = 4
EV_KEY = 5

# mouse dispatch kinds
M_DOWN = 0
M_UP = 1
M_MOVE = 2


class Event(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_int32),
        ("widget", ctypes.c_uint64),
        ("i0", ctypes.c_int32),
        ("i1", ctypes.c_int32),
    ]


DRAW_FN = ctypes.CFUNCTYPE(
    None, ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32,
    ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p)


def _load(path=None):
    if path is None:
        here = os.path.dirname(os.path.abspath(__file__))
        # the Makefile builds libgui.so at the demo root
        for cand in (
            os.path.join(here, "libgui.so"),
            os.path.join(here, "..", "..", "libgui.so"),
            "libgui.so",
        ):
            if os.path.exists(cand):
                path = cand
                break
        else:
            path = "libgui.so"
    lib = ctypes.CDLL(path)

    H = ctypes.c_uint64
    I = ctypes.c_int32
    U = ctypes.c_uint32
    S = ctypes.c_char_p
    P = ctypes.c_void_p

    def sig(name, restype, *argtypes):
        fn = getattr(lib, name)
        fn.restype = restype
        fn.argtypes = list(argtypes)

    sig("gui_version", I)
    sig("gui_desktop_create", H, I, I, I)
    sig("gui_desktop_destroy", I, H)
    sig("gui_desktop_set_theme", I, H, I)
    sig("gui_desktop_theme", I, H)
    sig("gui_window_create", H, H, S, I, I, I, I, I)
    sig("gui_window_set_title", I, H, S)
    sig("gui_window_get_title", I, H, S, I)
    sig("gui_window_content", H, H)
    sig("gui_window_is_visible", I, H)
    sig("gui_window_set_visible", I, H, I)
    sig("gui_widget_abs_rect", I, H, ctypes.POINTER(I), ctypes.POINTER(I),
        ctypes.POINTER(I), ctypes.POINTER(I))
    sig("gui_label_create", H, H, S, I, I)
    sig("gui_button_create", H, H, S, I, I, I, I)
    sig("gui_button_set_tag", I, H, I)
    sig("gui_slot_create", H, H, I, I, I, I)
    sig("gui_slot_set_item", I, H, S, I, U, I, I)
    sig("gui_slot_clear", I, H)
    sig("gui_slot_item_tag", I, H)
    sig("gui_grid_create", H, H, I, I, I, I, I, I)
    sig("gui_grid_slot_at", H, H, I, I)
    sig("gui_tabs_create", H, H, I, I, I, I)
    sig("gui_tabs_add", I, H, S)
    sig("gui_tabs_active", I, H)
    sig("gui_scrollview_create", H, H, I, I, I, I)
    sig("gui_scrollview_set_content_size", I, H, I)
    sig("gui_custom_create", H, H, I, I, I, I, DRAW_FN, P)
    sig("gui_canvas_fill_rect", I, P, I, I, I, I, U)
    sig("gui_canvas_frame_rect", I, P, I, I, I, I, U)
    sig("gui_canvas_text", I, P, I, I, S, U)
    sig("gui_canvas_text_width", I, P, S)
    sig("gui_canvas_text_height", I, P)
    sig("gui_dispatch_mouse", I, H, I, I, I, I, I)
    sig("gui_dispatch_wheel", I, H, I, I, I)
    sig("gui_poll_event", I, H, ctypes.POINTER(Event))
    sig("gui_run_frame", I, H)
    sig("gui_render_bmp", I, H, S)
    return lib


lib = _load()


def _b(s):
    return s.encode("utf-8") if isinstance(s, str) else s


class Desktop:
    def __init__(self, theme=THEME_SYSTEM7, w=800, h=600):
        self.h = lib.gui_desktop_create(theme, w, h)
        if self.h == 0:
            raise RuntimeError("gui_desktop_create failed")
        self._keepalive = []   # hold DRAW_FN trampolines so they are not GC'd

    def destroy(self):
        if self.h:
            lib.gui_desktop_destroy(self.h)
            self.h = 0

    def window(self, title, flags=WF_CLOSE | WF_DRAG, l=0, t=0, w=200, h=150):
        return Window(self, lib.gui_window_create(
            self.h, _b(title), flags, l, t, w, h))

    @property
    def theme(self):
        return lib.gui_desktop_theme(self.h)

    @theme.setter
    def theme(self, theme_id):
        lib.gui_desktop_set_theme(self.h, theme_id)

    def poll(self):
        ev = Event()
        if lib.gui_poll_event(self.h, ctypes.byref(ev)) == 1:
            return ev
        return None

    def click_at(self, x, y, button=0, ctrl=0):
        lib.gui_dispatch_mouse(self.h, M_DOWN, x, y, button, ctrl)
        lib.gui_dispatch_mouse(self.h, M_UP, x, y, button, ctrl)

    def drag(self, x0, y0, x1, y1):
        lib.gui_dispatch_mouse(self.h, M_DOWN, x0, y0, 0, 0)
        lib.gui_dispatch_mouse(self.h, M_MOVE, (x0 + x1) // 2, (y0 + y1) // 2, 0, 0)
        lib.gui_dispatch_mouse(self.h, M_MOVE, x1, y1, 0, 0)
        lib.gui_dispatch_mouse(self.h, M_UP, x1, y1, 0, 0)

    def render_bmp(self, path):
        return lib.gui_render_bmp(self.h, _b(path))

    def run_frame(self):
        return lib.gui_run_frame(self.h)


class Window:
    def __init__(self, desk, handle):
        self.desk = desk
        self.h = handle
        if handle == 0:
            raise RuntimeError("gui_window_create failed")

    @property
    def title(self):
        buf = ctypes.create_string_buffer(256)
        lib.gui_window_get_title(self.h, buf, 256)
        return buf.value.decode("utf-8")

    @title.setter
    def title(self, s):
        lib.gui_window_set_title(self.h, _b(s))

    @property
    def visible(self):
        return lib.gui_window_is_visible(self.h) == 1

    def content(self):
        return Widget(self.desk, lib.gui_window_content(self.h))


class Widget:
    def __init__(self, desk, handle):
        self.desk = desk
        self.h = handle

    def abs_rect(self):
        l, t, r, b = (ctypes.c_int32() for _ in range(4))
        lib.gui_widget_abs_rect(self.h, ctypes.byref(l), ctypes.byref(t),
                                ctypes.byref(r), ctypes.byref(b))
        return (l.value, t.value, r.value, b.value)

    def center(self):
        l, t, r, b = self.abs_rect()
        return ((l + r) // 2, (t + b) // 2)

    def label(self, caption, l, t):
        return Widget(self.desk, lib.gui_label_create(self.h, _b(caption), l, t))

    def button(self, caption, l, t, w, h, tag=0):
        b = lib.gui_button_create(self.h, _b(caption), l, t, w, h)
        if tag:
            lib.gui_button_set_tag(b, tag)
        return Widget(self.desk, b)

    def slot(self, l, t, sz=40, accept=0):
        return Slot(self.desk, lib.gui_slot_create(self.h, accept, l, t, sz))

    def grid(self, cols, rows, accept=0, pitch=44, l=0, t=0):
        return Grid(self.desk, lib.gui_grid_create(
            self.h, cols, rows, accept, pitch, l, t))

    def tabs(self, l, t, tabw=64, h=22):
        return Tabs(self.desk, lib.gui_tabs_create(self.h, l, t, tabw, h))

    def scrollview(self, l, t, w, h):
        return Widget(self.desk, lib.gui_scrollview_create(self.h, l, t, w, h))

    def custom(self, l, t, w, h, draw):
        trampoline = DRAW_FN(draw)
        self.desk._keepalive.append(trampoline)
        return Widget(self.desk, lib.gui_custom_create(
            self.h, l, t, w, h, trampoline, None))


class Slot(Widget):
    def set_item(self, name, category=0, color=0xFFCC8844, glyph=ord("?"), tag=-1):
        lib.gui_slot_set_item(self.h, _b(name), category, color, glyph, tag)

    def clear(self):
        lib.gui_slot_clear(self.h)

    @property
    def item_tag(self):
        return lib.gui_slot_item_tag(self.h)

    @property
    def filled(self):
        return lib.gui_slot_item_tag(self.h) != -2


class Grid(Widget):
    def slot_at(self, col, row):
        return Slot(self.desk, lib.gui_grid_slot_at(self.h, col, row))


class Tabs(Widget):
    def add(self, caption):
        lib.gui_tabs_add(self.h, _b(caption))

    @property
    def active(self):
        return lib.gui_tabs_active(self.h)
