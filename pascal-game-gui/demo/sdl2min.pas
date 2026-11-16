// SPDX-License-Identifier: 0BSD OR CC0-1.0
unit sdl2min;
{ Minimal SDL2 binding: only the calls the GUI demo needs. }
{ Vendored so the demo depends on libsdl2-dev alone, not an external FPC binding. }

{$mode objfpc}{$H+}
{$PACKRECORDS C}

interface

const
  SDL_INIT_VIDEO = $00000020;

  SDL_WINDOWPOS_CENTERED = $2FFF0000;
  SDL_WINDOW_SHOWN       = $00000004;

  SDL_RENDERER_SOFTWARE     = $00000001;
  SDL_RENDERER_ACCELERATED  = $00000002;
  SDL_RENDERER_PRESENTVSYNC = $00000004;

  { event types }
  SDL_EVENT_QUIT      = $100;
  SDL_KEYDOWN         = $300;
  SDL_KEYUP           = $301;
  SDL_MOUSEMOTION     = $400;
  SDL_MOUSEBUTTONDOWN = $401;
  SDL_MOUSEBUTTONUP   = $402;
  SDL_MOUSEWHEEL      = $403;

  { a few keycodes }
  SDLK_ESCAPE = 27;
  SDLK_RIGHT  = $4000004F;
  SDLK_LEFT   = $40000050;
  SDLK_DOWN   = $40000051;
  SDLK_UP     = $40000052;

  { mouse buttons }
  SDL_BUTTON_LEFT   = 1;
  SDL_BUTTON_MIDDLE = 2;
  SDL_BUTTON_RIGHT  = 3;

  { key modifiers }
  KMOD_LCTRL = $0040;
  KMOD_RCTRL = $0080;
  KMOD_CTRL  = $00C0;

type
  UInt8  = Byte;
  UInt16 = Word;
  UInt32 = LongWord;
  Sint32 = LongInt;

  PSDL_Window   = Pointer;
  PSDL_Renderer = Pointer;
  PSDL_Surface  = Pointer;
  PSDL_RWops    = Pointer;

  PSDL_Rect = ^TSDL_Rect;
  TSDL_Rect = record
    x, y, w, h: Sint32;
  end;

  TSDL_Keysym = record
    scancode: Sint32;
    sym:      Sint32;
    mods:     UInt16;
    unused:   UInt32;
  end;

  TSDL_KeyboardEvent = record
    etype:     UInt32;
    timestamp: UInt32;
    windowID:  UInt32;
    state:     UInt8;
    rep:       UInt8;
    padding2:  UInt8;
    padding3:  UInt8;
    keysym:    TSDL_Keysym;
  end;

  TSDL_MouseButtonEvent = record
    etype:     UInt32;
    timestamp: UInt32;
    windowID:  UInt32;
    which:     UInt32;
    button:    UInt8;
    state:     UInt8;
    clicks:    UInt8;
    padding1:  UInt8;
    x:         Sint32;
    y:         Sint32;
  end;

  TSDL_MouseMotionEvent = record
    etype:     UInt32;
    timestamp: UInt32;
    windowID:  UInt32;
    which:     UInt32;
    state:     UInt32;
    x:         Sint32;
    y:         Sint32;
    xrel:      Sint32;
    yrel:      Sint32;
  end;

  TSDL_MouseWheelEvent = record
    etype:     UInt32;
    timestamp: UInt32;
    windowID:  UInt32;
    which:     UInt32;
    x:         Sint32;
    y:         Sint32;
    direction: UInt32;
  end;

  PSDL_Event = ^TSDL_Event;
  TSDL_Event = record
    case LongInt of
      0: (etype:  UInt32);
      1: (key:    TSDL_KeyboardEvent);
      2: (button: TSDL_MouseButtonEvent);
      3: (motion: TSDL_MouseMotionEvent);
      4: (wheel:  TSDL_MouseWheelEvent);
      5: (padding: array[0..55] of Byte);
  end;

function  SDL_Init(flags: UInt32): Sint32; cdecl; external 'SDL2';
procedure SDL_Quit; cdecl; external 'SDL2';
function  SDL_GetError: PChar; cdecl; external 'SDL2';

function  SDL_CreateWindow(title: PChar; x, y, w, h: Sint32; flags: UInt32): PSDL_Window; cdecl; external 'SDL2';
procedure SDL_DestroyWindow(w: PSDL_Window); cdecl; external 'SDL2';

function  SDL_CreateRenderer(win: PSDL_Window; index: Sint32; flags: UInt32): PSDL_Renderer; cdecl; external 'SDL2';
function  SDL_CreateSoftwareRenderer(surface: PSDL_Surface): PSDL_Renderer; cdecl; external 'SDL2';
procedure SDL_DestroyRenderer(r: PSDL_Renderer); cdecl; external 'SDL2';

function  SDL_CreateRGBSurface(flags: UInt32; w, h, depth: Sint32;
            Rmask, Gmask, Bmask, Amask: UInt32): PSDL_Surface; cdecl; external 'SDL2';
procedure SDL_FreeSurface(s: PSDL_Surface); cdecl; external 'SDL2';

function  SDL_RWFromFile(afile, mode: PChar): PSDL_RWops; cdecl; external 'SDL2';
function  SDL_SaveBMP_RW(surface: PSDL_Surface; dst: PSDL_RWops; freedst: Sint32): Sint32; cdecl; external 'SDL2';

function  SDL_SetRenderDrawColor(r: PSDL_Renderer; red, green, blue, alpha: UInt8): Sint32; cdecl; external 'SDL2';
function  SDL_RenderClear(r: PSDL_Renderer): Sint32; cdecl; external 'SDL2';
function  SDL_RenderFillRect(r: PSDL_Renderer; rect: PSDL_Rect): Sint32; cdecl; external 'SDL2';
function  SDL_RenderDrawRect(r: PSDL_Renderer; rect: PSDL_Rect): Sint32; cdecl; external 'SDL2';
function  SDL_RenderDrawLine(r: PSDL_Renderer; x1, y1, x2, y2: Sint32): Sint32; cdecl; external 'SDL2';
function  SDL_RenderSetClipRect(r: PSDL_Renderer; rect: PSDL_Rect): Sint32; cdecl; external 'SDL2';
procedure SDL_RenderPresent(r: PSDL_Renderer); cdecl; external 'SDL2';

function  SDL_PollEvent(event: PSDL_Event): Sint32; cdecl; external 'SDL2';
function  SDL_GetModState: UInt32; cdecl; external 'SDL2';
procedure SDL_Delay(ms: UInt32); cdecl; external 'SDL2';
function  SDL_GetTicks: UInt32; cdecl; external 'SDL2';

{ SDL_SaveBMP is a C macro; provide it as a helper. }
function SDL_SaveBMP(surface: PSDL_Surface; afile: PChar): Sint32;

implementation

function SDL_SaveBMP(surface: PSDL_Surface; afile: PChar): Sint32;
begin
  SDL_SaveBMP := SDL_SaveBMP_RW(surface, SDL_RWFromFile(afile, 'wb'), 1);
end;

end.
