library guiabi;
{ The C ABI for the retained-mode GUI. This is the only unit that knows the C }
{ calling convention; it wraps the untouched Pascal core (gui, guitheme,      }
{ nestheme) and the SDL2 canvas. The core stays pure Pascal.                  }
{                                                                             }
{ Boundary rules, enforced here and nowhere else:                            }
{   - every export is cdecl and wraps its body in try/except, so no Pascal    }
{     exception can unwind into C, Lua, or Python;                           }
{   - nothing managed crosses: objects are named by integer handles, strings  }
{     pass as PChar in and through caller buffers out;                       }
{   - handles carry a generation counter, so a stale or forged handle fails   }
{     cleanly instead of dereferencing freed memory.                         }

{$mode objfpc}{$H+}
{$PACKRECORDS C}

uses
  Classes, sdl2min, gui, guitheme, nestheme, guisdl;

{ ============================ handle table ============================= }
{ A handle is a 64-bit value: (slot+1) in the high 32 bits, a generation    }
{ counter in the low 32. Handle 0 is null. Freeing a slot bumps its         }
{ generation, so any handle still naming the old slot no longer resolves.    }

type
  THandleKind = (hkFree, hkDesktop, hkWidget, hkDragItem);
  THandleEntry = record
    Obj:  TObject;
    Gen:  LongWord;
    Kind: THandleKind;
  end;

  { the flat event delivered to callers by gui_poll_event }
  TGuiEvent = packed record
    Kind:   Int32;      { 0 none, 1 button, 2 tab, 3 drop, 4 popup }
    Widget: UInt64;     { the widget that fired it, 0 when not applicable }
    I0, I1: Int32;      { button tag / tab index / popup index }
  end;
  PGuiEvent = ^TGuiEvent;

  TGuiDrawProc = procedure(canvas: Pointer; l, t, r, b: Int32; user: Pointer); cdecl;

  { The one escape hatch: a widget whose paint is a foreign cdecl callback.   }
  { It composes into the tree like any other widget; the core knows nothing   }
  { of the language on the other side of DrawCb.                              }
  TCustomWidget = class(TWidget)
  public
    DrawCb: TGuiDrawProc;
    User:   Pointer;
    procedure Draw(cv: TCanvas; theme: TTheme); override;
  end;

const
  evNone = 0; evButton = 1; evTab = 2; evDrop = 3; evPopup = 4; evKey = 5;
  QCap = 256;

var
  Handles: array of THandleEntry;

function Encode(slot: LongInt; gen: LongWord): UInt64;
begin
  Encode := (UInt64(slot + 1) shl 32) or gen;
end;

function AllocHandle(obj: TObject; kind: THandleKind): UInt64;
var i, n: LongInt;
begin
  for i := 0 to High(Handles) do
    if Handles[i].Kind = hkFree then begin
      Handles[i].Obj := obj; Handles[i].Kind := kind;
      exit(Encode(i, Handles[i].Gen));
    end;
  n := Length(Handles);
  SetLength(Handles, n + 1);
  Handles[n].Obj := obj; Handles[n].Kind := kind; Handles[n].Gen := 1;
  AllocHandle := Encode(n, Handles[n].Gen);
end;

{ Resolve a handle to its object, or nil if stale/forged/wrong kind. }
function Resolve(h: UInt64; kind: THandleKind): TObject;
var slot: LongInt; gen: LongWord;
begin
  Resolve := nil;
  if h = 0 then exit;
  slot := LongInt(h shr 32) - 1;
  gen  := LongWord(h and $FFFFFFFF);
  if (slot < 0) or (slot > High(Handles)) then exit;
  if Handles[slot].Kind = hkFree then exit;
  if (kind <> hkFree) and (Handles[slot].Kind <> kind) then exit;
  if Handles[slot].Gen <> gen then exit;
  Resolve := Handles[slot].Obj;
end;

procedure FreeHandleByObj(obj: TObject);
var i: LongInt;
begin
  for i := 0 to High(Handles) do
    if (Handles[i].Kind <> hkFree) and (Handles[i].Obj = obj) then begin
      Handles[i].Kind := hkFree;
      Handles[i].Obj  := nil;
      Inc(Handles[i].Gen);
    end;
end;

function HandleOf(obj: TObject): UInt64;
var i: LongInt;
begin
  HandleOf := 0;
  if obj = nil then exit;
  for i := 0 to High(Handles) do
    if (Handles[i].Kind <> hkFree) and (Handles[i].Obj = obj) then
      exit(Encode(i, Handles[i].Gen));
end;

{ ====================== per-desktop ABI state ========================= }
{ Wraps a TDesktop with an event queue and the lazily-opened SDL window.    }
{ The core's object callbacks are wired to methods here that enqueue flat    }
{ events for gui_poll_event. Tabs need a per-bar relay to carry identity,    }
{ since the core's OnChange passes only the index.                          }

type
  TAbiDesktop = class;

  TTabRelay = class
    Owner:  TAbiDesktop;
    Handle: UInt64;
    procedure Changed(index: LongInt);
  end;

  TAbiDesktop = class
    Desk:    TDesktop;
    W, H:    LongInt;
    CurThemeId: LongInt;
    Q:       array[0..QCap - 1] of TGuiEvent;
    QHead, QTail: LongInt;
    Relays:  TFPList;
    { SDL batteries, opened on the first gui_run_frame }
    Win:     PSDL_Window;
    Ren:     PSDL_Renderer;
    Cv:      TSDLCanvas;
    Opened:  Boolean;
    PopupH:  UInt64;
    constructor Create(themeId: LongInt; aw, ah: LongInt);
    destructor  Destroy; override;
    procedure Enqueue(const e: TGuiEvent);
    procedure ButtonClicked(sender: TWidget);
    procedure DragEnded(sender: TWidget);
    procedure PopupSelected(index: LongInt);
    function  NewTheme(themeId: LongInt): TTheme;
  end;

procedure TTabRelay.Changed(index: LongInt);
var e: TGuiEvent;
begin
  e.Kind := evTab; e.Widget := Handle; e.I0 := index; e.I1 := 0;
  Owner.Enqueue(e);
end;

function TAbiDesktop.NewTheme(themeId: LongInt): TTheme;
begin
  if themeId = 1 then NewTheme := TNesTheme.Create
  else NewTheme := TSystem7Theme.Create;
end;

constructor TAbiDesktop.Create(themeId: LongInt; aw, ah: LongInt);
begin
  inherited Create;
  W := aw; H := ah; CurThemeId := themeId;
  Desk := TDesktop.Create(NewTheme(themeId), aw, ah);
  Desk.OnDragEnd := @DragEnded;
  Relays := TFPList.Create;
  QHead := 0; QTail := 0;
  Opened := False; Win := nil; Ren := nil; Cv := nil;
  PopupH := 0;
end;

destructor TAbiDesktop.Destroy;
var i: LongInt;
begin
  if Opened then begin
    Cv.Free;
    SDL_DestroyRenderer(Ren);
    SDL_DestroyWindow(Win);
    SDL_Quit;
  end;
  for i := 0 to Relays.Count - 1 do TObject(Relays[i]).Free;
  Relays.Free;
  Desk.Theme.Free;      { the theme we allocated in NewTheme }
  Desk.Free;            { frees the whole widget tree }
  inherited Destroy;
end;

procedure TAbiDesktop.Enqueue(const e: TGuiEvent);
var nt: LongInt;
begin
  nt := (QTail + 1) mod QCap;
  if nt = QHead then exit;   { queue full, drop oldest-first by refusing newest }
  Q[QTail] := e;
  QTail := nt;
end;

procedure TAbiDesktop.ButtonClicked(sender: TWidget);
var e: TGuiEvent;
begin
  e.Kind := evButton; e.Widget := HandleOf(sender);
  e.I0 := TButton(sender).Tag; e.I1 := 0;
  Enqueue(e);
end;

procedure TAbiDesktop.DragEnded(sender: TWidget);
var e: TGuiEvent;
begin
  { the core reports a drop at the desktop level; the owner re-reads slots }
  e.Kind := evDrop; e.Widget := 0; e.I0 := 0; e.I1 := 0;
  Enqueue(e);
end;

procedure TAbiDesktop.PopupSelected(index: LongInt);
var e: TGuiEvent;
begin
  e.Kind := evPopup; e.Widget := PopupH; e.I0 := index; e.I1 := 0;
  Enqueue(e);
end;

{ ====================== small cdecl helpers =========================== }

function DeskOf(h: UInt64): TAbiDesktop;
begin
  DeskOf := TAbiDesktop(Resolve(h, hkDesktop));
end;

function WidgetOf(h: UInt64): TWidget;
begin
  WidgetOf := TWidget(Resolve(h, hkWidget));
end;

{ Map a core desktop root back to its ABI wrapper, so widget constructors can }
{ wire callbacks. Scans the handle table (small in practice).                 }
function DeskStateOf(root: TDesktop): TAbiDesktop;
var i: LongInt;
begin
  DeskStateOf := nil;
  for i := 0 to High(Handles) do
    if (Handles[i].Kind = hkDesktop) and (TAbiDesktop(Handles[i].Obj).Desk = root) then
      exit(TAbiDesktop(Handles[i].Obj));
end;

procedure TCustomWidget.Draw(cv: TCanvas; theme: TTheme);
var r: TRect;
begin
  if not Assigned(DrawCb) then exit;
  r := AbsRect;
  DrawCb(Pointer(cv), r.Left, r.Top, r.Right, r.Bottom, User);
end;

{ Reparent a freshly-created widget. The core constructor takes a parent and }
{ wires itself in; we only need to hand back a handle.                       }

{ ============================ exports ================================= }

function gui_version: Int32; cdecl;
begin
  gui_version := 2;
end;

{ ---- desktop ---- }

function gui_desktop_create(themeId, w, h: Int32): UInt64; cdecl;
var a: TAbiDesktop;
begin
  gui_desktop_create := 0;
  try
    a := TAbiDesktop.Create(themeId, w, h);
    gui_desktop_create := AllocHandle(a, hkDesktop);
  except
    gui_desktop_create := 0;
  end;
end;

function gui_desktop_destroy(d: UInt64): Int32; cdecl;
var a: TAbiDesktop; i: LongInt; root: TDesktop;
begin
  gui_desktop_destroy := -1;
  try
    a := DeskOf(d);
    if a = nil then exit;
    root := a.Desk;
    { invalidate every widget handle belonging to this desktop before the }
    { tree is freed, so no stale handle survives the destroy }
    for i := 0 to High(Handles) do
      if (Handles[i].Kind = hkWidget) and (TWidget(Handles[i].Obj).Root = root) then begin
        Handles[i].Kind := hkFree; Handles[i].Obj := nil; Inc(Handles[i].Gen);
      end;
    FreeHandleByObj(a);
    a.Free;
    gui_desktop_destroy := 0;
  except
    gui_desktop_destroy := -1;
  end;
end;

{ Swap the desktop's theme at run time. Allocates the new theme, installs it,  }
{ and frees the one it replaced (the ABI owns the theme objects it made).      }
function gui_desktop_set_theme(d: UInt64; themeId: Int32): Int32; cdecl;
var a: TAbiDesktop; old, fresh: TTheme;
begin
  gui_desktop_set_theme := -1;
  try
    a := DeskOf(d);
    if a = nil then exit;
    old := a.Desk.Theme;
    fresh := a.NewTheme(themeId);
    a.Desk.SetTheme(fresh);
    if (old <> nil) and (old <> fresh) then old.Free;
    a.CurThemeId := themeId;
    gui_desktop_set_theme := 0;
  except
    gui_desktop_set_theme := -1;
  end;
end;

{ Report the desktop's current theme id (for a caller that toggles). }
function gui_desktop_theme(d: UInt64): Int32; cdecl;
var a: TAbiDesktop;
begin
  gui_desktop_theme := -1;
  try
    a := DeskOf(d);
    if a <> nil then gui_desktop_theme := a.CurThemeId;
  except
    gui_desktop_theme := -1;
  end;
end;

{ ---- windows ---- }

function gui_window_create(d: UInt64; title: PChar; flags, l, t, w, h: Int32): UInt64; cdecl;
var a: TAbiDesktop; win: TWindow; fs: TWindowFlags;
begin
  gui_window_create := 0;
  try
    a := DeskOf(d);
    if a = nil then exit;
    fs := [];
    if (flags and 1) <> 0 then Include(fs, wfClose);
    if (flags and 2) <> 0 then Include(fs, wfDrag);
    if (flags and 4) <> 0 then Include(fs, wfZoom);
    if (flags and 8) <> 0 then Include(fs, wfGrow);
    win := TWindow.Create(a.Desk, StrPas(title), fs, l, t, w, h);
    gui_window_create := AllocHandle(win, hkWidget);
  except
    gui_window_create := 0;
  end;
end;

function gui_window_set_title(win: UInt64; title: PChar): Int32; cdecl;
var w: TWidget;
begin
  gui_window_set_title := -1;
  try
    w := WidgetOf(win);
    if (w = nil) or not (w is TWindow) then exit;
    TWindow(w).Title := StrPas(title);
    gui_window_set_title := 0;
  except
    gui_window_set_title := -1;
  end;
end;

function gui_window_get_title(win: UInt64; buf: PChar; buflen: Int32): Int32; cdecl;
var w: TWidget; s: String; n: LongInt;
begin
  gui_window_get_title := -1;
  try
    w := WidgetOf(win);
    if (w = nil) or not (w is TWindow) then exit;
    s := TWindow(w).Title;
    if (buf <> nil) and (buflen > 0) then begin
      n := Length(s);
      if n > buflen - 1 then n := buflen - 1;
      if n > 0 then Move(s[1], buf^, n);
      buf[n] := #0;
    end;
    gui_window_get_title := Length(s);
  except
    gui_window_get_title := -1;
  end;
end;

function gui_window_content(win: UInt64): UInt64; cdecl;
var w: TWidget;
begin
  gui_window_content := 0;
  try
    w := WidgetOf(win);
    if (w = nil) or not (w is TWindow) then exit;
    gui_window_content := AllocHandle(TWindow(w).Content, hkWidget);
  except
    gui_window_content := 0;
  end;
end;

function gui_window_is_visible(win: UInt64): Int32; cdecl;
var w: TWidget;
begin
  gui_window_is_visible := -1;
  try
    w := WidgetOf(win);
    if (w = nil) or not (w is TWindow) then exit;
    if w.Visible then gui_window_is_visible := 1 else gui_window_is_visible := 0;
  except
    gui_window_is_visible := -1;
  end;
end;

function gui_window_set_visible(win: UInt64; vis: Int32): Int32; cdecl;
var w: TWidget;
begin
  gui_window_set_visible := -1;
  try
    w := WidgetOf(win);
    if (w = nil) or not (w is TWindow) then exit;
    w.Visible := vis <> 0;
    gui_window_set_visible := 0;
  except
    gui_window_set_visible := -1;
  end;
end;

{ Report a widget's absolute (screen) rectangle, so a binding can aim a      }
{ synthetic click or drag at it without tracking layout itself.              }
function gui_widget_abs_rect(h: UInt64; out l, t, r, b: Int32): Int32; cdecl;
var w: TWidget; rc: TRect;
begin
  gui_widget_abs_rect := -1;
  try
    w := WidgetOf(h);
    if w = nil then exit;
    rc := w.AbsRect;
    l := rc.Left; t := rc.Top; r := rc.Right; b := rc.Bottom;
    gui_widget_abs_rect := 0;
  except
    gui_widget_abs_rect := -1;
  end;
end;

{ ---- content widgets ---- }

function gui_label_create(parent: UInt64; caption: PChar; l, t: Int32): UInt64; cdecl;
var p: TWidget; lb: TLabel;
begin
  gui_label_create := 0;
  try
    p := WidgetOf(parent);
    if p = nil then exit;
    lb := TLabel.Create(p, StrPas(caption), l, t);
    gui_label_create := AllocHandle(lb, hkWidget);
  except
    gui_label_create := 0;
  end;
end;

function gui_button_create(parent: UInt64; caption: PChar; l, t, w, h: Int32): UInt64; cdecl;
var p: TWidget; b: TButton; a: TAbiDesktop;
begin
  gui_button_create := 0;
  try
    p := WidgetOf(parent);
    if p = nil then exit;
    b := TButton.Create(p, StrPas(caption), l, t, w, h);
    if p.Root <> nil then begin
      a := TAbiDesktop(DeskStateOf(p.Root));
      if a <> nil then b.OnClick := @a.ButtonClicked;
    end;
    gui_button_create := AllocHandle(b, hkWidget);
  except
    gui_button_create := 0;
  end;
end;

function gui_button_set_tag(btn: UInt64; tag: Int32): Int32; cdecl;
var w: TWidget;
begin
  gui_button_set_tag := -1;
  try
    w := WidgetOf(btn);
    if (w = nil) or not (w is TButton) then exit;
    TButton(w).Tag := tag;
    gui_button_set_tag := 0;
  except
    gui_button_set_tag := -1;
  end;
end;

function gui_slot_create(parent: UInt64; accept, l, t, sz: Int32): UInt64; cdecl;
var p: TWidget; s: TItemSlot;
begin
  gui_slot_create := 0;
  try
    p := WidgetOf(parent);
    if p = nil then exit;
    s := TItemSlot.Create(p, accept, l, t, sz);
    gui_slot_create := AllocHandle(s, hkWidget);
  except
    gui_slot_create := 0;
  end;
end;

function gui_slot_set_item(slot: UInt64; name: PChar; category: Int32;
  color: UInt32; glyph, tag: Int32): Int32; cdecl;
var w: TWidget; it: TDragItem; g: Char;
begin
  gui_slot_set_item := -1;
  try
    w := WidgetOf(slot);
    if (w = nil) or not (w is TItemSlot) then exit;
    if glyph > 0 then g := Chr(glyph and $FF) else g := ' ';
    it := TDragItem.Create(StrPas(name), category, TColor(color), g);
    it.Tag := tag;
    TItemSlot(w).Item.Free;
    TItemSlot(w).Item := it;
    gui_slot_set_item := 0;
  except
    gui_slot_set_item := -1;
  end;
end;

function gui_slot_clear(slot: UInt64): Int32; cdecl;
var w: TWidget;
begin
  gui_slot_clear := -1;
  try
    w := WidgetOf(slot);
    if (w = nil) or not (w is TItemSlot) then exit;
    TItemSlot(w).Item.Free;
    TItemSlot(w).Item := nil;
    gui_slot_clear := 0;
  except
    gui_slot_clear := -1;
  end;
end;

{ Returns the slot's item tag, or -2 when the slot is empty, or -1 on error. }
function gui_slot_item_tag(slot: UInt64): Int32; cdecl;
var w: TWidget;
begin
  gui_slot_item_tag := -1;
  try
    w := WidgetOf(slot);
    if (w = nil) or not (w is TItemSlot) then exit;
    if TItemSlot(w).Item = nil then gui_slot_item_tag := -2
    else gui_slot_item_tag := TItemSlot(w).Item.Tag;
  except
    gui_slot_item_tag := -1;
  end;
end;

function gui_grid_create(parent: UInt64; cols, rows, accept, pitch, l, t: Int32): UInt64; cdecl;
var p: TWidget; grid: TItemGrid;
begin
  gui_grid_create := 0;
  try
    p := WidgetOf(parent);
    if p = nil then exit;
    grid := TItemGrid.Create(p, cols, rows, accept, pitch, l, t);
    gui_grid_create := AllocHandle(grid, hkWidget);
  except
    gui_grid_create := 0;
  end;
end;

function gui_grid_slot_at(grid: UInt64; col, row: Int32): UInt64; cdecl;
var w: TWidget; s: TItemSlot;
begin
  gui_grid_slot_at := 0;
  try
    w := WidgetOf(grid);
    if (w = nil) or not (w is TItemGrid) then exit;
    s := TItemGrid(w).SlotAt(col, row);
    if s = nil then exit;
    gui_grid_slot_at := HandleOf(s);
    if gui_grid_slot_at = 0 then gui_grid_slot_at := AllocHandle(s, hkWidget);
  except
    gui_grid_slot_at := 0;
  end;
end;

function gui_tabs_create(parent: UInt64; l, t, tabw, h: Int32): UInt64; cdecl;
var p: TWidget; tabs: TTabBar; a: TAbiDesktop; relay: TTabRelay; hh: UInt64;
begin
  gui_tabs_create := 0;
  try
    p := WidgetOf(parent);
    if p = nil then exit;
    tabs := TTabBar.Create(p, l, t, tabw, h);
    hh := AllocHandle(tabs, hkWidget);
    if p.Root <> nil then begin
      a := TAbiDesktop(DeskStateOf(p.Root));
      if a <> nil then begin
        relay := TTabRelay.Create;
        relay.Owner := a; relay.Handle := hh;
        a.Relays.Add(relay);
        tabs.OnChange := @relay.Changed;
      end;
    end;
    gui_tabs_create := hh;
  except
    gui_tabs_create := 0;
  end;
end;

function gui_tabs_add(tabs: UInt64; caption: PChar): Int32; cdecl;
var w: TWidget;
begin
  gui_tabs_add := -1;
  try
    w := WidgetOf(tabs);
    if (w = nil) or not (w is TTabBar) then exit;
    TTabBar(w).AddTab(StrPas(caption));
    gui_tabs_add := 0;
  except
    gui_tabs_add := -1;
  end;
end;

function gui_tabs_active(tabs: UInt64): Int32; cdecl;
var w: TWidget;
begin
  gui_tabs_active := -1;
  try
    w := WidgetOf(tabs);
    if (w = nil) or not (w is TTabBar) then exit;
    gui_tabs_active := TTabBar(w).ActiveTab;
  except
    gui_tabs_active := -1;
  end;
end;

function gui_scrollview_create(parent: UInt64; l, t, w, h: Int32): UInt64; cdecl;
var p: TWidget; sv: TScrollView;
begin
  gui_scrollview_create := 0;
  try
    p := WidgetOf(parent);
    if p = nil then exit;
    sv := TScrollView.Create(p, l, t, w, h);
    gui_scrollview_create := AllocHandle(sv, hkWidget);
  except
    gui_scrollview_create := 0;
  end;
end;

function gui_scrollview_set_content_size(sv: UInt64; h: Int32): Int32; cdecl;
var w: TWidget;
begin
  gui_scrollview_set_content_size := -1;
  try
    w := WidgetOf(sv);
    if (w = nil) or not (w is TScrollView) then exit;
    TScrollView(w).SetContentSize(h);
    gui_scrollview_set_content_size := 0;
  except
    gui_scrollview_set_content_size := -1;
  end;
end;

{ ---- the custom-draw escape hatch ---- }

function gui_custom_create(parent: UInt64; l, t, w, h: Int32;
  draw: TGuiDrawProc; user: Pointer): UInt64; cdecl;
var p: TWidget; c: TCustomWidget;
begin
  gui_custom_create := 0;
  try
    p := WidgetOf(parent);
    if p = nil then exit;
    c := TCustomWidget.Create(p);
    c.SetBounds(l, t, l + w, t + h);
    c.DrawCb := draw; c.User := user;
    gui_custom_create := AllocHandle(c, hkWidget);
  except
    gui_custom_create := 0;
  end;
end;

{ Canvas primitives for custom-draw callbacks. The token is the TCanvas the }
{ core handed the widget; a foreign draw routine paints through it.          }

function gui_canvas_fill_rect(cv: Pointer; l, t, r, b: Int32; color: UInt32): Int32; cdecl;
begin
  gui_canvas_fill_rect := -1;
  try
    if cv = nil then exit;
    TCanvas(cv).FillRect(Rect(l, t, r, b), TColor(color));
    gui_canvas_fill_rect := 0;
  except
    gui_canvas_fill_rect := -1;
  end;
end;

function gui_canvas_frame_rect(cv: Pointer; l, t, r, b: Int32; color: UInt32): Int32; cdecl;
begin
  gui_canvas_frame_rect := -1;
  try
    if cv = nil then exit;
    TCanvas(cv).FrameRect(Rect(l, t, r, b), TColor(color));
    gui_canvas_frame_rect := 0;
  except
    gui_canvas_frame_rect := -1;
  end;
end;

function gui_canvas_text(cv: Pointer; x, y: Int32; s: PChar; color: UInt32): Int32; cdecl;
begin
  gui_canvas_text := -1;
  try
    if cv = nil then exit;
    TCanvas(cv).DrawText(x, y, StrPas(s), TColor(color));
    gui_canvas_text := 0;
  except
    gui_canvas_text := -1;
  end;
end;

function gui_canvas_text_width(cv: Pointer; s: PChar): Int32; cdecl;
begin
  gui_canvas_text_width := 0;
  try
    if cv = nil then exit;
    gui_canvas_text_width := TCanvas(cv).TextWidth(StrPas(s));
  except
    gui_canvas_text_width := 0;
  end;
end;

function gui_canvas_text_height(cv: Pointer): Int32; cdecl;
begin
  gui_canvas_text_height := 0;
  try
    if cv = nil then exit;
    gui_canvas_text_height := TCanvas(cv).TextHeight;
  except
    gui_canvas_text_height := 0;
  end;
end;

{ ---- synthetic event injection (headless, selftest, bindings) ---- }

function gui_dispatch_mouse(d: UInt64; kind, x, y, button, ctrl: Int32): Int32; cdecl;
var a: TAbiDesktop; e: TUIEvent;
begin
  gui_dispatch_mouse := -1;
  try
    a := DeskOf(d);
    if a = nil then exit;
    e.Handled := False; e.Wheel := 0; e.Key := 0;
    e.Pos.X := x; e.Pos.Y := y; e.Button := button; e.Ctrl := ctrl <> 0;
    case kind of
      0: e.Kind := ekMouseDown;
      1: e.Kind := ekMouseUp;
      2: e.Kind := ekMouseMove;
    else exit;
    end;
    a.Desk.Dispatch(e);
    gui_dispatch_mouse := 0;
  except
    gui_dispatch_mouse := -1;
  end;
end;

function gui_dispatch_wheel(d: UInt64; x, y, delta: Int32): Int32; cdecl;
var a: TAbiDesktop; e: TUIEvent;
begin
  gui_dispatch_wheel := -1;
  try
    a := DeskOf(d);
    if a = nil then exit;
    e.Handled := False; e.Ctrl := False; e.Key := 0; e.Button := 0;
    e.Kind := ekMouseWheel; e.Pos.X := x; e.Pos.Y := y; e.Wheel := delta;
    a.Desk.Dispatch(e);
    gui_dispatch_wheel := 0;
  except
    gui_dispatch_wheel := -1;
  end;
end;

{ ---- event queue ---- }

function gui_poll_event(d: UInt64; out ev: TGuiEvent): Int32; cdecl;
var a: TAbiDesktop;
begin
  gui_poll_event := 0;
  try
    a := DeskOf(d);
    if a = nil then exit(-1);
    if a.QHead = a.QTail then begin
      ev.Kind := evNone; ev.Widget := 0; ev.I0 := 0; ev.I1 := 0;
      exit(0);
    end;
    ev := a.Q[a.QHead];
    a.QHead := (a.QHead + 1) mod QCap;
    gui_poll_event := 1;
  except
    gui_poll_event := -1;
  end;
end;

{ ---- batteries render path (SDL2) ---- }

procedure EnsureOpen(a: TAbiDesktop);
begin
  if a.Opened then exit;
  SDL_Init(SDL_INIT_VIDEO);
  a.Win := SDL_CreateWindow('Pascal Game GUI v2', SDL_WINDOWPOS_CENTERED,
             SDL_WINDOWPOS_CENTERED, a.W, a.H, SDL_WINDOW_SHOWN);
  a.Ren := SDL_CreateRenderer(a.Win, -1, SDL_RENDERER_ACCELERATED or SDL_RENDERER_PRESENTVSYNC);
  a.Cv  := TSDLCanvas.Create(a.Ren);
  a.Opened := True;
end;

{ Pump SDL input, dispatch to the core, paint and present one frame.         }
{ Returns 1 while running, 0 after a quit request.                           }
function gui_run_frame(d: UInt64): Int32; cdecl;
var a: TAbiDesktop; se: TSDL_Event; e: TUIEvent; qe: TGuiEvent; running: Int32;
begin
  gui_run_frame := 0;
  try
    a := DeskOf(d);
    if a = nil then exit(-1);
    EnsureOpen(a);
    running := 1;
    while SDL_PollEvent(@se) <> 0 do begin
      case se.etype of
        SDL_EVENT_QUIT: running := 0;
        SDL_KEYDOWN: begin
          { deliver the key to the caller through the event queue, so the host }
          { (in any language) decides what it means, e.g. toggling the theme }
          qe.Kind := evKey; qe.Widget := 0; qe.I0 := se.key.keysym.sym; qe.I1 := 0;
          a.Enqueue(qe);
        end;
        SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP: begin
          if se.etype = SDL_MOUSEBUTTONDOWN then e.Kind := ekMouseDown else e.Kind := ekMouseUp;
          e.Pos.X := se.button.x; e.Pos.Y := se.button.y;
          if se.button.button = SDL_BUTTON_RIGHT then e.Button := 2
          else if se.button.button = SDL_BUTTON_MIDDLE then e.Button := 1
          else e.Button := 0;
          e.Ctrl := (SDL_GetModState and KMOD_CTRL) <> 0;
          e.Wheel := 0; e.Key := 0; e.Handled := False;
          a.Desk.Dispatch(e);
        end;
        SDL_MOUSEMOTION: begin
          e.Kind := ekMouseMove; e.Pos.X := se.motion.x; e.Pos.Y := se.motion.y;
          e.Button := 0; e.Ctrl := False; e.Wheel := 0; e.Key := 0; e.Handled := False;
          a.Desk.Dispatch(e);
        end;
        SDL_MOUSEWHEEL: begin
          e.Kind := ekMouseWheel; e.Wheel := se.wheel.y;
          e.Pos.X := 0; e.Pos.Y := 0; e.Button := 0; e.Ctrl := False; e.Key := 0; e.Handled := False;
          a.Desk.Dispatch(e);
        end;
      end;
    end;
    SDL_SetRenderDrawColor(a.Ren, 0, 0, 0, 255);
    SDL_RenderClear(a.Ren);
    a.Desk.PaintAll(a.Cv);
    SDL_RenderPresent(a.Ren);
    SDL_Delay(16);
    gui_run_frame := running;
  except
    gui_run_frame := -1;
  end;
end;

{ Render one frame to a BMP file with a headless software renderer.          }
function gui_render_bmp(d: UInt64; path: PChar): Int32; cdecl;
var a: TAbiDesktop; surf: PSDL_Surface; ren: PSDL_Renderer; cv: TSDLCanvas; rc: Int32;
begin
  gui_render_bmp := -1;
  try
    a := DeskOf(d);
    if a = nil then exit;
    SDL_Init(0);
    surf := SDL_CreateRGBSurface(0, a.W, a.H, 32, $00FF0000, $0000FF00, $000000FF, $FF000000);
    ren  := SDL_CreateSoftwareRenderer(surf);
    cv   := TSDLCanvas.Create(ren);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    a.Desk.PaintAll(cv);
    SDL_RenderPresent(ren);
    rc := SDL_SaveBMP(surf, path);
    cv.Free;
    SDL_DestroyRenderer(ren);
    SDL_FreeSurface(surf);
    if rc = 0 then gui_render_bmp := 0 else gui_render_bmp := -2;
  except
    gui_render_bmp := -1;
  end;
end;

exports
  gui_version,
  gui_desktop_create, gui_desktop_destroy, gui_desktop_set_theme,
  gui_desktop_theme,
  gui_window_create, gui_window_set_title, gui_window_get_title,
  gui_window_content, gui_window_is_visible, gui_window_set_visible,
  gui_widget_abs_rect,
  gui_label_create,
  gui_button_create, gui_button_set_tag,
  gui_slot_create, gui_slot_set_item, gui_slot_clear, gui_slot_item_tag,
  gui_grid_create, gui_grid_slot_at,
  gui_tabs_create, gui_tabs_add, gui_tabs_active,
  gui_scrollview_create, gui_scrollview_set_content_size,
  gui_custom_create,
  gui_canvas_fill_rect, gui_canvas_frame_rect, gui_canvas_text,
  gui_canvas_text_width, gui_canvas_text_height,
  gui_dispatch_mouse, gui_dispatch_wheel,
  gui_poll_event,
  gui_run_frame, gui_render_bmp;

begin
  SetLength(Handles, 0);
end.
