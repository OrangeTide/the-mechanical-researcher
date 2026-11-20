#!/usr/bin/env python3
"""
demo.py : the Python binding driving the Pascal GUI through its C ABI.

  python3 demo.py              open a window and run the scene (needs a display)
  python3 demo.py --shot f.bmp render one frame headless to a BMP file
  python3 demo.py --selftest   drive the ABI headless and check it round-trips

The scene is a System 7 inventory window with a 4x2 grid of item slots and an
equipment window with a single weapon slot. A custom-draw widget paints a small
gauge the core knows nothing about, proving the escape hatch crosses to Python.
"""

import sys
import gui


def gauge(cv, l, t, r, b, user):
    """A foreign paint routine: a two-tone bar drawn through the ABI canvas."""
    gui.lib.gui_canvas_fill_rect(cv, l, t, r, b, 0xFF202020)
    mid = l + (r - l) * 2 // 3
    gui.lib.gui_canvas_fill_rect(cv, l + 2, t + 2, mid, b - 2, 0xFF30C030)
    gui.lib.gui_canvas_frame_rect(cv, l, t, r, b, 0xFFFFFFFF)
    gui.lib.gui_canvas_text(cv, l + 4, t + 4, b"HP", 0xFFFFFFFF)


def build(theme):
    d = gui.Desktop(theme=theme, w=760, h=520)

    inv = d.window("Inventory", flags=gui.WF_CLOSE | gui.WF_DRAG | gui.WF_ZOOM,
                   l=40, t=40, w=360, h=260)
    c = inv.content()
    tabs = c.tabs(12, 8, tabw=70, h=22)
    tabs.add("Bag")
    tabs.add("Quest")
    grid = c.grid(4, 2, accept=0, pitch=44, l=12, t=40)
    # fill a few slots with tagged items; the tag is the item's identity, so a
    # drop can be read back by reading the destination slot's tag.
    labels = ["Map", "Key", "Gem", "Coin"]
    for i, name in enumerate(labels):
        grid.slot_at(i, 0).set_item(
            name, category=1, color=0xFF3366CC, glyph=ord(name[0]), tag=100 + i)

    equip = d.window("Equipment", flags=gui.WF_CLOSE | gui.WF_DRAG,
                     l=440, t=40, w=260, h=260)
    ec = equip.content()
    ec.label("Weapon", 20, 16)
    weapon = ec.slot(20, 32, sz=44, accept=0)
    ec.custom(20, 100, 200, 28, gauge)

    return d, grid, weapon


def selftest():
    ok = True
    d, grid, weapon = build(gui.THEME_SYSTEM7)

    src = grid.slot_at(0, 0)
    assert src.filled, "source slot should start filled"
    assert not weapon.filled, "weapon slot should start empty"
    src_tag = src.item_tag

    sx, sy = src.center()
    wx, wy = weapon.center()
    d.drag(sx, sy, wx, wy)

    # a drop should have queued an event
    saw_drop = False
    while True:
        ev = d.poll()
        if ev is None:
            break
        if ev.kind == gui.EV_DROP:
            saw_drop = True

    moved = weapon.filled and not src.filled and weapon.item_tag == src_tag
    print("drop event delivered:", saw_drop)
    print("item moved src->weapon (tag %d):" % src_tag, moved)
    ok = ok and saw_drop and moved

    # run-time theme swap
    d.theme = gui.THEME_NES
    theme_ok = d.theme == gui.THEME_NES
    print("run-time theme swap:", theme_ok)
    ok = ok and theme_ok

    # stale-handle hardening: destroy the desktop, then a call on an old handle
    # must fail cleanly rather than crash.
    old = weapon.h
    d.destroy()
    rc = gui.lib.gui_slot_item_tag(old)
    print("stale handle rejected cleanly:", rc < 0)
    ok = ok and (rc < 0)

    print("SELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def shot(path):
    d, _, _ = build(gui.THEME_SYSTEM7)
    rc = d.render_bmp(path)
    print("wrote", path if rc == 0 else "(failed)")
    d.destroy()
    return 0 if rc == 0 else 1


def run():
    # press T to toggle the theme live between System 7 and NES
    d, _, _ = build(gui.THEME_SYSTEM7)
    print("press T to toggle the theme, close the window to quit")
    while d.run_frame() == 1:
        ev = d.poll()
        while ev is not None:
            if ev.kind == gui.EV_KEY and ev.i0 in (ord("t"), ord("T")):
                d.theme = (gui.THEME_SYSTEM7 if d.theme == gui.THEME_NES
                           else gui.THEME_NES)
            ev = d.poll()
    d.destroy()
    return 0


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    elif len(sys.argv) >= 3 and sys.argv[1] == "--shot":
        sys.exit(shot(sys.argv[2]))
    else:
        sys.exit(run())
