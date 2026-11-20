/*
 * gui_lua.c : a Lua C module wrapping libgui.so.
 *
 * Build it into gui.so and load it with `local gui = require 'gui'`. Handles
 * cross as Lua numbers; since the library's handles stay well under 2^53 in
 * practice, the double representation is exact. The module is written for the
 * Lua 5.1 C API (what ships here); the two registration lines noted below are
 * the only thing that differs on 5.2+.
 */
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "gui.h"

static gui_handle
check_handle(lua_State *L, int i)
{
    return (gui_handle)(uint64_t)luaL_checknumber(L, i);
}

static int l_version(lua_State *L)
{
    lua_pushinteger(L, gui_version());
    return 1;
}

static int l_desktop_create(lua_State *L)
{
    int theme = luaL_optinteger(L, 1, GUI_THEME_SYSTEM7);
    int w = luaL_optinteger(L, 2, 800);
    int h = luaL_optinteger(L, 3, 600);
    lua_pushnumber(L, (lua_Number)gui_desktop_create(theme, w, h));
    return 1;
}

static int l_desktop_destroy(lua_State *L)
{
    lua_pushinteger(L, gui_desktop_destroy(check_handle(L, 1)));
    return 1;
}

static int l_desktop_set_theme(lua_State *L)
{
    lua_pushinteger(L, gui_desktop_set_theme(check_handle(L, 1),
                                             luaL_checkinteger(L, 2)));
    return 1;
}

static int l_desktop_theme(lua_State *L)
{
    lua_pushinteger(L, gui_desktop_theme(check_handle(L, 1)));
    return 1;
}

static int l_window_create(lua_State *L)
{
    gui_handle d = check_handle(L, 1);
    const char *title = luaL_checkstring(L, 2);
    int flags = luaL_checkinteger(L, 3);
    int l = luaL_checkinteger(L, 4), t = luaL_checkinteger(L, 5);
    int w = luaL_checkinteger(L, 6), h = luaL_checkinteger(L, 7);
    lua_pushnumber(L, (lua_Number)gui_window_create(d, title, flags, l, t, w, h));
    return 1;
}

static int l_window_content(lua_State *L)
{
    lua_pushnumber(L, (lua_Number)gui_window_content(check_handle(L, 1)));
    return 1;
}

static int l_window_is_visible(lua_State *L)
{
    lua_pushinteger(L, gui_window_is_visible(check_handle(L, 1)));
    return 1;
}

static int l_label_create(lua_State *L)
{
    gui_handle p = check_handle(L, 1);
    const char *cap = luaL_checkstring(L, 2);
    int l = luaL_checkinteger(L, 3), t = luaL_checkinteger(L, 4);
    lua_pushnumber(L, (lua_Number)gui_label_create(p, cap, l, t));
    return 1;
}

static int l_button_create(lua_State *L)
{
    gui_handle p = check_handle(L, 1);
    const char *cap = luaL_checkstring(L, 2);
    int l = luaL_checkinteger(L, 3), t = luaL_checkinteger(L, 4);
    int w = luaL_checkinteger(L, 5), h = luaL_checkinteger(L, 6);
    lua_pushnumber(L, (lua_Number)gui_button_create(p, cap, l, t, w, h));
    return 1;
}

static int l_button_set_tag(lua_State *L)
{
    lua_pushinteger(L, gui_button_set_tag(check_handle(L, 1), luaL_checkinteger(L, 2)));
    return 1;
}

static int l_slot_create(lua_State *L)
{
    gui_handle p = check_handle(L, 1);
    int accept = luaL_checkinteger(L, 2);
    int l = luaL_checkinteger(L, 3), t = luaL_checkinteger(L, 4);
    int sz = luaL_checkinteger(L, 5);
    lua_pushnumber(L, (lua_Number)gui_slot_create(p, accept, l, t, sz));
    return 1;
}

static int l_slot_set_item(lua_State *L)
{
    gui_handle s = check_handle(L, 1);
    const char *name = luaL_checkstring(L, 2);
    int cat = luaL_checkinteger(L, 3);
    uint32_t color = (uint32_t)luaL_checknumber(L, 4);
    int glyph = luaL_checkinteger(L, 5);
    int tag = luaL_optinteger(L, 6, -1);
    lua_pushinteger(L, gui_slot_set_item(s, name, cat, color, glyph, tag));
    return 1;
}

static int l_slot_clear(lua_State *L)
{
    lua_pushinteger(L, gui_slot_clear(check_handle(L, 1)));
    return 1;
}

static int l_slot_item_tag(lua_State *L)
{
    lua_pushinteger(L, gui_slot_item_tag(check_handle(L, 1)));
    return 1;
}

static int l_grid_create(lua_State *L)
{
    gui_handle p = check_handle(L, 1);
    int cols = luaL_checkinteger(L, 2), rows = luaL_checkinteger(L, 3);
    int accept = luaL_checkinteger(L, 4), pitch = luaL_checkinteger(L, 5);
    int l = luaL_checkinteger(L, 6), t = luaL_checkinteger(L, 7);
    lua_pushnumber(L, (lua_Number)gui_grid_create(p, cols, rows, accept, pitch, l, t));
    return 1;
}

static int l_grid_slot_at(lua_State *L)
{
    gui_handle g = check_handle(L, 1);
    lua_pushnumber(L, (lua_Number)gui_grid_slot_at(g, luaL_checkinteger(L, 2),
                                                   luaL_checkinteger(L, 3)));
    return 1;
}

static int l_tabs_create(lua_State *L)
{
    gui_handle p = check_handle(L, 1);
    int l = luaL_checkinteger(L, 2), t = luaL_checkinteger(L, 3);
    int tabw = luaL_checkinteger(L, 4), h = luaL_checkinteger(L, 5);
    lua_pushnumber(L, (lua_Number)gui_tabs_create(p, l, t, tabw, h));
    return 1;
}

static int l_tabs_add(lua_State *L)
{
    lua_pushinteger(L, gui_tabs_add(check_handle(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

static int l_tabs_active(lua_State *L)
{
    lua_pushinteger(L, gui_tabs_active(check_handle(L, 1)));
    return 1;
}

static int l_widget_abs_rect(lua_State *L)
{
    int32_t l, t, r, b;
    if (gui_widget_abs_rect(check_handle(L, 1), &l, &t, &r, &b) != 0)
        return 0;
    lua_pushinteger(L, l); lua_pushinteger(L, t);
    lua_pushinteger(L, r); lua_pushinteger(L, b);
    return 4;
}

static int l_dispatch_mouse(lua_State *L)
{
    gui_handle d = check_handle(L, 1);
    int kind = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3), y = luaL_checkinteger(L, 4);
    int button = luaL_optinteger(L, 5, 0), ctrl = luaL_optinteger(L, 6, 0);
    lua_pushinteger(L, gui_dispatch_mouse(d, kind, x, y, button, ctrl));
    return 1;
}

static int l_poll_event(lua_State *L)
{
    gui_event ev;
    if (gui_poll_event(check_handle(L, 1), &ev) != 1) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushinteger(L, ev.kind);        lua_setfield(L, -2, "kind");
    lua_pushnumber(L, (lua_Number)ev.widget); lua_setfield(L, -2, "widget");
    lua_pushinteger(L, ev.i0);          lua_setfield(L, -2, "i0");
    lua_pushinteger(L, ev.i1);          lua_setfield(L, -2, "i1");
    return 1;
}

static int l_run_frame(lua_State *L)
{
    lua_pushinteger(L, gui_run_frame(check_handle(L, 1)));
    return 1;
}

static int l_render_bmp(lua_State *L)
{
    lua_pushinteger(L, gui_render_bmp(check_handle(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

static const luaL_Reg funcs[] = {
    { "version", l_version },
    { "desktop_create", l_desktop_create },
    { "desktop_destroy", l_desktop_destroy },
    { "desktop_set_theme", l_desktop_set_theme },
    { "desktop_theme", l_desktop_theme },
    { "window_create", l_window_create },
    { "window_content", l_window_content },
    { "window_is_visible", l_window_is_visible },
    { "label_create", l_label_create },
    { "button_create", l_button_create },
    { "button_set_tag", l_button_set_tag },
    { "slot_create", l_slot_create },
    { "slot_set_item", l_slot_set_item },
    { "slot_clear", l_slot_clear },
    { "slot_item_tag", l_slot_item_tag },
    { "grid_create", l_grid_create },
    { "grid_slot_at", l_grid_slot_at },
    { "tabs_create", l_tabs_create },
    { "tabs_add", l_tabs_add },
    { "tabs_active", l_tabs_active },
    { "widget_abs_rect", l_widget_abs_rect },
    { "dispatch_mouse", l_dispatch_mouse },
    { "poll_event", l_poll_event },
    { "run_frame", l_run_frame },
    { "render_bmp", l_render_bmp },
    { NULL, NULL }
};

static void
set_const(lua_State *L, const char *name, int value)
{
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}

int
luaopen_gui(lua_State *L)
{
    /* Lua 5.1 registration. On 5.2+ use luaL_newlib(L, funcs) instead. */
    lua_newtable(L);
    luaL_register(L, NULL, funcs);

    set_const(L, "THEME_SYSTEM7", GUI_THEME_SYSTEM7);
    set_const(L, "THEME_NES", GUI_THEME_NES);
    set_const(L, "WF_CLOSE", GUI_WF_CLOSE);
    set_const(L, "WF_DRAG", GUI_WF_DRAG);
    set_const(L, "WF_ZOOM", GUI_WF_ZOOM);
    set_const(L, "WF_GROW", GUI_WF_GROW);
    set_const(L, "EV_NONE", GUI_EV_NONE);
    set_const(L, "EV_BUTTON", GUI_EV_BUTTON);
    set_const(L, "EV_TAB", GUI_EV_TAB);
    set_const(L, "EV_DROP", GUI_EV_DROP);
    set_const(L, "EV_POPUP", GUI_EV_POPUP);
    set_const(L, "EV_KEY", GUI_EV_KEY);
    set_const(L, "M_DOWN", 0);
    set_const(L, "M_UP", 1);
    set_const(L, "M_MOVE", 2);
    return 1;
}
