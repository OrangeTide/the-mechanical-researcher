/*
 * example_c.c : the C binding to libgui.so.
 *
 *   example_c            open a window and run the scene (needs a display)
 *   example_c --shot f   render one frame headless to a BMP file
 *   example_c --selftest drive the ABI headless and check it round-trips
 *
 * C is the ABI's home audience: it includes gui.h and calls the handles
 * directly, with no marshalling layer. The scene matches the other bindings.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "gui.h"

/* a foreign paint routine: a small HP gauge the core knows nothing about */
static void
gauge(void *cv, int32_t l, int32_t t, int32_t r, int32_t b, void *user)
{
    (void)user;
    int mid = l + (r - l) * 2 / 3;
    gui_canvas_fill_rect(cv, l, t, r, b, 0xFF202020u);
    gui_canvas_fill_rect(cv, l + 2, t + 2, mid, b - 2, 0xFF30C030u);
    gui_canvas_frame_rect(cv, l, t, r, b, 0xFFFFFFFFu);
    gui_canvas_text(cv, l + 4, t + 4, "HP", 0xFFFFFFFFu);
}

static gui_handle g_grid, g_weapon;

static gui_handle
build(int theme)
{
    gui_handle d = gui_desktop_create(theme, 760, 520);
    static const char *labels[4] = { "Map", "Key", "Gem", "Coin" };
    gui_handle inv, c, tabs, equip, ec, i;

    inv = gui_window_create(d, "Inventory",
                            GUI_WF_CLOSE | GUI_WF_DRAG | GUI_WF_ZOOM,
                            40, 40, 360, 260);
    c = gui_window_content(inv);
    tabs = gui_tabs_create(c, 12, 8, 70, 22);
    gui_tabs_add(tabs, "Bag");
    gui_tabs_add(tabs, "Quest");
    g_grid = gui_grid_create(c, 4, 2, 0, 44, 12, 40);
    for (i = 0; i < 4; i++) {
        gui_handle s = gui_grid_slot_at(g_grid, (int)i, 0);
        gui_slot_set_item(s, labels[i], 1, 0xFF3366CCu, labels[i][0], 100 + (int)i);
    }

    equip = gui_window_create(d, "Equipment", GUI_WF_CLOSE | GUI_WF_DRAG,
                              440, 40, 260, 260);
    ec = gui_window_content(equip);
    gui_label_create(ec, "Weapon", 20, 16);
    g_weapon = gui_slot_create(ec, 0, 20, 32, 44);
    gui_custom_create(ec, 20, 100, 200, 28, gauge, NULL);
    return d;
}

static int
center_x(gui_handle w)
{
    int32_t l, t, r, b;
    gui_widget_abs_rect(w, &l, &t, &r, &b);
    return (l + r) / 2;
}

static int
center_y(gui_handle w)
{
    int32_t l, t, r, b;
    gui_widget_abs_rect(w, &l, &t, &r, &b);
    return (t + b) / 2;
}

static int
selftest(void)
{
    gui_handle d = build(GUI_THEME_SYSTEM7);
    gui_handle src = gui_grid_slot_at(g_grid, 0, 0);
    int src_tag = gui_slot_item_tag(src);
    int sx = center_x(src), sy = center_y(src);
    int wx = center_x(g_weapon), wy = center_y(g_weapon);
    int saw_drop = 0, moved, stale_ok, ok;
    gui_event ev;

    if (gui_slot_item_tag(src) < 0) { printf("source empty at start\n"); return 1; }
    if (gui_slot_item_tag(g_weapon) != -2) { printf("weapon not empty at start\n"); return 1; }

    gui_dispatch_mouse(d, 0, sx, sy, 0, 0);               /* down on source */
    gui_dispatch_mouse(d, 2, (sx + wx) / 2, (sy + wy) / 2, 0, 0); /* move */
    gui_dispatch_mouse(d, 2, wx, wy, 0, 0);               /* move to weapon */
    gui_dispatch_mouse(d, 1, wx, wy, 0, 0);               /* up on weapon */

    while (gui_poll_event(d, &ev) == 1)
        if (ev.kind == GUI_EV_DROP)
            saw_drop = 1;

    moved = (gui_slot_item_tag(g_weapon) == src_tag) &&
            (gui_slot_item_tag(src) == -2);
    printf("drop event delivered: %d\n", saw_drop);
    printf("item moved src->weapon (tag %d): %d\n", src_tag, moved);

    /* run-time theme swap: install NES, confirm it took */
    int theme_ok = gui_desktop_set_theme(d, GUI_THEME_NES) == 0 &&
                   gui_desktop_theme(d) == GUI_THEME_NES;
    printf("run-time theme swap: %d\n", theme_ok);

    /* stale-handle hardening: after destroy an old handle must fail cleanly */
    gui_desktop_destroy(d);
    stale_ok = gui_slot_item_tag(g_weapon) < 0;
    printf("stale handle rejected cleanly: %d\n", stale_ok);

    ok = saw_drop && moved && theme_ok && stale_ok;
    printf("SELFTEST %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static int
shot(const char *path)
{
    gui_handle d = build(GUI_THEME_SYSTEM7);
    int rc = gui_render_bmp(d, path);
    printf(rc == 0 ? "wrote %s\n" : "render failed (%s)\n", path);
    gui_desktop_destroy(d);
    return rc == 0 ? 0 : 1;
}

static int
run(void)
{
    /* press T to toggle the theme live between System 7 and NES */
    gui_handle d = build(GUI_THEME_SYSTEM7);
    gui_event ev;
    printf("press T to toggle the theme, close the window to quit\n");
    while (gui_run_frame(d) == 1) {
        while (gui_poll_event(d, &ev) == 1) {
            if (ev.kind == GUI_EV_KEY && (ev.i0 == 't' || ev.i0 == 'T')) {
                int cur = gui_desktop_theme(d);
                gui_desktop_set_theme(d, cur == GUI_THEME_NES
                                      ? GUI_THEME_SYSTEM7 : GUI_THEME_NES);
            }
        }
    }
    gui_desktop_destroy(d);
    return 0;
}

int
main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--selftest") == 0)
        return selftest();
    if (argc >= 3 && strcmp(argv[1], "--shot") == 0)
        return shot(argv[2]);
    return run();
}
