unit richwidgets;
{ Proof that the core carries chasm-grade richness as an extension, not as new }
{ core code. A paperdoll and a paged inventory, built only from core TItemSlot, }
{ TItemGrid, TTabBar and the one TDragItem drag protocol. Nothing here needed a }
{ change to gui.pas; both are compositions of parts the core already ships.    }

{$mode objfpc}{$H+}

interface

uses
  gui;

const
  { item category bits, used as slot accept masks }
  catWeapon = 1;
  catShield = 2;
  catHelm   = 4;
  catArmor  = 8;
  catBoots  = 16;
  catAny    = 0;   { a bag slot accepts anything }

type
  { An Ultima-style equipment doll: a blocky figure with category-locked slots }
  { placed over it. Each slot is a plain core TItemSlot with an accept mask.    }
  TPaperdoll = class(TWidget)
  public
    Head, Chest, MainHand, OffHand, Feet: TItemSlot;
    constructor Create(AParent: TWidget; l, t: LongInt);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
  end;

  { A Neverwinter Nights-style multi-page inventory: a tab bar over several     }
  { item grids, one grid per page. The tab bar's OnChange shows the active grid }
  { and hides the rest. Pure composition of TTabBar and TItemGrid.             }
  TPagedInventory = class(TWidget)
  private
    FPages: array of TItemGrid;
    FTabs:  TTabBar;
    FActive: LongInt;
    procedure TabChanged(index: LongInt);
    procedure ShowPage(i: LongInt);
  public
    constructor Create(AParent: TWidget; pages, cols, rows: LongInt; l, t: LongInt);
    function  Page(i: LongInt): TItemGrid;
    property  ActivePage: LongInt read FActive;
  end;

implementation

{ ---- TPaperdoll ---- }

constructor TPaperdoll.Create(AParent: TWidget; l, t: LongInt);
const sz = 40;
begin
  inherited Create(AParent);
  SetBounds(l, t, l + 180, t + 220);
  { positions are relative to this widget's parent, like any child }
  Head     := TItemSlot.Create(Self, catHelm,   l + 70,  t + 4,   sz);
  Chest    := TItemSlot.Create(Self, catArmor,  l + 70,  t + 50,  sz);
  MainHand := TItemSlot.Create(Self, catWeapon, l + 16,  t + 60,  sz);
  OffHand  := TItemSlot.Create(Self, catShield, l + 124, t + 60,  sz);
  Feet     := TItemSlot.Create(Self, catBoots,  l + 70,  t + 150, sz);
end;

procedure TPaperdoll.Draw(cv: TCanvas; theme: TTheme);
var r: TRect; cxl: LongInt;
begin
  { a simple humanoid silhouette behind the equipment slots }
  r := AbsRect;
  cxl := (r.Left + r.Right) div 2;
  cv.FillRect(Rect(cxl - 10, r.Top + 6,  cxl + 10, r.Top + 26),  clGray);   { head }
  cv.FillRect(Rect(cxl - 18, r.Top + 30, cxl + 18, r.Top + 110), clGray);   { torso }
  cv.FillRect(Rect(cxl - 34, r.Top + 34, cxl - 18, r.Top + 96),  clGray);   { left arm }
  cv.FillRect(Rect(cxl + 18, r.Top + 34, cxl + 34, r.Top + 96),  clGray);   { right arm }
  cv.FillRect(Rect(cxl - 16, r.Top + 110, cxl - 2, r.Top + 190), clGray);   { left leg }
  cv.FillRect(Rect(cxl + 2,  r.Top + 110, cxl + 16, r.Top + 190), clGray);  { right leg }
end;

{ ---- TPagedInventory ---- }

constructor TPagedInventory.Create(AParent: TWidget; pages, cols, rows: LongInt; l, t: LongInt);
var i: LongInt; names: array[0..3] of String;
begin
  inherited Create(AParent);
  SetBounds(l, t, l + cols * 44 + 24, t + rows * 44 + 40);
  names[0] := 'Bag'; names[1] := 'Quest'; names[2] := 'Keys'; names[3] := 'Misc';
  FTabs := TTabBar.Create(Self, l + 8, t + 6, 60, 22);
  SetLength(FPages, pages);
  for i := 0 to pages - 1 do begin
    if i <= High(names) then FTabs.AddTab(names[i]) else FTabs.AddTab('Tab');
    FPages[i] := TItemGrid.Create(Self, cols, rows, catAny, 44, l + 8, t + 34);
  end;
  FTabs.OnChange := @TabChanged;
  FActive := 0;
  ShowPage(0);
end;

procedure TPagedInventory.ShowPage(i: LongInt);
var k: LongInt;
begin
  for k := 0 to High(FPages) do
    FPages[k].Visible := (k = i);
  FActive := i;
end;

procedure TPagedInventory.TabChanged(index: LongInt);
begin
  ShowPage(index);
end;

function TPagedInventory.Page(i: LongInt): TItemGrid;
begin
  if (i >= 0) and (i <= High(FPages)) then Page := FPages[i] else Page := nil;
end;

end.
