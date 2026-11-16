// SPDX-License-Identifier: 0BSD OR CC0-1.0
unit iconview;
{ A Finder-like extension: a surface of freely arranged icons. Each icon is  }
{ dragged to any position, and a right-click or control-click opens a context }
{ menu. Built on the core: it is a container of draggable icon widgets plus   }
{ the desktop's popup menu. Drop it inside a TScrollView for a scrolling pane. }

{$mode objfpc}{$H+}

interface

uses
  gui;

type
  TIconView = class;

  TDeskIcon = class(TWidget)
  private
    FView:   TIconView;
    FMoving: Boolean;
    FGrabDX, FGrabDY: LongInt;
  public
    Caption: string;
    Color:   TColor;
    Glyph:   Char;
    constructor Create(AView: TIconView; const ALabel: string;
                       AColor: TColor; AGlyph: Char; ax, ay: LongInt);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
    procedure OnMouseDown(var e: TUIEvent); override;
    procedure OnMouseMove(var e: TUIEvent); override;
    procedure OnMouseUp(var e: TUIEvent); override;
  end;

  TIconView = class(TWidget)
  private
    FMenuTarget: TDeskIcon;
    procedure HandlePopup(index: LongInt);
  public
    constructor Create(AParent: TWidget; w, h: LongInt);
    function  AddIcon(const ALabel: string; AColor: TColor; AGlyph: Char;
                      ax, ay: LongInt): TDeskIcon;
    function  IconCount: LongInt;
    procedure OpenMenuFor(icon: TDeskIcon; at: TPoint);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
  end;

const
  IconW = 60;
  IconH = 54;

implementation

{ ---- TDeskIcon ---- }

constructor TDeskIcon.Create(AView: TIconView; const ALabel: string;
  AColor: TColor; AGlyph: Char; ax, ay: LongInt);
begin
  inherited Create(AView);
  FView := AView;
  Caption := ALabel;
  Color := AColor;
  Glyph := AGlyph;
  SetBounds(ax, ay, ax + IconW, ay + IconH);
end;

procedure TDeskIcon.Draw(cv: TCanvas; theme: TTheme);
var r, tile: TRect; cx: LongInt;
begin
  r := AbsRect;
  cx := (r.Left + r.Right) div 2;
  tile := Rect(cx - 16, r.Top + 2, cx + 16, r.Top + 34);
  cv.FillRect(tile, Color);
  cv.FrameRect(tile, clBlack);
  cv.DrawText(cx - cv.TextWidth(Glyph) div 2,
              (tile.Top + tile.Bottom) div 2 - cv.TextHeight div 2, Glyph, clBlack);
  cv.DrawText(cx - cv.TextWidth(Caption) div 2, r.Top + 38, Caption, clBlack);
end;

procedure TDeskIcon.OnMouseDown(var e: TUIEvent);
var a: TRect;
begin
  if (e.Button = 2) or e.Ctrl then begin
    FView.OpenMenuFor(Self, e.Pos);
    exit;
  end;
  FMoving := True;
  a := AbsRect;
  FGrabDX := e.Pos.X - a.Left;
  FGrabDY := e.Pos.Y - a.Top;
end;

procedure TDeskIcon.OnMouseMove(var e: TUIEvent);
var pa: TRect; nx, ny, w, h, cw, ch: LongInt;
begin
  if not FMoving then exit;
  pa := FParent.AbsRect;
  w := RectW(FBounds); h := RectH(FBounds);
  cw := RectW(FParent.Bounds); ch := RectH(FParent.Bounds);
  nx := (e.Pos.X - FGrabDX) - pa.Left;
  ny := (e.Pos.Y - FGrabDY) - pa.Top;
  if nx < 0 then nx := 0;
  if ny < 0 then ny := 0;
  if nx + w > cw then nx := cw - w;
  if ny + h > ch then ny := ch - h;
  SetBounds(nx, ny, nx + w, ny + h);
end;

procedure TDeskIcon.OnMouseUp(var e: TUIEvent);
begin
  FMoving := False;
end;

{ ---- TIconView ---- }

constructor TIconView.Create(AParent: TWidget; w, h: LongInt);
begin
  inherited Create(AParent);
  SetBounds(0, 0, w, h);
end;

function TIconView.AddIcon(const ALabel: string; AColor: TColor; AGlyph: Char;
  ax, ay: LongInt): TDeskIcon;
begin
  AddIcon := TDeskIcon.Create(Self, ALabel, AColor, AGlyph, ax, ay);
end;

function TIconView.IconCount: LongInt;
begin
  IconCount := ChildCount;
end;

procedure TIconView.OpenMenuFor(icon: TDeskIcon; at: TPoint);
begin
  FMenuTarget := icon;
  Root.ShowPopup(['Open', 'Duplicate', 'Delete'], at, @HandlePopup);
end;

procedure TIconView.HandlePopup(index: LongInt);
begin
  if FMenuTarget = nil then exit;
  case index of
    0: { Open: a real app would act here; the demo does nothing }
       ;
    1: AddIcon(FMenuTarget.Caption, FMenuTarget.Color, FMenuTarget.Glyph,
               FMenuTarget.Bounds.Left + 18, FMenuTarget.Bounds.Top + 18);
    2: begin
         RemoveChild(FMenuTarget);
         FMenuTarget.Free;      { safe: HandlePopup is a method of the view, not the icon }
       end;
  end;
  FMenuTarget := nil;
end;

procedure TIconView.Draw(cv: TCanvas; theme: TTheme);
begin
  cv.FillRect(AbsRect, clWhite);
end;

end.
