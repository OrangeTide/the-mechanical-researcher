unit guitheme;
{ The System 7 theme: all window and widget chrome.                         }
{ It draws only through the abstract TCanvas, so it is backend-neutral and  }
{ works with any canvas, the OpenGL one included.                           }

{$mode objfpc}{$H+}

interface

uses
  gui;

type
  TSystem7Theme = class(TTheme)
  public
    function  TitleBarHeight: LongInt; override;
    function  BorderWidth: LongInt; override;
    procedure DrawWindow(cv: TCanvas; w: TWindow); override;
    procedure DrawButton(cv: TCanvas; const r: TRect; const caption: String;
                         pressed, isDefault, enabled: Boolean); override;
    procedure DrawSlot(cv: TCanvas; const r: TRect; hot, filled: Boolean); override;
    procedure DrawTab(cv: TCanvas; const r: TRect; const caption: String; active: Boolean); override;
    procedure DrawPanel(cv: TCanvas; const r: TRect); override;
    procedure DrawScrollbar(cv: TCanvas; const bar, thumb: TRect); override;
  end;

implementation

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

procedure TSystem7Theme.DrawButton(cv: TCanvas; const r: TRect; const caption: String;
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

procedure TSystem7Theme.DrawTab(cv: TCanvas; const r: TRect; const caption: String; active: Boolean);
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
