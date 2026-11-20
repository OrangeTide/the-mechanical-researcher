program demo_native;
{ The native Pascal host: it uses the core directly with `uses gui`, no FFI.   }
{ This is the home language. It builds a scene with the paperdoll and paged     }
{ inventory from richwidgets, proving an extension can meet chasm's richness on }
{ the shipped core.                                                            }
{                                                                              }
{   demo_native              open a window and run (needs a display)           }
{   demo_native --shot f     render one System 7 frame headless to BMP f        }
{   demo_native --shot-nes f render one NES-themed frame headless to BMP f       }
{   demo_native --selftest   drive drag-and-drop headless and check it          }

{$mode objfpc}{$H+}

uses
  SysUtils, sdl2min, gui, guitheme, nestheme, guisdl, richwidgets;

const
  ScreenW = 760; ScreenH = 520;

var
  desk:  TDesktop;
  doll:  TPaperdoll;
  inv:   TPagedInventory;
  invWin, eqWin: TWindow;
  curTheme: TTheme;      { the theme object the desktop currently owns here }
  curThemeId: LongInt;   { 0 = System 7, 1 = NES }

procedure BuildScene(theme: TTheme);
var g: TItemGrid; i: LongInt; it: TDragItem;
    names: array[0..3] of String; cats: array[0..3] of LongInt;
begin
  curTheme := theme;
  desk := TDesktop.Create(theme, ScreenW, ScreenH);

  invWin := TWindow.Create(desk, 'Inventory', [wfClose, wfDrag, wfZoom], 40, 40, 300, 280);
  inv := TPagedInventory.Create(invWin.Content, 3, 4, 3, 0, 0);

  { seed page 0 with a few draggable items of distinct categories }
  names[0] := 'Sword'; cats[0] := catWeapon;
  names[1] := 'Helm';  cats[1] := catHelm;
  names[2] := 'Mail';  cats[2] := catArmor;
  names[3] := 'Boots'; cats[3] := catBoots;
  g := inv.Page(0);
  for i := 0 to 3 do begin
    it := TDragItem.Create(names[i], cats[i], TColor($FF3366CC + LongWord(i) * $001A00), names[i][1]);
    it.Tag := i;
    g.SlotAt(i, 0).Item := it;
  end;

  eqWin := TWindow.Create(desk, 'Equipment', [wfClose, wfDrag], 400, 40, 300, 300);
  doll := TPaperdoll.Create(eqWin.Content, 0, 0);

  desk.BringToFront(eqWin);
  desk.BringToFront(invWin);
end;

function CenterOf(w: TWidget): TPoint;
var r: TRect;
begin
  r := w.AbsRect;
  CenterOf.X := (r.Left + r.Right) div 2;
  CenterOf.Y := (r.Top + r.Bottom) div 2;
end;

procedure DragFromTo(src, dst: TWidget);
var e: TUIEvent; a, b: TPoint;
begin
  a := CenterOf(src); b := CenterOf(dst);
  e.Handled := False; e.Ctrl := False; e.Wheel := 0; e.Key := 0; e.Button := 0;
  e.Kind := ekMouseDown; e.Pos := a; desk.Dispatch(e);
  e.Kind := ekMouseMove; e.Pos.X := (a.X + b.X) div 2; e.Pos.Y := (a.Y + b.Y) div 2; desk.Dispatch(e);
  e.Kind := ekMouseMove; e.Pos := b; desk.Dispatch(e);
  e.Kind := ekMouseUp;   e.Pos := b; desk.Dispatch(e);
end;

procedure RenderShot(const fn: String);
var surf: PSDL_Surface; ren: PSDL_Renderer; cv: TSDLCanvas;
begin
  SDL_Init(0);
  surf := SDL_CreateRGBSurface(0, ScreenW, ScreenH, 32, $00FF0000, $0000FF00, $000000FF, $FF000000);
  ren := SDL_CreateSoftwareRenderer(surf);
  cv := TSDLCanvas.Create(ren);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
  SDL_RenderClear(ren);
  desk.PaintAll(cv);
  SDL_RenderPresent(ren);
  if SDL_SaveBMP(surf, PChar(fn)) = 0 then Writeln('wrote ', fn)
  else Writeln('SaveBMP failed: ', SDL_GetError);
  cv.Free; SDL_DestroyRenderer(ren); SDL_FreeSurface(surf); SDL_Quit;
end;

procedure ToggleTheme; forward;

function SelfTest: Boolean;
var g: TItemGrid; ok: Boolean;
begin
  BuildScene(TSystem7Theme.Create);
  g := inv.Page(0);
  ok := True;

  { a sword (catWeapon) should move into the main-hand slot }
  DragFromTo(g.SlotAt(0, 0), doll.MainHand);
  ok := ok and (doll.MainHand.Item <> nil) and (g.SlotAt(0, 0).Item = nil);
  Writeln('weapon accepted into main hand: ', (doll.MainHand.Item <> nil));

  { a helm (catHelm) dropped on the main-hand slot is refused and stays put }
  DragFromTo(g.SlotAt(1, 0), doll.MainHand);
  ok := ok and (g.SlotAt(1, 0).Item <> nil);
  Writeln('helm refused by main hand, returned to source: ', (g.SlotAt(1, 0).Item <> nil));

  { the same helm goes into the head slot }
  DragFromTo(g.SlotAt(1, 0), doll.Head);
  ok := ok and (doll.Head.Item <> nil) and (g.SlotAt(1, 0).Item = nil);
  Writeln('helm accepted into head: ', (doll.Head.Item <> nil));

  { run-time theme swap: System 7 -> NES, in place }
  ToggleTheme;
  ok := ok and (curThemeId = 1) and (desk.Theme is TNesTheme);
  Writeln('run-time theme swap: ', (desk.Theme is TNesTheme));

  SelfTest := ok;
end;

{ Swap the theme in place, the native way: build the other theme, hand it to    }
{ the desktop, and free the one it replaced. The next PaintAll uses the new one. }
procedure ToggleTheme;
var old, fresh: TTheme;
begin
  old := desk.Theme;
  if curThemeId = 1 then begin fresh := TSystem7Theme.Create; curThemeId := 0; end
  else begin fresh := TNesTheme.Create; curThemeId := 1; end;
  desk.SetTheme(fresh);
  curTheme := fresh;
  old.Free;
end;

procedure RunInteractive;
var win: PSDL_Window; ren: PSDL_Renderer; cv: TSDLCanvas;
    e: TSDL_Event; ue: TUIEvent; running: Boolean;
begin
  if SDL_Init(SDL_INIT_VIDEO) <> 0 then begin
    Writeln('SDL_Init failed: ', SDL_GetError); Halt(1);
  end;
  BuildScene(TNesTheme.Create); curThemeId := 1;
  win := SDL_CreateWindow('Pascal Game GUI v2 (native)', SDL_WINDOWPOS_CENTERED,
           SDL_WINDOWPOS_CENTERED, ScreenW, ScreenH, SDL_WINDOW_SHOWN);
  ren := SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED or SDL_RENDERER_PRESENTVSYNC);
  cv := TSDLCanvas.Create(ren);
  Writeln('press T to toggle the theme, close the window to quit');
  running := True;
  while running do begin
    while SDL_PollEvent(@e) <> 0 do begin
      case e.etype of
        SDL_EVENT_QUIT: running := False;
        SDL_KEYDOWN:
          if (e.key.keysym.sym = 116) or (e.key.keysym.sym = 84) then ToggleTheme;
        SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP: begin
          if e.etype = SDL_MOUSEBUTTONDOWN then ue.Kind := ekMouseDown else ue.Kind := ekMouseUp;
          ue.Pos.X := e.button.x; ue.Pos.Y := e.button.y;
          if e.button.button = SDL_BUTTON_RIGHT then ue.Button := 2 else ue.Button := 0;
          ue.Ctrl := False; ue.Wheel := 0; ue.Key := 0; ue.Handled := False;
          desk.Dispatch(ue);
        end;
        SDL_MOUSEMOTION: begin
          ue.Kind := ekMouseMove; ue.Pos.X := e.motion.x; ue.Pos.Y := e.motion.y;
          ue.Button := 0; ue.Ctrl := False; ue.Wheel := 0; ue.Key := 0; ue.Handled := False;
          desk.Dispatch(ue);
        end;
      end;
    end;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    desk.PaintAll(cv);
    SDL_RenderPresent(ren);
    SDL_Delay(16);
  end;
  cv.Free; SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
  curTheme.Free; desk.Free; SDL_Quit;
end;

var
  mode, fn: String;
begin
  if (ParamCount >= 1) and (ParamStr(1) = '--selftest') then begin
    if SelfTest then begin Writeln('SELFTEST PASS'); Halt(0); end
    else begin Writeln('SELFTEST FAIL'); Halt(1); end;
  end;

  if (ParamCount >= 2) and ((ParamStr(1) = '--shot') or (ParamStr(1) = '--shot-nes')) then begin
    mode := ParamStr(1); fn := ParamStr(2);
    if mode = '--shot-nes' then BuildScene(TNesTheme.Create)
    else BuildScene(TSystem7Theme.Create);
    RenderShot(fn);
    Halt(0);
  end;

  { interactive: a live window, press T to toggle the theme }
  RunInteractive;
end.
