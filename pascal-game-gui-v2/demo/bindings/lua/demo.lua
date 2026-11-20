#!/usr/bin/env lua
-- demo.lua : the Lua binding driving the Pascal GUI through its C ABI.
--
--   lua demo.lua              open a window and run the scene (needs a display)
--   lua demo.lua --shot f.bmp render one frame headless to a BMP file
--   lua demo.lua --selftest   drive the ABI headless and check it round-trips
--
-- The module gui.so must sit next to this file (the Makefile builds it there).

package.cpath = package.cpath .. ";./?.so"
local gui = require("gui")

local function build(theme)
  local d = gui.desktop_create(theme, 760, 520)

  local inv = gui.window_create(d, "Inventory",
    gui.WF_CLOSE + gui.WF_DRAG + gui.WF_ZOOM, 40, 40, 360, 260)
  local c = gui.window_content(inv)
  local tabs = gui.tabs_create(c, 12, 8, 70, 22)
  gui.tabs_add(tabs, "Bag")
  gui.tabs_add(tabs, "Quest")
  local grid = gui.grid_create(c, 4, 2, 0, 44, 12, 40)
  local labels = { "Map", "Key", "Gem", "Coin" }
  for i = 1, 4 do
    local s = gui.grid_slot_at(grid, i - 1, 0)
    gui.slot_set_item(s, labels[i], 1, 0xFF3366CC, string.byte(labels[i]), 100 + i)
  end

  local equip = gui.window_create(d, "Equipment",
    gui.WF_CLOSE + gui.WF_DRAG, 440, 40, 260, 260)
  local ec = gui.window_content(equip)
  gui.label_create(ec, "Weapon", 20, 16)
  local weapon = gui.slot_create(ec, 0, 20, 32, 44)

  return d, grid, weapon
end

local function center(w)
  local l, t, r, b = gui.widget_abs_rect(w)
  return math.floor((l + r) / 2), math.floor((t + b) / 2)
end

local function drag(d, x0, y0, x1, y1)
  gui.dispatch_mouse(d, gui.M_DOWN, x0, y0, 0, 0)
  gui.dispatch_mouse(d, gui.M_MOVE, math.floor((x0 + x1) / 2), math.floor((y0 + y1) / 2), 0, 0)
  gui.dispatch_mouse(d, gui.M_MOVE, x1, y1, 0, 0)
  gui.dispatch_mouse(d, gui.M_UP, x1, y1, 0, 0)
end

local function selftest()
  local d, grid, weapon = build(gui.THEME_SYSTEM7)
  local src = gui.grid_slot_at(grid, 0, 0)
  local src_tag = gui.slot_item_tag(src)

  assert(src_tag >= 0, "source slot should start filled")
  assert(gui.slot_item_tag(weapon) == -2, "weapon slot should start empty")

  local sx, sy = center(src)
  local wx, wy = center(weapon)
  drag(d, sx, sy, wx, wy)

  local saw_drop = false
  while true do
    local ev = gui.poll_event(d)
    if not ev then break end
    if ev.kind == gui.EV_DROP then saw_drop = true end
  end

  local moved = gui.slot_item_tag(weapon) == src_tag and gui.slot_item_tag(src) == -2
  print("drop event delivered: " .. tostring(saw_drop))
  print("item moved src->weapon (tag " .. src_tag .. "): " .. tostring(moved))

  -- run-time theme swap
  gui.desktop_set_theme(d, gui.THEME_NES)
  local theme_ok = gui.desktop_theme(d) == gui.THEME_NES
  print("run-time theme swap: " .. tostring(theme_ok))

  -- stale-handle hardening
  gui.desktop_destroy(d)
  local stale_ok = gui.slot_item_tag(weapon) < 0
  print("stale handle rejected cleanly: " .. tostring(stale_ok))

  local ok = saw_drop and moved and theme_ok and stale_ok
  print("SELFTEST " .. (ok and "PASS" or "FAIL"))
  os.exit(ok and 0 or 1)
end

local function shot(path)
  local d = build(gui.THEME_SYSTEM7)
  local rc = gui.render_bmp(d, path)
  print(rc == 0 and ("wrote " .. path) or "render failed")
  gui.desktop_destroy(d)
  os.exit(rc == 0 and 0 or 1)
end

local function run()
  -- press T to toggle the theme live between System 7 and NES
  local d = build(gui.THEME_SYSTEM7)
  print("press T to toggle the theme, close the window to quit")
  while gui.run_frame(d) == 1 do
    local ev = gui.poll_event(d)
    while ev do
      if ev.kind == gui.EV_KEY and (ev.i0 == 116 or ev.i0 == 84) then
        local cur = gui.desktop_theme(d)
        gui.desktop_set_theme(d, cur == gui.THEME_NES and gui.THEME_SYSTEM7 or gui.THEME_NES)
      end
      ev = gui.poll_event(d)
    end
  end
  gui.desktop_destroy(d)
end

if arg[1] == "--selftest" then
  selftest()
elseif arg[1] == "--shot" and arg[2] then
  shot(arg[2])
else
  run()
end
