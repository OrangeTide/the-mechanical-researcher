/*
 * gui.h : the C ABI for the Pascal retained-mode GUI toolkit.
 *
 * Every object is named by an opaque 64-bit handle. A handle carries a
 * generation counter, so a stale or forged handle fails cleanly rather than
 * dereferencing freed memory. Handle 0 is null / error. Functions that return
 * a handle return 0 on failure; functions that return int return 0 on success
 * and a negative code on failure.
 *
 * The library is backend-complete: it ships an SDL2 canvas and owns the
 * window and render loop (gui_run_frame), so a caller in any language needs no
 * renderer of its own. gui_custom_create plus the gui_canvas_* calls are the
 * one escape hatch for drawing a widget the core does not know about.
 */
#ifndef PASCAL_GAME_GUI_H
#define PASCAL_GAME_GUI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t gui_handle;

/* theme ids for gui_desktop_create */
#define GUI_THEME_SYSTEM7 0
#define GUI_THEME_NES     1

/* window flag bits */
#define GUI_WF_CLOSE 1
#define GUI_WF_DRAG  2
#define GUI_WF_ZOOM  4
#define GUI_WF_GROW  8

/* event kinds */
#define GUI_EV_NONE   0
#define GUI_EV_BUTTON 1   /* widget = button, i0 = button tag           */
#define GUI_EV_TAB    2   /* widget = tab bar, i0 = active tab index     */
#define GUI_EV_DROP   3   /* an item settled; re-read the slots you own  */
#define GUI_EV_POPUP  4   /* widget = popup, i0 = chosen index           */
#define GUI_EV_KEY    5   /* a key was pressed, i0 = keycode             */

typedef struct {
    int32_t  kind;
    uint64_t widget;
    int32_t  i0;
    int32_t  i1;
} gui_event;

/* a foreign paint routine for gui_custom_create; draws via gui_canvas_* */
typedef void (*gui_draw_fn)(void *canvas, int32_t l, int32_t t,
                            int32_t r, int32_t b, void *user);

int32_t gui_version(void);

/* desktop */
gui_handle gui_desktop_create(int32_t theme_id, int32_t w, int32_t h);
int32_t    gui_desktop_destroy(gui_handle d);
int32_t    gui_desktop_set_theme(gui_handle d, int32_t theme_id); /* run-time swap */
int32_t    gui_desktop_theme(gui_handle d);                       /* current id */

/* windows (parented on the desktop) */
gui_handle gui_window_create(gui_handle d, const char *title, int32_t flags,
                             int32_t l, int32_t t, int32_t w, int32_t h);
int32_t    gui_window_set_title(gui_handle win, const char *title);
int32_t    gui_window_get_title(gui_handle win, char *buf, int32_t buflen);
gui_handle gui_window_content(gui_handle win);
int32_t    gui_window_is_visible(gui_handle win);
int32_t    gui_window_set_visible(gui_handle win, int32_t vis);

/* a widget's absolute (screen) rectangle, for aiming synthetic input */
int32_t    gui_widget_abs_rect(gui_handle h, int32_t *l, int32_t *t,
                               int32_t *r, int32_t *b);

/* content widgets (parent is a window content area, grid, or scroll view) */
gui_handle gui_label_create(gui_handle parent, const char *caption,
                            int32_t l, int32_t t);
gui_handle gui_button_create(gui_handle parent, const char *caption,
                             int32_t l, int32_t t, int32_t w, int32_t h);
int32_t    gui_button_set_tag(gui_handle btn, int32_t tag);

gui_handle gui_slot_create(gui_handle parent, int32_t accept,
                           int32_t l, int32_t t, int32_t sz);
int32_t    gui_slot_set_item(gui_handle slot, const char *name,
                             int32_t category, uint32_t color,
                             int32_t glyph, int32_t tag);
int32_t    gui_slot_clear(gui_handle slot);
int32_t    gui_slot_item_tag(gui_handle slot);   /* -2 empty, -1 error */

gui_handle gui_grid_create(gui_handle parent, int32_t cols, int32_t rows,
                           int32_t accept, int32_t pitch, int32_t l, int32_t t);
gui_handle gui_grid_slot_at(gui_handle grid, int32_t col, int32_t row);

gui_handle gui_tabs_create(gui_handle parent, int32_t l, int32_t t,
                           int32_t tabw, int32_t h);
int32_t    gui_tabs_add(gui_handle tabs, const char *caption);
int32_t    gui_tabs_active(gui_handle tabs);

gui_handle gui_scrollview_create(gui_handle parent, int32_t l, int32_t t,
                                 int32_t w, int32_t h);
int32_t    gui_scrollview_set_content_size(gui_handle sv, int32_t h);

/* the custom-draw escape hatch */
gui_handle gui_custom_create(gui_handle parent, int32_t l, int32_t t,
                             int32_t w, int32_t h, gui_draw_fn draw, void *user);
int32_t gui_canvas_fill_rect(void *cv, int32_t l, int32_t t, int32_t r,
                             int32_t b, uint32_t color);
int32_t gui_canvas_frame_rect(void *cv, int32_t l, int32_t t, int32_t r,
                              int32_t b, uint32_t color);
int32_t gui_canvas_text(void *cv, int32_t x, int32_t y, const char *s,
                        uint32_t color);
int32_t gui_canvas_text_width(void *cv, const char *s);
int32_t gui_canvas_text_height(void *cv);

/* synthetic input (headless tests and bindings without a window) */
int32_t gui_dispatch_mouse(gui_handle d, int32_t kind, int32_t x, int32_t y,
                           int32_t button, int32_t ctrl);  /* kind 0 down 1 up 2 move */
int32_t gui_dispatch_wheel(gui_handle d, int32_t x, int32_t y, int32_t delta);

/* pull the next queued high-level event; returns 1 if one was written, else 0 */
int32_t gui_poll_event(gui_handle d, gui_event *ev);

/* batteries render path */
int32_t gui_run_frame(gui_handle d);              /* 1 running, 0 quit, <0 err */
int32_t gui_render_bmp(gui_handle d, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* PASCAL_GAME_GUI_H */
