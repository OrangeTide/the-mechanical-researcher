unit nestheme;
{ The NES theme: Mega Man menu chrome, ported from rockman-redux theme_nes.c. }
{ Black panels with a bright-blue double border, a solid blue title bar, and  }
{ white text. Like guitheme it draws only through the abstract TCanvas, so it  }
{ is backend-neutral and swaps in behind the same TTheme seam.                 }

{$mode objfpc}{$H+}

interface

uses
  gui;

type
  TNesTheme = class(TTheme)
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

{ A small NES-flavored palette ($AARRGGBB). }
const
  nesBlack  = TColor($FF000000);
  nesWhite  = TColor($FFFCFCFC);
  nesGray   = TColor($FFA4A4A4);
  mmBlue    = TColor($FF0070EC);   { title bar, borders }
  mmLtBlue  = TColor($FF3CBCFC);   { highlights, active frame }
  mmDkBlue  = TColor($FF0000A8);   { inset shadow, inactive }
  mmYellow  = TColor($FFF8B800);   { default-button accent }

{ The signature double border: bright outer, dark inner. }
procedure DoubleFrame(cv: TCanvas; const r: TRect; outer, inner: TColor);
begin
  cv.FrameRect(r, outer);
  cv.FrameRect(Inset(r, 1), inner);
end;

function TNesTheme.TitleBarHeight: LongInt; begin TitleBarHeight := 24; end;
function TNesTheme.BorderWidth: LongInt; begin BorderWidth := 4; end;

procedure TNesTheme.DrawWindow(cv: TCanvas; w: TWindow);
var r, tb, cb, kb, gb: TRect; cx, tw, i: LongInt;
begin
  r := w.AbsRect;
  cv.FillRect(r, nesBlack);
  { title bar }
  tb := Rect(r.Left, r.Top, r.Right, r.Top + TitleBarHeight);
  if w.Active then cv.FillRect(tb, mmBlue) else cv.FillRect(tb, mmDkBlue);
  cv.HLine(r.Left, r.Right, tb.Bottom, mmLtBlue);
  { title text, centered }
  tw := cv.TextWidth(w.Title);
  cx := (tb.Left + tb.Right) div 2 - tw div 2;
  cv.DrawText(cx, tb.Top + (TitleBarHeight - cv.TextHeight) div 2, w.Title, nesWhite);
  { close box }
  if wfClose in w.Flags then begin
    cb := w.CloseBoxRect;
    cv.FillRect(cb, mmLtBlue); cv.FrameRect(cb, nesWhite);
  end;
  { zoom box with an inner square }
  if wfZoom in w.Flags then begin
    kb := w.ZoomBoxRect;
    cv.FillRect(kb, mmLtBlue); cv.FrameRect(kb, nesWhite);
    cv.FrameRect(Rect(kb.Left + 2, kb.Top + 2, kb.Left + 7, kb.Top + 7), nesWhite);
  end;
  { grow (resize) box with diagonal nubs }
  if wfGrow in w.Flags then begin
    gb := w.GrowBoxRect;
    cv.FillRect(gb, nesBlack); cv.FrameRect(gb, mmLtBlue);
    for i := 0 to 2 do
      cv.FillRect(Rect(gb.Right - 5 - i * 4, gb.Bottom - 5 - i * 4,
                       gb.Right - 3 - i * 4, gb.Bottom - 3 - i * 4), mmLtBlue);
  end;
  { the frame goes on last so it crowns the title bar and boxes }
  DoubleFrame(cv, r, mmLtBlue, mmDkBlue);
end;

procedure TNesTheme.DrawButton(cv: TCanvas; const r: TRect; const caption: String;
  pressed, isDefault, enabled: Boolean);
var cx, cy: LongInt; fg: TColor;
begin
  if isDefault then cv.FrameRect(Inset(r, -3), mmYellow);
  if pressed then cv.FillRect(r, mmBlue) else cv.FillRect(r, nesBlack);
  cv.FrameRect(r, nesWhite);
  if enabled then fg := nesWhite else fg := nesGray;
  cx := (r.Left + r.Right) div 2 - cv.TextWidth(caption) div 2;
  cy := (r.Top + r.Bottom) div 2 - cv.TextHeight div 2;
  cv.DrawText(cx, cy, caption, fg);
end;

procedure TNesTheme.DrawSlot(cv: TCanvas; const r: TRect; hot, filled: Boolean);
var outer: TColor;
begin
  cv.FillRect(r, nesBlack);
  if hot then outer := mmLtBlue else outer := mmBlue;
  DoubleFrame(cv, r, outer, mmDkBlue);
end;

procedure TNesTheme.DrawTab(cv: TCanvas; const r: TRect; const caption: String; active: Boolean);
var cx: LongInt; fg: TColor;
begin
  if active then begin cv.FillRect(r, mmBlue); cv.FrameRect(r, mmLtBlue); fg := nesWhite; end
  else begin cv.FillRect(r, nesBlack); cv.FrameRect(r, mmDkBlue); fg := nesGray; end;
  cx := (r.Left + r.Right) div 2 - cv.TextWidth(caption) div 2;
  cv.DrawText(cx, r.Top + (RectH(r) - cv.TextHeight) div 2, caption, fg);
end;

procedure TNesTheme.DrawPanel(cv: TCanvas; const r: TRect);
begin
  cv.FillRect(r, nesBlack);
  DoubleFrame(cv, r, mmBlue, mmDkBlue);
end;

procedure TNesTheme.DrawScrollbar(cv: TCanvas; const bar, thumb: TRect);
var up, dn: TRect;
begin
  cv.FillRect(bar, nesBlack);
  cv.FrameRect(bar, mmBlue);
  up := Rect(bar.Left, bar.Top, bar.Right, bar.Top + 16);
  dn := Rect(bar.Left, bar.Bottom - 16, bar.Right, bar.Bottom);
  cv.FillRect(up, mmDkBlue); cv.FrameRect(up, mmLtBlue);
  cv.FillRect(dn, mmDkBlue); cv.FrameRect(dn, mmLtBlue);
  { arrow nubs }
  cv.FillRect(Rect(up.Left + 5, up.Top + 6, up.Right - 5, up.Top + 9), nesWhite);
  cv.FillRect(Rect(dn.Left + 5, dn.Bottom - 9, dn.Right - 5, dn.Bottom - 6), nesWhite);
  { thumb }
  cv.FillRect(thumb, mmLtBlue);
  cv.FrameRect(thumb, nesWhite);
end;

end.
