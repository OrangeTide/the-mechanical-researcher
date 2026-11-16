// SPDX-License-Identifier: 0BSD OR CC0-1.0
unit rpgext;
{ Game-side extensions built entirely on the core: a multi-page inventory   }
{ (Neverwinter Nights style) and an equipment paperdoll (Ultima style).     }
{ Neither adds a new mechanism; both compose TItemSlot, TItemGrid, TTabBar. }

{$mode objfpc}{$H+}

interface

uses
  SysUtils, gui;

const
  { item category bits, used as slot accept masks }
  CAT_WEAPON = 1;
  CAT_SHIELD = 2;
  CAT_HELMET = 4;
  CAT_ARMOR  = 8;
  CAT_BOOTS  = 16;
  CAT_GLOVES = 32;
  CAT_RING   = 64;
  CAT_AMULET = 128;
  CAT_LEGS   = 256;
  CAT_POTION = 512;
  CAT_MISC   = 1024;

type
  { ---- multi-page inventory ---- }
  TInventoryWindow = class(TWindow)
  private
    FTabs:  TTabBar;
    FPages: array of TItemGrid;
    procedure HandleTab(index: LongInt);
  public
    constructor Create(AParent: TWidget; l, t: LongInt);
    procedure ShowPage(i: LongInt);
    function  Page(i: LongInt): TItemGrid;
  end;

  { ---- equipment paperdoll ---- }
  TBodySlot = (bsHead, bsAmulet, bsChest, bsMainHand, bsOffHand,
               bsGloves, bsLegs, bsFeet, bsRing);

  TPaperdoll = class(TWidget)
  private
    FSlots: array[TBodySlot] of TItemSlot;
  public
    constructor Create(AParent: TWidget; l, t, w, h: LongInt);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
    procedure PlaceSlot(s: TBodySlot; l, t, sz, accept: LongInt);
    function  Slot(s: TBodySlot): TItemSlot;
  end;

implementation

{ ---- TInventoryWindow ---- }

constructor TInventoryWindow.Create(AParent: TWidget; l, t: LongInt);
var i: LongInt;
begin
  inherited Create(AParent, 'Inventory', [wfClose, wfDrag, wfZoom, wfGrow], l, t, 264, 230);
  FTabs := TTabBar.Create(Content, 6, 4, 58, 20);
  FTabs.OnChange := @HandleTab;
  SetLength(FPages, 4);
  for i := 0 to 3 do begin
    FTabs.AddTab('Bag' + IntToStr(i + 1));
    FPages[i] := TItemGrid.Create(Content, 6, 4, 0, 40, 6, 30);
    FPages[i].Visible := (i = 0);
  end;
end;

procedure TInventoryWindow.HandleTab(index: LongInt);
begin
  ShowPage(index);
end;

procedure TInventoryWindow.ShowPage(i: LongInt);
var k: LongInt;
begin
  for k := 0 to High(FPages) do
    FPages[k].Visible := (k = i);
end;

function TInventoryWindow.Page(i: LongInt): TItemGrid;
begin
  Page := FPages[i];
end;

{ ---- TPaperdoll ---- }

constructor TPaperdoll.Create(AParent: TWidget; l, t, w, h: LongInt);
var s: TBodySlot;
begin
  inherited Create(AParent);
  SetBounds(l, t, l + w, t + h);
  for s := Low(TBodySlot) to High(TBodySlot) do FSlots[s] := nil;
end;

procedure TPaperdoll.PlaceSlot(s: TBodySlot; l, t, sz, accept: LongInt);
begin
  FSlots[s] := TItemSlot.Create(Self, accept, l, t, sz);
end;

function TPaperdoll.Slot(s: TBodySlot): TItemSlot;
begin
  Slot := FSlots[s];
end;

procedure TPaperdoll.Draw(cv: TCanvas; theme: TTheme);
var r: TRect; cx, cy: LongInt; skin, line: TColor;
begin
  r := AbsRect;
  cv.FillRect(r, clWhite);
  skin := TColor($FFD9C9A8);
  line := TColor($FF8A764F);
  cx := (r.left + r.right) div 2;
  cy := r.top + 20;
  { blocky figure: head, torso, arms, legs }
  cv.FillRect(Rect(cx - 12, cy, cx + 12, cy + 22), skin);        { head }
  cv.FrameRect(Rect(cx - 12, cy, cx + 12, cy + 22), line);
  cv.FillRect(Rect(cx - 20, cy + 24, cx + 20, cy + 70), skin);   { torso }
  cv.FrameRect(Rect(cx - 20, cy + 24, cx + 20, cy + 70), line);
  cv.FillRect(Rect(cx - 34, cy + 26, cx - 20, cy + 64), skin);   { left arm }
  cv.FrameRect(Rect(cx - 34, cy + 26, cx - 20, cy + 64), line);
  cv.FillRect(Rect(cx + 20, cy + 26, cx + 34, cy + 64), skin);   { right arm }
  cv.FrameRect(Rect(cx + 20, cy + 26, cx + 34, cy + 64), line);
  cv.FillRect(Rect(cx - 16, cy + 70, cx - 2, cy + 118), skin);   { left leg }
  cv.FrameRect(Rect(cx - 16, cy + 70, cx - 2, cy + 118), line);
  cv.FillRect(Rect(cx + 2, cy + 70, cx + 16, cy + 118), skin);   { right leg }
  cv.FrameRect(Rect(cx + 2, cy + 70, cx + 16, cy + 118), line);
end;

end.
