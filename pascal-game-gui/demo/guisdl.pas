// SPDX-License-Identifier: 0BSD OR CC0-1.0
unit guisdl;
{ The SDL2 backend: a concrete TCanvas, the System 7 theme, and a small     }
{ embedded bitmap font. This is the only part of the toolkit that knows SDL. }

{$mode objfpc}{$H+}

interface

uses
  sdl2min, gui;

type
  TSDLCanvas = class(TCanvas)
  private
    FR:     PSDL_Renderer;
    FScale: LongInt;        { pixel scale for the bitmap font }
    procedure SetColor(c: TColor);
  public
    constructor Create(renderer: PSDL_Renderer);
    procedure SetClip(const r: TRect); override;
    procedure ClearClip; override;
    procedure FillRect(const r: TRect; c: TColor); override;
    procedure FrameRect(const r: TRect; c: TColor); override;
    procedure HLine(x1, x2, y: LongInt; c: TColor); override;
    procedure VLine(x, y1, y2: LongInt; c: TColor); override;
    procedure DrawText(x, y: LongInt; const s: string; c: TColor); override;
    function  TextWidth(const s: string): LongInt; override;
    function  TextHeight: LongInt; override;
  end;

  TSystem7Theme = class(TTheme)
  public
    function  TitleBarHeight: LongInt; override;
    function  BorderWidth: LongInt; override;
    procedure DrawWindow(cv: TCanvas; w: TWindow); override;
    procedure DrawButton(cv: TCanvas; const r: TRect; const caption: string;
                         pressed, isDefault, enabled: Boolean); override;
    procedure DrawSlot(cv: TCanvas; const r: TRect; hot, filled: Boolean); override;
    procedure DrawTab(cv: TCanvas; const r: TRect; const caption: string; active: Boolean); override;
    procedure DrawPanel(cv: TCanvas; const r: TRect); override;
    procedure DrawScrollbar(cv: TCanvas; const bar, thumb: TRect); override;
  end;

implementation

{ ---- 5x7 bitmap font, codes 32..90, lowercase folded to uppercase ---- }
{ Each row's low 5 bits are pixels, leftmost column = value 16.             }
const
  FontLo = 32;
  FontHi = 90;
  Font: array[FontLo..FontHi] of array[0..6] of Byte = (
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 32 space }
    (%00100,%00100,%00100,%00100,%00100,%00000,%00100), { 33 ! }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 34 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 35 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 36 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 37 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 38 }
    (%00100,%00100,%01000,%00000,%00000,%00000,%00000), { 39 ' }
    (%00010,%00100,%01000,%01000,%01000,%00100,%00010), { 40 ( }
    (%01000,%00100,%00010,%00010,%00010,%00100,%01000), { 41 ) }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 42 }
    (%00000,%00100,%00100,%11111,%00100,%00100,%00000), { 43 + }
    (%00000,%00000,%00000,%00000,%01100,%00100,%01000), { 44 , }
    (%00000,%00000,%00000,%11111,%00000,%00000,%00000), { 45 - }
    (%00000,%00000,%00000,%00000,%00000,%01100,%01100), { 46 . }
    (%00001,%00001,%00010,%00100,%01000,%10000,%10000), { 47 / }
    (%01110,%10001,%10011,%10101,%11001,%10001,%01110), { 48 0 }
    (%00100,%01100,%00100,%00100,%00100,%00100,%01110), { 49 1 }
    (%01110,%10001,%00001,%00010,%00100,%01000,%11111), { 50 2 }
    (%11111,%00010,%00100,%00010,%00001,%10001,%01110), { 51 3 }
    (%00010,%00110,%01010,%10010,%11111,%00010,%00010), { 52 4 }
    (%11111,%10000,%11110,%00001,%00001,%10001,%01110), { 53 5 }
    (%00110,%01000,%10000,%11110,%10001,%10001,%01110), { 54 6 }
    (%11111,%00001,%00010,%00100,%01000,%01000,%01000), { 55 7 }
    (%01110,%10001,%10001,%01110,%10001,%10001,%01110), { 56 8 }
    (%01110,%10001,%10001,%01111,%00001,%00010,%01100), { 57 9 }
    (%00000,%01100,%01100,%00000,%01100,%01100,%00000), { 58 : }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 59 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 60 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 61 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 62 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 63 }
    (%00000,%00000,%00000,%00000,%00000,%00000,%00000), { 64 }
    (%01110,%10001,%10001,%11111,%10001,%10001,%10001), { 65 A }
    (%11110,%10001,%10001,%11110,%10001,%10001,%11110), { 66 B }
    (%01111,%10000,%10000,%10000,%10000,%10000,%01111), { 67 C }
    (%11110,%10001,%10001,%10001,%10001,%10001,%11110), { 68 D }
    (%11111,%10000,%10000,%11110,%10000,%10000,%11111), { 69 E }
    (%11111,%10000,%10000,%11110,%10000,%10000,%10000), { 70 F }
    (%01111,%10000,%10000,%10111,%10001,%10001,%01111), { 71 G }
    (%10001,%10001,%10001,%11111,%10001,%10001,%10001), { 72 H }
    (%11111,%00100,%00100,%00100,%00100,%00100,%11111), { 73 I }
    (%00111,%00010,%00010,%00010,%00010,%10010,%01100), { 74 J }
    (%10001,%10010,%10100,%11000,%10100,%10010,%10001), { 75 K }
    (%10000,%10000,%10000,%10000,%10000,%10000,%11111), { 76 L }
    (%10001,%11011,%10101,%10101,%10001,%10001,%10001), { 77 M }
    (%10001,%10001,%11001,%10101,%10011,%10001,%10001), { 78 N }
    (%01110,%10001,%10001,%10001,%10001,%10001,%01110), { 79 O }
    (%11110,%10001,%10001,%11110,%10000,%10000,%10000), { 80 P }
    (%01110,%10001,%10001,%10001,%10101,%10010,%01101), { 81 Q }
    (%11110,%10001,%10001,%11110,%10100,%10010,%10001), { 82 R }
    (%01111,%10000,%10000,%01110,%00001,%00001,%11110), { 83 S }
    (%11111,%00100,%00100,%00100,%00100,%00100,%00100), { 84 T }
    (%10001,%10001,%10001,%10001,%10001,%10001,%01110), { 85 U }
    (%10001,%10001,%10001,%10001,%10001,%01010,%00100), { 86 V }
    (%10001,%10001,%10001,%10101,%10101,%11011,%10001), { 87 W }
    (%10001,%10001,%01010,%00100,%01010,%10001,%10001), { 88 X }
    (%10001,%10001,%01010,%00100,%00100,%00100,%00100), { 89 Y }
    (%11111,%00001,%00010,%00100,%01000,%10000,%11111)  { 90 Z }
  );

{ ---- TSDLCanvas ---- }

constructor TSDLCanvas.Create(renderer: PSDL_Renderer);
begin
  inherited Create;
  FR := renderer;
  FScale := 2;
end;

procedure TSDLCanvas.SetColor(c: TColor);
begin
  SDL_SetRenderDrawColor(FR, (c shr 16) and $FF, (c shr 8) and $FF, c and $FF,
                         (c shr 24) and $FF);
end;

procedure TSDLCanvas.SetClip(const r: TRect);
var sr: TSDL_Rect;
begin
  sr.x := r.Left; sr.y := r.Top; sr.w := RectW(r); sr.h := RectH(r);
  SDL_RenderSetClipRect(FR, @sr);
end;

procedure TSDLCanvas.ClearClip;
begin
  SDL_RenderSetClipRect(FR, nil);
end;

procedure TSDLCanvas.FillRect(const r: TRect; c: TColor);
var sr: TSDL_Rect;
begin
  SetColor(c);
  sr.x := r.Left; sr.y := r.Top; sr.w := RectW(r); sr.h := RectH(r);
  SDL_RenderFillRect(FR, @sr);
end;

procedure TSDLCanvas.FrameRect(const r: TRect; c: TColor);
var sr: TSDL_Rect;
begin
  SetColor(c);
  sr.x := r.Left; sr.y := r.Top; sr.w := RectW(r); sr.h := RectH(r);
  SDL_RenderDrawRect(FR, @sr);
end;

procedure TSDLCanvas.HLine(x1, x2, y: LongInt; c: TColor);
begin
  SetColor(c);
  SDL_RenderDrawLine(FR, x1, y, x2, y);
end;

procedure TSDLCanvas.VLine(x, y1, y2: LongInt; c: TColor);
begin
  SetColor(c);
  SDL_RenderDrawLine(FR, x, y1, x, y2);
end;

procedure TSDLCanvas.DrawText(x, y: LongInt; const s: string; c: TColor);
var i, col, row, cx: LongInt; ch: Char; bits: Byte; px: TSDL_Rect;
begin
  SetColor(c);
  cx := x;
  for i := 1 to Length(s) do begin
    ch := UpCase(s[i]);
    if (Ord(ch) >= FontLo) and (Ord(ch) <= FontHi) then begin
      for row := 0 to 6 do begin
        bits := Font[Ord(ch)][row];
        for col := 0 to 4 do
          if (bits and (16 shr col)) <> 0 then begin
            px.x := cx + col * FScale;
            px.y := y + row * FScale;
            px.w := FScale; px.h := FScale;
            SDL_RenderFillRect(FR, @px);
          end;
      end;
    end;
    cx := cx + 6 * FScale;
  end;
end;

function TSDLCanvas.TextWidth(const s: string): LongInt;
begin
  TextWidth := Length(s) * 6 * FScale;
end;

function TSDLCanvas.TextHeight: LongInt;
begin
  TextHeight := 7 * FScale;
end;

{ ---- TSystem7Theme ---- }

function TSystem7Theme.TitleBarHeight: LongInt; begin TitleBarHeight := 24; end;
function TSystem7Theme.BorderWidth: LongInt; begin BorderWidth := 2; end;

procedure TSystem7Theme.DrawWindow(cv: TCanvas; w: TWindow);
var r, tb, kb, cb, gb: TRect; cx, tw, i, yy: LongInt;
begin
  r := w.AbsRect;
  { drop shadow }
  cv.FillRect(Rect(r.Left + 3, r.Top + 3, r.Right + 3, r.Bottom + 3), clDkGray);
  { body }
  cv.FillRect(r, clWhite);
  cv.FrameRect(r, clBlack);
  { title bar }
  tb := Rect(r.Left, r.Top, r.Right, r.Top + TitleBarHeight);
  cv.FillRect(tb, clWhite);
  cv.HLine(r.Left, r.Right, tb.Bottom, clBlack);
  { racing stripes when active }
  if w.Active then begin
    i := 0; yy := tb.Top + 5;
    while i < 6 do begin
      cv.HLine(tb.Left + 22, tb.Right - 22, yy, clBlack);
      yy := yy + 2; i := i + 1;
    end;
  end;
  { title text on a knockout }
  tw := cv.TextWidth(w.Title);
  cx := (tb.Left + tb.Right) div 2 - tw div 2;
  cv.FillRect(Rect(cx - 6, tb.Top + 3, cx + tw + 6, tb.Top + TitleBarHeight - 4), clWhite);
  cv.DrawText(cx, tb.Top + (TitleBarHeight - cv.TextHeight) div 2, w.Title, clBlack);
  { close box }
  if wfClose in w.Flags then begin
    cb := Rect(tb.Left + 6, tb.Top + 6, tb.Left + 18, tb.Top + 18);
    cv.FillRect(cb, clWhite); cv.FrameRect(cb, clBlack);
  end;
  { zoom box }
  if wfZoom in w.Flags then begin
    kb := Rect(tb.Right - 18, tb.Top + 6, tb.Right - 6, tb.Top + 18);
    cv.FillRect(kb, clWhite); cv.FrameRect(kb, clBlack);
    cv.FrameRect(Rect(kb.Left + 2, kb.Top + 2, kb.Left + 7, kb.Top + 7), clBlack);
  end;
  { grow (resize) box in the bottom-right corner }
  if wfGrow in w.Flags then begin
    gb := Rect(r.Right - 16, r.Bottom - 16, r.Right, r.Bottom);
    cv.FillRect(gb, clWhite);
    cv.FrameRect(gb, clBlack);
    for i := 0 to 2 do
      cv.FillRect(Rect(gb.Right - 5 - i * 4, gb.Bottom - 5 - i * 4,
                       gb.Right - 3 - i * 4, gb.Bottom - 3 - i * 4), clDkGray);
  end;
end;

procedure TSystem7Theme.DrawButton(cv: TCanvas; const r: TRect; const caption: string;
  pressed, isDefault, enabled: Boolean);
var cx, cy: LongInt;
begin
  if isDefault then
    cv.FrameRect(Rect(r.Left - 3, r.Top - 3, r.Right + 3, r.Bottom + 3), clBlack);
  if pressed then cv.FillRect(r, clLtGray) else cv.FillRect(r, clWhite);
  cv.FrameRect(r, clBlack);
  cx := (r.Left + r.Right) div 2 - cv.TextWidth(caption) div 2;
  cy := (r.Top + r.Bottom) div 2 - cv.TextHeight div 2;
  cv.DrawText(cx, cy, caption, clBlack);
end;

procedure TSystem7Theme.DrawSlot(cv: TCanvas; const r: TRect; hot, filled: Boolean);
begin
  cv.FillRect(r, clParch);
  { engraved look: dark top/left, light bottom/right }
  cv.HLine(r.Left, r.Right - 1, r.Top, clDkGray);
  cv.VLine(r.Left, r.Top, r.Bottom - 1, clDkGray);
  cv.HLine(r.Left, r.Right - 1, r.Bottom - 1, clGray);
  cv.VLine(r.Right - 1, r.Top, r.Bottom - 1, clGray);
end;

procedure TSystem7Theme.DrawTab(cv: TCanvas; const r: TRect; const caption: string; active: Boolean);
var cx: LongInt;
begin
  if active then cv.FillRect(r, clWhite) else cv.FillRect(r, clLtGray);
  cv.FrameRect(r, clBlack);
  cx := (r.Left + r.Right) div 2 - cv.TextWidth(caption) div 2;
  cv.DrawText(cx, r.Top + (RectH(r) - cv.TextHeight) div 2, caption, clBlack);
end;

procedure TSystem7Theme.DrawPanel(cv: TCanvas; const r: TRect);
begin
  cv.FillRect(r, clLtGray);
  cv.FrameRect(r, clBlack);
end;

procedure TSystem7Theme.DrawScrollbar(cv: TCanvas; const bar, thumb: TRect);
var up, dn: TRect;
begin
  cv.FillRect(bar, clLtGray);
  cv.FrameRect(bar, clBlack);
  up := Rect(bar.Left, bar.Top, bar.Right, bar.Top + 16);
  dn := Rect(bar.Left, bar.Bottom - 16, bar.Right, bar.Bottom);
  cv.FillRect(up, clWhite); cv.FrameRect(up, clBlack);
  cv.FillRect(dn, clWhite); cv.FrameRect(dn, clBlack);
  { arrow nubs }
  cv.FillRect(Rect(up.Left + 5, up.Top + 6, up.Right - 5, up.Top + 9), clDkGray);
  cv.FillRect(Rect(dn.Left + 5, dn.Bottom - 9, dn.Right - 5, dn.Bottom - 6), clDkGray);
  { thumb }
  cv.FillRect(thumb, clWhite);
  cv.FrameRect(thumb, clBlack);
end;

end.
