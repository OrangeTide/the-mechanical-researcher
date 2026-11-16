// SPDX-License-Identifier: 0BSD OR CC0-1.0
program demo;
{ A toy game loop: a scrolling city map as the live background, with a      }
{ multi-page inventory and an equipment paperdoll floating over it.         }
{                                                                           }
{   ./demo           run interactively (needs a display)                    }
{   ./demo --shot f  render one frame to BMP file f and exit (headless)     }

{$mode objfpc}{$H+}

uses
  SysUtils, sdl2min, gui, guisdl, rpgext, iconview;

const
  ScreenW = 800;
  ScreenH = 600;

type
  { the live background: a scrolling city map }
  TCityView = class(TGameView)
  public
    Scroll: LongInt;
    procedure Draw(cv: TCanvas; theme: TTheme); override;
  end;

procedure TCityView.Draw(cv: TCanvas; theme: TTheme);
var r: TRect; gx, gy, ox, wx, x, y, seed: LongInt; col: TColor;
begin
  r := AbsRect;
  cv.FillRect(r, TColor($FF2E6B3F));           { grass }
  ox := Scroll mod 64;
  x := r.left - ox;
  while x < r.right do begin
    cv.FillRect(Rect(x, r.top, x + 6, r.bottom), TColor($FF9A9488)); x := x + 64;
  end;
  y := r.top;
  while y < r.bottom do begin
    cv.FillRect(Rect(r.left, y, r.right, y + 6), TColor($FF9A9488)); y := y + 64;
  end;
  for gy := 0 to (ScreenH div 64) do
    for gx := 0 to (ScreenW div 64) + 1 do begin
      wx := gx + (Scroll div 64);
      seed := (wx * 73856093) xor (gy * 19349663);
      if (Abs(seed) mod 5) < 2 then begin
        x := r.left + gx * 64 - ox + 14;
        y := r.top + gy * 64 + 14;
        case (Abs(seed) div 8) mod 3 of
          0: col := TColor($FFB5483A);
          1: col := TColor($FF4A6D8C);
        else col := TColor($FFC2A24A);
        end;
        cv.FillRect(Rect(x, y, x + 30, y + 30), col);
        cv.FrameRect(Rect(x, y, x + 30, y + 30), clBlack);
      end;
    end;
end;

{ ---- scene construction ---- }

var
  theme: TSystem7Theme;
  desk:  TDesktop;
  city:  TCityView;
  inv:   TInventoryWindow;
  equipWin: TWindow;
  doll:  TPaperdoll;
  filesWin: TWindow;
  sview: TScrollView;
  icons: TIconView;
  demoIcon: TDeskIcon;
  lastMouse: TPoint;

procedure BuildScene;
begin
  city := TCityView.Create(desk);
  city.SetBounds(0, 0, ScreenW, ScreenH);

  { inventory, with the first bag partly filled }
  inv := TInventoryWindow.Create(desk, 40, 70);
  inv.Page(0).SlotAt(0, 0).Item := TDragItem.Create('Sword',  CAT_WEAPON, TColor($FFB5483A), 'S');
  inv.Page(0).SlotAt(1, 0).Item := TDragItem.Create('Shield', CAT_SHIELD, TColor($FF4A6D8C), 'D');
  inv.Page(0).SlotAt(2, 0).Item := TDragItem.Create('Helm',   CAT_HELMET, TColor($FF9A9488), 'H');
  inv.Page(0).SlotAt(3, 0).Item := TDragItem.Create('Armor',  CAT_ARMOR,  TColor($FF4F8C5A), 'A');
  inv.Page(0).SlotAt(0, 1).Item := TDragItem.Create('Boots',  CAT_BOOTS,  TColor($FF8A5A3A), 'T');
  inv.Page(0).SlotAt(1, 1).Item := TDragItem.Create('Gloves', CAT_GLOVES, TColor($FF8A5A3A), 'G');
  inv.Page(0).SlotAt(2, 1).Item := TDragItem.Create('Ring',   CAT_RING,   TColor($FFC2A24A), 'R');
  inv.Page(0).SlotAt(3, 1).Item := TDragItem.Create('Amulet', CAT_AMULET, TColor($FFC2A24A), 'M');
  inv.Page(0).SlotAt(0, 2).Item := TDragItem.Create('Potion', CAT_POTION, TColor($FFA040A0), 'P');

  { equipment window: a fixed utility window, so no zoom or grow box }
  equipWin := TWindow.Create(desk, 'Equipment', [wfClose, wfDrag], 470, 110, 210, 232);
  doll := TPaperdoll.Create(equipWin.Content, 0, 0, 206, 206);
  doll.PlaceSlot(bsHead,     84,   2, 28, CAT_HELMET);
  doll.PlaceSlot(bsAmulet,   124, 30, 22, CAT_AMULET);
  doll.PlaceSlot(bsChest,    84,  50, 28, CAT_ARMOR);
  doll.PlaceSlot(bsMainHand, 28,  46, 28, CAT_WEAPON);
  doll.PlaceSlot(bsOffHand,  142, 46, 28, CAT_SHIELD or CAT_WEAPON);
  doll.PlaceSlot(bsGloves,   28,  82, 22, CAT_GLOVES);
  doll.PlaceSlot(bsRing,     146, 84, 20, CAT_RING);
  doll.PlaceSlot(bsLegs,     84,  92, 28, CAT_LEGS);
  doll.PlaceSlot(bsFeet,     84, 150, 28, CAT_BOOTS);

  { start with a couple of things equipped }
  doll.Slot(bsHead).Item     := TDragItem.Create('Helm',  CAT_HELMET, TColor($FF9A9488), 'H');
  doll.Slot(bsMainHand).Item := TDragItem.Create('Sword', CAT_WEAPON, TColor($FFB5483A), 'S');

  { a Finder-like window: a scrolling pane of freely arranged icons.          }
  { Drag an icon to move it; right-click or control-click for a context menu. }
  filesWin := TWindow.Create(desk, 'Items', [wfClose, wfDrag, wfZoom, wfGrow], 150, 330, 300, 200);
  sview := TScrollView.Create(filesWin.Content, 0, 0,
             RectW(filesWin.Content.Bounds), RectH(filesWin.Content.Bounds));
  icons := TIconView.Create(sview, RectW(sview.ViewportRect), 400);
  icons.AddIcon('Map',    TColor($FF4A6D8C), 'M',  12,  10);
  icons.AddIcon('Note',   TColor($FFC2A24A), 'N',  92,  22);
  icons.AddIcon('Key',    TColor($FFC2A24A), 'K', 180,   8);
  demoIcon :=
  icons.AddIcon('Coin',   TColor($FFC2A24A), 'C', 110,  90);
  icons.AddIcon('Gem',    TColor($FFA040A0), 'J',  20,  86);
  icons.AddIcon('Scroll', TColor($FFD9C9A8), 'S', 196,  82);
  icons.AddIcon('Rune',   TColor($FF4A6D8C), 'R',  40, 150);
  icons.AddIcon('Herb',   TColor($FF4F8C5A), 'H', 130, 160);
  icons.AddIcon('Ore',    TColor($FF9A9488), 'O', 205, 150);
  icons.AddIcon('Bone',   TColor($FFE8E0CF), 'B',  24, 228);
  icons.AddIcon('Ring',   TColor($FFC2A24A), 'G', 120, 240);
  icons.AddIcon('Wand',   TColor($FFB5483A), 'W', 200, 238);
  icons.AddIcon('Torch',  TColor($FFB5483A), 'T',  60, 320);
  icons.AddIcon('Lamp',   TColor($FFC2A24A), 'L', 170, 332);
  sview.SetContentSize(400);

  desk.BringToFront(equipWin);
  desk.BringToFront(inv);
  desk.BringToFront(filesWin);
end;

{ ---- headless screenshot ---- }

procedure RenderShot(const fn: string; withMenu: Boolean);
var surf: PSDL_Surface; ren: PSDL_Renderer; cv: TSDLCanvas;
    e: TUIEvent; a: TRect;
begin
  SDL_Init(0);
  surf := SDL_CreateRGBSurface(0, ScreenW, ScreenH, 32,
            $00FF0000, $0000FF00, $000000FF, $FF000000);
  ren := SDL_CreateSoftwareRenderer(surf);
  cv := TSDLCanvas.Create(ren);
  if withMenu then begin
    { pop the context menu open so the screenshot shows it }
    a := demoIcon.AbsRect;
    e.handled := False; e.ctrl := False; e.button := 2; e.kind := ekMouseDown;
    e.pos := Pt((a.left + a.right) div 2, (a.top + a.bottom) div 2);
    desk.Dispatch(e);
  end;
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
  SDL_RenderClear(ren);
  desk.PaintAll(cv);
  SDL_RenderPresent(ren);
  if SDL_SaveBMP(surf, PChar(fn)) = 0 then
    Writeln('wrote ', fn)
  else
    Writeln('SaveBMP failed: ', SDL_GetError);
  cv.Free;
  SDL_DestroyRenderer(ren);
  SDL_FreeSurface(surf);
  SDL_Quit;
end;

{ ---- scripted self-test: exercises the drag-and-drop path headlessly ---- }

function CenterOf(w: TWidget): TPoint;
var a: TRect;
begin
  a := w.AbsRect;
  CenterOf := Pt((a.left + a.right) div 2, (a.top + a.bottom) div 2);
end;

function CenterRect(const a: TRect): TPoint;
begin
  CenterRect := Pt((a.left + a.right) div 2, (a.top + a.bottom) div 2);
end;

procedure Click(const p: TPoint);
var e: TUIEvent;
begin
  e.handled := False; e.button := 0;
  e.kind := ekMouseDown; e.pos := p; desk.Dispatch(e);
  e.kind := ekMouseUp;   e.pos := p; desk.Dispatch(e);
end;

procedure SelfTest;
var e: TUIEvent; src, dst: TItemSlot; ok: Boolean; w0: LongInt; gc: TPoint;
begin
  ok := True;

  { 1. drag the sword onto the off-hand slot, which accepts weapons }
  src := inv.Page(0).SlotAt(0, 0);
  dst := doll.Slot(bsOffHand);
  e.handled := False; e.button := 0;
  e.kind := ekMouseDown; e.pos := CenterOf(src); desk.Dispatch(e);
  if desk.Dragging = nil then begin Writeln('FAIL: pickup'); ok := False; end;
  e.kind := ekMouseMove; e.pos := CenterOf(dst); desk.Dispatch(e);
  e.kind := ekMouseUp;   e.pos := CenterOf(dst); desk.Dispatch(e);
  if dst.Item = nil then begin Writeln('FAIL: valid drop rejected'); ok := False; end;
  if src.Item <> nil then begin Writeln('FAIL: source not cleared'); ok := False; end;

  { 2. a helm dropped on the boots slot must be refused and returned }
  src := inv.Page(0).SlotAt(2, 0);
  dst := doll.Slot(bsFeet);
  e.kind := ekMouseDown; e.pos := CenterOf(src); desk.Dispatch(e);
  e.kind := ekMouseUp;   e.pos := CenterOf(dst); desk.Dispatch(e);
  if dst.Item <> nil then begin Writeln('FAIL: wrong category accepted'); ok := False; end;
  if src.Item = nil then begin Writeln('FAIL: refused item not returned'); ok := False; end;

  { 3. the zoom box maximizes, then restores }
  w0 := RectW(inv.Bounds);
  Click(CenterRect(inv.ZoomBoxRect));
  if RectW(inv.Bounds) <= w0 then begin Writeln('FAIL: zoom did not maximize'); ok := False; end;
  Click(CenterRect(inv.ZoomBoxRect));
  if RectW(inv.Bounds) <> w0 then begin Writeln('FAIL: zoom did not restore'); ok := False; end;

  { 4. the grow box resizes the window }
  w0 := RectW(inv.Bounds);
  gc := CenterRect(inv.GrowBoxRect);
  e.handled := False; e.button := 0;
  e.kind := ekMouseDown; e.pos := gc; desk.Dispatch(e);
  e.kind := ekMouseMove; e.pos := Pt(gc.x + 60, gc.y + 40); desk.Dispatch(e);
  e.kind := ekMouseUp;   e.pos := Pt(gc.x + 60, gc.y + 40); desk.Dispatch(e);
  if RectW(inv.Bounds) <= w0 then begin Writeln('FAIL: grow did not resize'); ok := False; end;

  { 5. the scroll view scrolls on a wheel event }
  if sview.ScrollY <> 0 then begin Writeln('FAIL: scroll not at top'); ok := False; end;
  e.handled := False; e.ctrl := False; e.wheel := -3;
  e.kind := ekMouseWheel; e.pos := CenterRect(sview.ViewportRect); desk.Dispatch(e);
  if sview.ScrollY <= 0 then begin Writeln('FAIL: wheel did not scroll'); ok := False; end;
  sview.SetScrollY(0);

  { 6. right-click opens a context menu, and Delete removes the icon }
  w0 := icons.IconCount;
  gc := CenterOf(demoIcon);
  e.handled := False; e.ctrl := False; e.button := 2;
  e.kind := ekMouseDown; e.pos := gc; desk.Dispatch(e);
  e.kind := ekMouseUp;   e.pos := gc; desk.Dispatch(e);
  if not desk.PopupVisible then begin Writeln('FAIL: context menu did not open'); ok := False; end;
  { the menu opened at gc; click its third row (Delete) }
  e.handled := False; e.button := 0; e.ctrl := False;
  e.kind := ekMouseDown; e.pos := Pt(gc.x + 8, gc.y + 46); desk.Dispatch(e);
  if desk.PopupVisible then begin Writeln('FAIL: menu did not dismiss'); ok := False; end;
  if icons.IconCount <> w0 - 1 then begin Writeln('FAIL: Delete did not remove icon'); ok := False; end;

  if ok then Writeln('SELFTEST PASS') else Halt(1);
end;

{ ---- interactive loop ---- }

function MapButton(b: Byte): LongInt;
begin
  case b of
    SDL_BUTTON_MIDDLE: MapButton := 1;
    SDL_BUTTON_RIGHT:  MapButton := 2;
  else MapButton := 0;
  end;
end;

procedure RunInteractive;
var
  win: PSDL_Window; ren: PSDL_Renderer; cv: TSDLCanvas;
  ev: TSDL_Event; uie: TUIEvent; running: Boolean;
begin
  if SDL_Init(SDL_INIT_VIDEO) <> 0 then begin
    Writeln('SDL_Init failed: ', SDL_GetError); Halt(1);
  end;
  win := SDL_CreateWindow('Pascal Game GUI', SDL_WINDOWPOS_CENTERED,
           SDL_WINDOWPOS_CENTERED, ScreenW, ScreenH, SDL_WINDOW_SHOWN);
  ren := SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED or SDL_RENDERER_PRESENTVSYNC);
  cv := TSDLCanvas.Create(ren);
  running := True;
  while running do begin
    while SDL_PollEvent(@ev) <> 0 do begin
      uie.handled := False;
      case ev.etype of
        SDL_EVENT_QUIT: running := False;
        SDL_MOUSEBUTTONDOWN: begin
          uie.kind := ekMouseDown; uie.pos := Pt(ev.button.x, ev.button.y);
          uie.button := MapButton(ev.button.button);
          uie.ctrl := (SDL_GetModState and KMOD_CTRL) <> 0;
          desk.Dispatch(uie);
        end;
        SDL_MOUSEBUTTONUP: begin
          uie.kind := ekMouseUp; uie.pos := Pt(ev.button.x, ev.button.y);
          uie.button := MapButton(ev.button.button);
          uie.ctrl := (SDL_GetModState and KMOD_CTRL) <> 0;
          desk.Dispatch(uie);
        end;
        SDL_MOUSEMOTION: begin
          lastMouse := Pt(ev.motion.x, ev.motion.y);
          uie.kind := ekMouseMove; uie.pos := lastMouse;
          uie.ctrl := False; desk.Dispatch(uie);
        end;
        SDL_MOUSEWHEEL: begin
          uie.kind := ekMouseWheel; uie.pos := lastMouse;
          uie.wheel := ev.wheel.y; uie.ctrl := False;
          desk.Dispatch(uie);
        end;
        SDL_KEYDOWN:
          case ev.key.keysym.sym of
            SDLK_ESCAPE: running := False;
            SDLK_LEFT:   city.Scroll := city.Scroll - 16;
            SDLK_RIGHT:  city.Scroll := city.Scroll + 16;
          end;
      end;
    end;
    city.Scroll := city.Scroll + 1;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    desk.PaintAll(cv);
    SDL_RenderPresent(ren);
    SDL_Delay(16);
  end;
  cv.Free;
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit;
end;

begin
  theme := TSystem7Theme.Create;
  desk := TDesktop.Create(theme, ScreenW, ScreenH);
  BuildScene;
  if (ParamCount >= 2) and (ParamStr(1) = '--shot') then
    RenderShot(ParamStr(2), False)
  else if (ParamCount >= 2) and (ParamStr(1) = '--shotmenu') then
    RenderShot(ParamStr(2), True)
  else if (ParamCount >= 1) and (ParamStr(1) = '--selftest') then
    SelfTest
  else
    RunInteractive;
  desk.Free;
  theme.Free;
end.
