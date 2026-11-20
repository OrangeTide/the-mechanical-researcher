unit gui;
{ Retained-mode GUI toolkit core, backend-agnostic.                        }
{ Floating System 7 style windows over a live game canvas. Draws through an }
{ abstract TCanvas and an abstract TTheme, so it knows nothing about the    }
{ renderer. guigl provides the OpenGL canvas, guitheme the System 7 chrome. }

{$mode objfpc}{$H+}

interface

uses
  Classes;   { for TFPList }

type
  TColor = LongWord;   { $AARRGGBB }

  TPoint = record X, Y: LongInt; end;
  TRect  = record Left, Top, Right, Bottom: LongInt; end;

  TEventKind = (ekMouseDown, ekMouseUp, ekMouseMove, ekMouseWheel,
                ekKeyDown, ekKeyUp);

  TUIEvent = record
    Kind:    TEventKind;
    Pos:     TPoint;      { mouse position, screen coordinates }
    Button:  LongInt;     { 0 = left, 1 = middle, 2 = right }
    Key:     LongInt;     { virtual key code }
    Wheel:   LongInt;     { signed wheel delta }
    Ctrl:    Boolean;     { control key held (for control-click) }
    Handled: Boolean;
  end;

  { ---- a draggable item: the game-defined payload ---- }
  TDragItem = class
  public
    Name:     String;
    Category: LongInt;    { a single category bit, for drop validation }
    Color:    TColor;     { tile colour, stands in for item art }
    Glyph:    Char;       { letter drawn on the tile }
    Tag:      LongInt;    { caller payload, e.g. a content index; -1 when unset }
    constructor Create(const AName: String; ACat: LongInt; AColor: TColor; AGlyph: Char);
  end;

  { ---- the drawing seam ---- }
  TCanvas = class
  public
    procedure SetClip(const r: TRect); virtual; abstract;
    procedure ClearClip; virtual; abstract;
    procedure FillRect(const r: TRect; c: TColor); virtual; abstract;
    procedure FrameRect(const r: TRect; c: TColor); virtual; abstract;
    procedure HLine(x1, x2, y: LongInt; c: TColor); virtual; abstract;
    procedure VLine(x, y1, y2: LongInt; c: TColor); virtual; abstract;
    procedure DrawText(x, y: LongInt; const s: String; c: TColor); virtual; abstract;
    function  TextWidth(const s: String): LongInt; virtual; abstract;
    function  TextHeight: LongInt; virtual; abstract;
  end;

  TWidget = class;
  TWindow = class;

  { ---- the theme seam: all chrome is drawn here ---- }
  TTheme = class
  public
    function  TitleBarHeight: LongInt; virtual; abstract;
    function  BorderWidth: LongInt; virtual; abstract;
    procedure DrawWindow(cv: TCanvas; w: TWindow); virtual; abstract;
    procedure DrawButton(cv: TCanvas; const r: TRect; const caption: String;
                         pressed, isDefault, enabled: Boolean); virtual; abstract;
    procedure DrawSlot(cv: TCanvas; const r: TRect; hot, filled: Boolean); virtual; abstract;
    procedure DrawTab(cv: TCanvas; const r: TRect; const caption: String; active: Boolean); virtual; abstract;
    procedure DrawPanel(cv: TCanvas; const r: TRect); virtual; abstract;
    procedure DrawScrollbar(cv: TCanvas; const bar, thumb: TRect); virtual; abstract;
  end;

  { ---- callbacks ---- }
  TNotifyProc = procedure(sender: TWidget) of object;
  TTabProc    = procedure(index: LongInt) of object;
  TPopupProc  = procedure(index: LongInt) of object;

  TDesktop   = class;
  TPopupMenu = class;

  { ---- base widget ---- }
  TWidget = class
  protected
    FParent:   TWidget;
    FChildren: TFPList;
    FBounds:   TRect;       { parent-relative }
    FVisible:  Boolean;
    FEnabled:  Boolean;
    function  GetChild(i: LongInt): TWidget;
  public
    constructor Create(AParent: TWidget);
    destructor  Destroy; override;

    procedure AddChild(w: TWidget);
    procedure RemoveChild(w: TWidget);   { detach without freeing }
    function  ChildCount: LongInt;
    property  Child[i: LongInt]: TWidget read GetChild;

    function  AbsRect: TRect;                   { screen coordinates }
    function  HitTest(const p: TPoint): TWidget; virtual;
    function  Root: TDesktop;

    procedure Paint(cv: TCanvas; theme: TTheme); virtual;  { self then children }
    procedure Draw(cv: TCanvas; theme: TTheme); virtual;   { just self; default: nothing }

    procedure OnMouseDown(var e: TUIEvent); virtual;
    procedure OnMouseUp(var e: TUIEvent); virtual;
    procedure OnMouseMove(var e: TUIEvent); virtual;
    procedure OnMouseWheel(var e: TUIEvent); virtual;
    procedure OnKeyDown(var e: TUIEvent); virtual;

    { drag-and-drop capability; defaults mean "not a source / not a target" }
    function  DragStart(const p: TPoint): TDragItem; virtual;
    function  DragAccepts(it: TDragItem): Boolean; virtual;
    procedure DragDrop(it: TDragItem; const p: TPoint); virtual;

    procedure SetBounds(l, t, r, b: LongInt);
    property  Bounds:  TRect   read FBounds;
    property  Visible: Boolean read FVisible write FVisible;
    property  Enabled: Boolean read FEnabled write FEnabled;
    property  Parent:  TWidget read FParent;
  end;

  { ---- the game view behind the windows ---- }
  TGameView = class(TWidget)
  public
    procedure Draw(cv: TCanvas; theme: TTheme); override;   { override in the game }
  end;

  { ---- the root: windows over a live game background ---- }
  TDesktop = class(TWidget)
  private
    FTheme:   TTheme;
    FActive:  TWindow;
    FCapture: TWidget;        { widget holding the mouse during a press }
    FDrag:    TDragItem;      { item in hand, nil when not dragging }
    FDragSrc: TWidget;        { slot to return a fumbled item to }
    FPopup:   TPopupMenu;     { open context menu, nil when none }
    FMouse:   TPoint;
  public
    { Fired after a dragged item settles, so an owner can read the new layout back. }
    OnDragEnd: TNotifyProc;
    constructor Create(theme: TTheme; w, h: LongInt);
    procedure Dispatch(var e: TUIEvent); reintroduce;
    procedure PaintAll(cv: TCanvas);
    procedure BringToFront(w: TWindow);
    procedure ShowPopup(const items: array of String; at: TPoint; cb: TPopupProc);
    procedure DismissPopup;
    function  PopupVisible: Boolean;
    { Replace the active theme; the next PaintAll repaints the whole tree    }
    { through it. Ownership of the theme object stays with the caller, so the }
    { caller frees the one it replaces.                                       }
    procedure SetTheme(t: TTheme);
    property  Theme:  TTheme  read FTheme;
    property  Active: TWindow read FActive;
    property  Dragging: TDragItem read FDrag;
  end;

  { ---- a System 7 window ---- }
  { wfZoom (maximize box) and wfGrow (resize box) are optional per window, }
  { so a fixed utility window can leave them out.                          }
  TWindowFlag  = (wfClose, wfDrag, wfZoom, wfGrow);
  TWindowFlags = set of TWindowFlag;

  TWindow = class(TWidget)
  private
    FTitle:    String;
    FFlags:    TWindowFlags;
    FContent:  TWidget;
    FActive:   Boolean;
    FMoving:   Boolean;
    FResizing: Boolean;
    FZoomed:   Boolean;
    FGrabDX, FGrabDY: LongInt;   { move: mouse-from-corner; resize: edge-from-mouse }
    FRestore:  TRect;           { bounds to return to when un-zooming }
    FMinW, FMinH: LongInt;
    procedure UpdateContentBounds;
    procedure ToggleZoom;
  public
    constructor Create(AParent: TWidget; const ATitle: String;
                       AFlags: TWindowFlags; l, t, w, h: LongInt);
    procedure Paint(cv: TCanvas; theme: TTheme); override;
    procedure Draw(cv: TCanvas; theme: TTheme); override;
    function  HitTest(const p: TPoint): TWidget; override;
    procedure OnMouseDown(var e: TUIEvent); override;
    procedure OnMouseMove(var e: TUIEvent); override;
    procedure OnMouseUp(var e: TUIEvent); override;
    function  TitleBarRect: TRect;
    function  CloseBoxRect: TRect;
    function  ZoomBoxRect: TRect;
    function  GrowBoxRect: TRect;
    function  ContentRect: TRect;
    property  Title:   String  read FTitle write FTitle;
    property  Flags:   TWindowFlags read FFlags;
    property  Active:  Boolean read FActive write FActive;
    property  Content: TWidget read FContent;
  end;

  { ---- core widgets ---- }
  TLabel = class(TWidget)
  public
    Caption: String;
    TextColor: TColor;
    constructor Create(AParent: TWidget; const ACaption: String; l, t: LongInt);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
  end;

  TButton = class(TWidget)
  private
    FPressed: Boolean;
  public
    Caption:   String;
    IsDefault: Boolean;
    Tag:       LongInt;    { caller payload, e.g. a menu action; 0 when unset }
    OnClick:   TNotifyProc;
    constructor Create(AParent: TWidget; const ACaption: String; l, t, w, h: LongInt);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
    procedure OnMouseDown(var e: TUIEvent); override;
    procedure OnMouseUp(var e: TUIEvent); override;
  end;

  TItemSlot = class(TWidget)
  private
    FItem:    TDragItem;
    FAccepts: LongInt;    { 0 = accept anything }
  public
    constructor Create(AParent: TWidget; acceptMask: LongInt; l, t, sz: LongInt);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
    function  DragStart(const p: TPoint): TDragItem; override;
    function  DragAccepts(it: TDragItem): Boolean; override;
    procedure DragDrop(it: TDragItem; const p: TPoint); override;
    property  Item: TDragItem read FItem write FItem;
  end;

  TItemGrid = class(TWidget)
  private
    FCols, FRows: LongInt;
  public
    constructor Create(AParent: TWidget; cols, rows, accept, pitch, l, t: LongInt);
    function  SlotAt(col, row: LongInt): TItemSlot;
    property  Cols: LongInt read FCols;
    property  Rows: LongInt read FRows;
  end;

  TTabBar = class(TWidget)
  private
    FCaptions: array of String;
    FActive:   LongInt;
    FTabW:     LongInt;
  public
    OnChange: TTabProc;
    constructor Create(AParent: TWidget; l, t, tabW, h: LongInt);
    procedure AddTab(const caption: String);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
    procedure OnMouseDown(var e: TUIEvent); override;
    property  ActiveTab: LongInt read FActive;
  end;

  { ---- a clipped, scrollable viewport over an oversized content widget ---- }
  { The first child added is the scrolled content; size it with SetContentSize. }
  TScrollView = class(TWidget)
  private
    FContentH:  LongInt;
    FScrollY:   LongInt;
    FBarW:      LongInt;
    FThumbDrag: Boolean;
    FThumbGrab: LongInt;
    function  ViewportW: LongInt;
    function  ViewportH: LongInt;
    function  MaxScroll: LongInt;
    function  TrackTop: LongInt;
    function  TrackH: LongInt;
    function  Content: TWidget;
    procedure Reposition;
  public
    constructor Create(AParent: TWidget; l, t, w, h: LongInt);
    procedure SetContentSize(h: LongInt);
    procedure SetScrollY(y: LongInt);
    function  ViewportRect: TRect;
    function  ScrollbarRect: TRect;
    function  ThumbRect: TRect;
    procedure Paint(cv: TCanvas; theme: TTheme); override;
    procedure OnMouseDown(var e: TUIEvent); override;
    procedure OnMouseMove(var e: TUIEvent); override;
    procedure OnMouseUp(var e: TUIEvent); override;
    procedure OnMouseWheel(var e: TUIEvent); override;
    property  ScrollY: LongInt read FScrollY;
  end;

  { ---- a context menu that floats above everything ---- }
  TPopupMenu = class(TWidget)
  private
    FItems:    array of String;
    FOnSelect: TPopupProc;
    FItemH:    LongInt;
  public
    constructor Create(AParent: TWidget);
    procedure Setup(const items: array of String; at: TPoint; cb: TPopupProc);
    procedure Draw(cv: TCanvas; theme: TTheme); override;
    procedure OnMouseDown(var e: TUIEvent); override;
  end;

{ ---- geometry helpers ---- }
function  Rect(l, t, r, b: LongInt): TRect;
function  Pt(x, y: LongInt): TPoint;
function  PtInRect(const r: TRect; const p: TPoint): Boolean;
function  RectW(const r: TRect): LongInt;
function  RectH(const r: TRect): LongInt;
function  Inset(const r: TRect; d: LongInt): TRect;

const
  clBlack   = TColor($FF000000);
  clWhite   = TColor($FFFFFFFF);
  clLtGray  = TColor($FFC8C8C8);
  clGray    = TColor($FF909090);
  clDkGray  = TColor($FF505050);
  clParch   = TColor($FFF3EEE2);

implementation

{ ---- geometry ---- }

function Rect(l, t, r, b: LongInt): TRect;
begin
  Rect.Left := l; Rect.Top := t; Rect.Right := r; Rect.Bottom := b;
end;

function Pt(x, y: LongInt): TPoint;
begin
  Pt.X := x; Pt.Y := y;
end;

function PtInRect(const r: TRect; const p: TPoint): Boolean;
begin
  PtInRect := (p.X >= r.Left) and (p.X < r.Right) and
              (p.Y >= r.Top) and (p.Y < r.Bottom);
end;

function RectW(const r: TRect): LongInt; begin RectW := r.Right - r.Left; end;
function RectH(const r: TRect): LongInt; begin RectH := r.Bottom - r.Top; end;

function Inset(const r: TRect; d: LongInt): TRect;
begin
  Inset := Rect(r.Left + d, r.Top + d, r.Right - d, r.Bottom - d);
end;

{ ---- TDragItem ---- }

constructor TDragItem.Create(const AName: String; ACat: LongInt; AColor: TColor; AGlyph: Char);
begin
  inherited Create;
  Name := AName; Category := ACat; Color := AColor; Glyph := AGlyph;
  Tag := -1;
end;

{ ---- TWidget ---- }

constructor TWidget.Create(AParent: TWidget);
begin
  inherited Create;
  FChildren := TFPList.Create;
  FVisible := True;
  FEnabled := True;
  FBounds := Rect(0, 0, 0, 0);
  FParent := AParent;
  if AParent <> nil then AParent.AddChild(Self);
end;

destructor TWidget.Destroy;
var i: LongInt;
begin
  for i := 0 to FChildren.Count - 1 do
    TWidget(FChildren[i]).Free;
  FChildren.Free;
  inherited Destroy;
end;

procedure TWidget.AddChild(w: TWidget);
begin
  FChildren.Add(w);
end;

procedure TWidget.RemoveChild(w: TWidget);
begin
  FChildren.Remove(w);
end;

function TWidget.ChildCount: LongInt;
begin
  ChildCount := FChildren.Count;
end;

function TWidget.GetChild(i: LongInt): TWidget;
begin
  GetChild := TWidget(FChildren[i]);
end;

procedure TWidget.SetBounds(l, t, r, b: LongInt);
begin
  FBounds := Rect(l, t, r, b);
end;

function TWidget.AbsRect: TRect;
var p: TWidget; r: TRect;
begin
  r := FBounds;
  p := FParent;
  while p <> nil do begin
    r.Left := r.Left + p.FBounds.Left;  r.Right := r.Right + p.FBounds.Left;
    r.Top := r.Top + p.FBounds.Top;     r.Bottom := r.Bottom + p.FBounds.Top;
    p := p.FParent;
  end;
  AbsRect := r;
end;

function TWidget.Root: TDesktop;
var p: TWidget;
begin
  p := Self;
  while p.FParent <> nil do p := p.FParent;
  if p is TDesktop then Root := TDesktop(p) else Root := nil;
end;

function TWidget.HitTest(const p: TPoint): TWidget;
var i: LongInt; r: TWidget;
begin
  if (not FVisible) or (not PtInRect(AbsRect, p)) then exit(nil);
  for i := FChildren.Count - 1 downto 0 do begin
    r := TWidget(FChildren[i]).HitTest(p);
    if r <> nil then exit(r);
  end;
  HitTest := Self;
end;

procedure TWidget.Paint(cv: TCanvas; theme: TTheme);
var i: LongInt;
begin
  if not FVisible then exit;
  Draw(cv, theme);
  for i := 0 to FChildren.Count - 1 do
    TWidget(FChildren[i]).Paint(cv, theme);
end;

procedure TWidget.Draw(cv: TCanvas; theme: TTheme); begin end;
procedure TWidget.OnMouseDown(var e: TUIEvent); begin end;
procedure TWidget.OnMouseUp(var e: TUIEvent); begin end;
procedure TWidget.OnMouseMove(var e: TUIEvent); begin end;
procedure TWidget.OnMouseWheel(var e: TUIEvent); begin end;
procedure TWidget.OnKeyDown(var e: TUIEvent); begin end;

function  TWidget.DragStart(const p: TPoint): TDragItem; begin DragStart := nil; end;
function  TWidget.DragAccepts(it: TDragItem): Boolean; begin DragAccepts := False; end;
procedure TWidget.DragDrop(it: TDragItem; const p: TPoint); begin end;

{ ---- TGameView ---- }

procedure TGameView.Draw(cv: TCanvas; theme: TTheme);
begin
  { the host game overrides this to render the world }
  cv.FillRect(AbsRect, clDkGray);
end;

{ ---- TDesktop ---- }

constructor TDesktop.Create(theme: TTheme; w, h: LongInt);
begin
  inherited Create(nil);
  FTheme := theme;
  SetBounds(0, 0, w, h);
end;

procedure TDesktop.BringToFront(w: TWindow);
begin
  if FChildren.IndexOf(w) >= 0 then begin
    FChildren.Remove(w);
    FChildren.Add(w);        { last = front }
  end;
  if FActive <> nil then FActive.Active := False;
  FActive := w;
  w.Active := True;
end;

function FindDropTarget(start: TWidget; it: TDragItem; const p: TPoint): TWidget;
var w: TWidget;
begin
  w := start;
  while w <> nil do begin
    if w.DragAccepts(it) then exit(w);
    w := w.Parent;
  end;
  FindDropTarget := nil;
end;

procedure TDesktop.ShowPopup(const items: array of String; at: TPoint; cb: TPopupProc);
begin
  DismissPopup;
  FPopup := TPopupMenu.Create(Self);     { last child = front-most }
  FPopup.Setup(items, at, cb);
end;

procedure TDesktop.DismissPopup;
begin
  if FPopup <> nil then begin
    RemoveChild(FPopup);
    FPopup.Free;
    FPopup := nil;
  end;
end;

function TDesktop.PopupVisible: Boolean;
begin
  PopupVisible := FPopup <> nil;
end;

procedure TDesktop.SetTheme(t: TTheme);
begin
  if t <> nil then FTheme := t;
end;

procedure TDesktop.Dispatch(var e: TUIEvent);
var hit, tgt: TWidget; it: TDragItem; win: TWidget;
begin
  FMouse := e.Pos;

  { ---- an open context menu eats the next press ---- }
  if (FPopup <> nil) and (e.Kind = ekMouseDown) then begin
    if PtInRect(FPopup.AbsRect, e.Pos) then FPopup.OnMouseDown(e);
    DismissPopup;
    exit;
  end;

  { ---- a drag is in progress: route to the drag machinery ---- }
  if FDrag <> nil then begin
    if e.Kind = ekMouseUp then begin
      hit := HitTest(e.Pos);
      tgt := nil;
      if hit <> nil then tgt := FindDropTarget(hit, FDrag, e.Pos);
      it := FDrag; FDrag := nil;
      if tgt <> nil then
        tgt.DragDrop(it, e.Pos)
      else
        FDragSrc.DragDrop(it, e.Pos);   { return to source: no lost items }
      FDragSrc := nil;
      if OnDragEnd <> nil then OnDragEnd(Self);
    end;
    exit;
  end;

  case e.Kind of
    ekMouseDown:
      begin
        hit := HitTest(e.Pos);
        if (hit = nil) or (hit is TDesktop) or (hit is TGameView) then exit;
        { bring the owning window to front }
        win := hit;
        while (win <> nil) and (not (win is TWindow)) do win := win.Parent;
        if win is TWindow then BringToFront(TWindow(win));
        { does this press pick up an item? }
        it := hit.DragStart(e.Pos);
        if it <> nil then begin
          FDrag := it; FDragSrc := hit;
        end else begin
          FCapture := hit;
          hit.OnMouseDown(e);
        end;
      end;
    ekMouseMove:
      if FCapture <> nil then FCapture.OnMouseMove(e);
    ekMouseUp:
      begin
        if FCapture <> nil then begin
          FCapture.OnMouseUp(e);
          FCapture := nil;
        end;
      end;
    ekMouseWheel:
      begin
        hit := HitTest(e.Pos);
        while hit <> nil do begin
          hit.OnMouseWheel(e);
          if e.Handled then break;
          hit := hit.Parent;
        end;
      end;
    ekKeyDown:
      if FActive <> nil then FActive.OnKeyDown(e);
  end;
end;

procedure TDesktop.PaintAll(cv: TCanvas);
var i: LongInt; r: TRect; it: TDragItem;
begin
  { children[0] is the game view; the rest are windows, painted back-to-front }
  for i := 0 to FChildren.Count - 1 do
    TWidget(FChildren[i]).Paint(cv, FTheme);
  { drag image follows the cursor }
  if FDrag <> nil then begin
    r := Rect(FMouse.X - 16, FMouse.Y - 16, FMouse.X + 16, FMouse.Y + 16);
    cv.FillRect(r, FDrag.Color);
    cv.FrameRect(r, clBlack);
    it := FDrag;
    cv.DrawText(FMouse.X - cv.TextWidth(it.Glyph) div 2,
                FMouse.Y - cv.TextHeight div 2, it.Glyph, clBlack);
  end;
end;

{ ---- TWindow ---- }

constructor TWindow.Create(AParent: TWidget; const ATitle: String;
  AFlags: TWindowFlags; l, t, w, h: LongInt);
begin
  inherited Create(AParent);
  FTitle := ATitle;
  FFlags := AFlags;
  FMinW := 120;
  FMinH := 72;
  SetBounds(l, t, l + w, t + h);
  FContent := TWidget.Create(Self);
  UpdateContentBounds;
end;

procedure TWindow.UpdateContentBounds;
var bw, th, w, h: LongInt;
begin
  bw := Root.Theme.BorderWidth;
  th := Root.Theme.TitleBarHeight;
  w := RectW(FBounds); h := RectH(FBounds);
  FContent.SetBounds(bw, th, w - bw, h - bw);
end;

procedure TWindow.ToggleZoom;
var d: TDesktop;
begin
  d := Root;
  if not FZoomed then begin
    FRestore := FBounds;
    SetBounds(8, 8, RectW(d.Bounds) - 8, RectH(d.Bounds) - 8);
    FZoomed := True;
  end else begin
    FBounds := FRestore;
    FZoomed := False;
  end;
  UpdateContentBounds;
end;

function TWindow.TitleBarRect: TRect;
var r: TRect;
begin
  r := AbsRect;
  TitleBarRect := Rect(r.Left, r.Top, r.Right, r.Top + Root.Theme.TitleBarHeight);
end;

function TWindow.CloseBoxRect: TRect;
var tb: TRect;
begin
  tb := TitleBarRect;
  CloseBoxRect := Rect(tb.Left + 6, tb.Top + 6, tb.Left + 18, tb.Top + 18);
end;

function TWindow.ZoomBoxRect: TRect;
var r: TRect;
begin
  r := AbsRect;
  ZoomBoxRect := Rect(r.Right - 18, r.Top + 6, r.Right - 6, r.Top + 18);
end;

function TWindow.GrowBoxRect: TRect;
var r: TRect;
begin
  r := AbsRect;
  GrowBoxRect := Rect(r.Right - 16, r.Bottom - 16, r.Right, r.Bottom);
end;

function TWindow.ContentRect: TRect;
begin
  ContentRect := FContent.AbsRect;
end;

procedure TWindow.Paint(cv: TCanvas; theme: TTheme);
begin
  if not FVisible then exit;
  Draw(cv, theme);                { chrome }
  cv.SetClip(ContentRect);        { keep content inside the frame when resized }
  FContent.Paint(cv, theme);
  cv.ClearClip;
end;

procedure TWindow.Draw(cv: TCanvas; theme: TTheme);
begin
  theme.DrawWindow(cv, Self);
end;

function TWindow.HitTest(const p: TPoint): TWidget;
begin
  if (not FVisible) or (not PtInRect(AbsRect, p)) then exit(nil);
  { the window claims its own chrome so content cannot swallow those clicks }
  if PtInRect(TitleBarRect, p) then exit(Self);
  if (wfGrow in FFlags) and PtInRect(GrowBoxRect, p) then exit(Self);
  HitTest := inherited HitTest(p);
end;

procedure TWindow.OnMouseDown(var e: TUIEvent);
var r: TRect;
begin
  if (wfClose in FFlags) and PtInRect(CloseBoxRect, e.Pos) then begin
    FVisible := False;
    exit;
  end;
  if (wfZoom in FFlags) and PtInRect(ZoomBoxRect, e.Pos) then begin
    ToggleZoom;
    exit;
  end;
  if (wfGrow in FFlags) and PtInRect(GrowBoxRect, e.Pos) then begin
    FResizing := True;
    r := AbsRect;
    FGrabDX := r.Right - e.Pos.X;      { mouse to right edge }
    FGrabDY := r.Bottom - e.Pos.Y;     { mouse to bottom edge }
    exit;
  end;
  if (wfDrag in FFlags) and PtInRect(TitleBarRect, e.Pos) then begin
    FMoving := True;
    r := AbsRect;
    FGrabDX := e.Pos.X - r.Left;
    FGrabDY := e.Pos.Y - r.Top;
  end;
end;

procedure TWindow.OnMouseMove(var e: TUIEvent);
var nx, ny, nr, nb, w, h: LongInt;
begin
  { top-level windows sit on the desktop at (0,0), so screen == parent coords }
  if FMoving then begin
    w := RectW(FBounds); h := RectH(FBounds);
    nx := e.Pos.X - FGrabDX; ny := e.Pos.Y - FGrabDY;
    SetBounds(nx, ny, nx + w, ny + h);
    FZoomed := False;
  end else if FResizing then begin
    nr := e.Pos.X + FGrabDX;
    nb := e.Pos.Y + FGrabDY;
    if nr - FBounds.Left < FMinW then nr := FBounds.Left + FMinW;
    if nb - FBounds.Top  < FMinH then nb := FBounds.Top + FMinH;
    SetBounds(FBounds.Left, FBounds.Top, nr, nb);
    UpdateContentBounds;
    FZoomed := False;
  end;
end;

procedure TWindow.OnMouseUp(var e: TUIEvent);
begin
  FMoving := False;
  FResizing := False;
end;

{ ---- TLabel ---- }

constructor TLabel.Create(AParent: TWidget; const ACaption: String; l, t: LongInt);
begin
  inherited Create(AParent);
  Caption := ACaption;
  TextColor := clBlack;
  SetBounds(l, t, l + 100, t + 12);
end;

procedure TLabel.Draw(cv: TCanvas; theme: TTheme);
var r: TRect;
begin
  r := AbsRect;
  cv.DrawText(r.Left, r.Top, Caption, TextColor);
end;

{ ---- TButton ---- }

constructor TButton.Create(AParent: TWidget; const ACaption: String; l, t, w, h: LongInt);
begin
  inherited Create(AParent);
  Caption := ACaption;
  Tag := 0;
  SetBounds(l, t, l + w, t + h);
end;

procedure TButton.Draw(cv: TCanvas; theme: TTheme);
begin
  theme.DrawButton(cv, AbsRect, Caption, FPressed, IsDefault, FEnabled);
end;

procedure TButton.OnMouseDown(var e: TUIEvent);
begin
  FPressed := True;
end;

procedure TButton.OnMouseUp(var e: TUIEvent);
begin
  FPressed := False;
  if PtInRect(AbsRect, e.Pos) and (OnClick <> nil) then OnClick(Self);
end;

{ ---- TItemSlot ---- }

constructor TItemSlot.Create(AParent: TWidget; acceptMask: LongInt; l, t, sz: LongInt);
begin
  inherited Create(AParent);
  FAccepts := acceptMask;
  SetBounds(l, t, l + sz, t + sz);
end;

procedure TItemSlot.Draw(cv: TCanvas; theme: TTheme);
var r, inr: TRect;
begin
  r := AbsRect;
  theme.DrawSlot(cv, r, False, FItem <> nil);
  if FItem <> nil then begin
    inr := Inset(r, 3);
    cv.FillRect(inr, FItem.Color);
    cv.FrameRect(inr, clBlack);
    cv.DrawText((r.Left + r.Right) div 2 - cv.TextWidth(FItem.Glyph) div 2,
                (r.Top + r.Bottom) div 2 - cv.TextHeight div 2,
                FItem.Glyph, clBlack);
  end;
end;

function TItemSlot.DragStart(const p: TPoint): TDragItem;
begin
  if FItem = nil then
    DragStart := nil
  else begin
    DragStart := FItem;
    FItem := nil;
  end;
end;

function TItemSlot.DragAccepts(it: TDragItem): Boolean;
begin
  DragAccepts := (FItem = nil) and
                 ((FAccepts = 0) or ((FAccepts and it.Category) <> 0));
end;

procedure TItemSlot.DragDrop(it: TDragItem; const p: TPoint);
begin
  FItem := it;
end;

{ ---- TItemGrid ---- }

constructor TItemGrid.Create(AParent: TWidget; cols, rows, accept, pitch, l, t: LongInt);
var c, rr: LongInt;
begin
  inherited Create(AParent);
  FCols := cols; FRows := rows;
  SetBounds(l, t, l + cols * pitch, t + rows * pitch);
  for rr := 0 to rows - 1 do
    for c := 0 to cols - 1 do
      TItemSlot.Create(Self, accept, c * pitch, rr * pitch, pitch - 4);
end;

function TItemGrid.SlotAt(col, row: LongInt): TItemSlot;
begin
  SlotAt := TItemSlot(FChildren[row * FCols + col]);
end;

{ ---- TTabBar ---- }

constructor TTabBar.Create(AParent: TWidget; l, t, tabW, h: LongInt);
begin
  inherited Create(AParent);
  FTabW := tabW;
  FActive := 0;
  SetBounds(l, t, l + 400, t + h);
end;

procedure TTabBar.AddTab(const caption: String);
var n: LongInt;
begin
  n := Length(FCaptions);
  SetLength(FCaptions, n + 1);
  FCaptions[n] := caption;
end;

procedure TTabBar.Draw(cv: TCanvas; theme: TTheme);
var i: LongInt; r, b: TRect;
begin
  b := AbsRect;
  for i := 0 to High(FCaptions) do begin
    r := Rect(b.Left + i * (FTabW + 2), b.Top,
              b.Left + i * (FTabW + 2) + FTabW, b.Bottom);
    theme.DrawTab(cv, r, FCaptions[i], i = FActive);
  end;
end;

procedure TTabBar.OnMouseDown(var e: TUIEvent);
var b: TRect; i, rel: LongInt;
begin
  b := AbsRect;
  rel := e.Pos.X - b.Left;
  i := rel div (FTabW + 2);
  if (i >= 0) and (i <= High(FCaptions)) then begin
    FActive := i;
    if OnChange <> nil then OnChange(i);
  end;
end;

{ ---- TScrollView ---- }

constructor TScrollView.Create(AParent: TWidget; l, t, w, h: LongInt);
begin
  inherited Create(AParent);
  FBarW := 16;
  FScrollY := 0;
  SetBounds(l, t, l + w, t + h);
end;

function TScrollView.Content: TWidget;
begin
  if FChildren.Count > 0 then Content := TWidget(FChildren[0]) else Content := nil;
end;

function TScrollView.ViewportW: LongInt; begin ViewportW := RectW(FBounds) - FBarW; end;
function TScrollView.ViewportH: LongInt; begin ViewportH := RectH(FBounds); end;

function TScrollView.MaxScroll: LongInt;
begin
  MaxScroll := FContentH - ViewportH;
  if MaxScroll < 0 then MaxScroll := 0;
end;

function TScrollView.TrackTop: LongInt; begin TrackTop := AbsRect.Top + FBarW; end;
function TScrollView.TrackH: LongInt; begin TrackH := RectH(FBounds) - 2 * FBarW; end;

procedure TScrollView.Reposition;
var c: TWidget;
begin
  c := Content;
  if c <> nil then
    c.SetBounds(0, -FScrollY, ViewportW, -FScrollY + FContentH);
end;

procedure TScrollView.SetContentSize(h: LongInt);
begin
  FContentH := h;
  SetScrollY(FScrollY);
end;

procedure TScrollView.SetScrollY(y: LongInt);
begin
  if y < 0 then y := 0;
  if y > MaxScroll then y := MaxScroll;
  FScrollY := y;
  Reposition;
end;

function TScrollView.ViewportRect: TRect;
var r: TRect;
begin
  r := AbsRect;
  ViewportRect := Rect(r.Left, r.Top, r.Right - FBarW, r.Bottom);
end;

function TScrollView.ScrollbarRect: TRect;
var r: TRect;
begin
  r := AbsRect;
  ScrollbarRect := Rect(r.Right - FBarW, r.Top, r.Right, r.Bottom);
end;

function TScrollView.ThumbRect: TRect;
var bar: TRect; th, ty, tt, trk: LongInt;
begin
  bar := ScrollbarRect;
  tt := TrackTop; trk := TrackH;
  if FContentH <= ViewportH then th := trk
  else th := (trk * ViewportH) div FContentH;
  if th < 20 then th := 20;
  if MaxScroll <= 0 then ty := tt
  else ty := tt + ((trk - th) * FScrollY) div MaxScroll;
  ThumbRect := Rect(bar.Left + 2, ty, bar.Right - 2, ty + th);
end;

procedure TScrollView.Paint(cv: TCanvas; theme: TTheme);
var c: TWidget;
begin
  if not FVisible then exit;
  cv.FillRect(ViewportRect, clWhite);
  cv.SetClip(ViewportRect);
  c := Content;
  if c <> nil then c.Paint(cv, theme);
  cv.ClearClip;
  theme.DrawScrollbar(cv, ScrollbarRect, ThumbRect);
  cv.FrameRect(AbsRect, clBlack);
end;

procedure TScrollView.OnMouseDown(var e: TUIEvent);
var bar: TRect;
begin
  bar := ScrollbarRect;
  if not PtInRect(bar, e.Pos) then exit;
  if e.Pos.Y < bar.Top + FBarW then SetScrollY(FScrollY - 24)
  else if e.Pos.Y >= bar.Bottom - FBarW then SetScrollY(FScrollY + 24)
  else if PtInRect(ThumbRect, e.Pos) then begin
    FThumbDrag := True;
    FThumbGrab := e.Pos.Y - ThumbRect.Top;
  end
  else if e.Pos.Y < ThumbRect.Top then SetScrollY(FScrollY - ViewportH)
  else SetScrollY(FScrollY + ViewportH);
end;

procedure TScrollView.OnMouseMove(var e: TUIEvent);
var th, newTop, denom: LongInt;
begin
  if not FThumbDrag then exit;
  th := RectH(ThumbRect);
  newTop := e.Pos.Y - FThumbGrab;
  denom := TrackH - th;
  if denom <= 0 then exit;
  SetScrollY(((newTop - TrackTop) * MaxScroll) div denom);
end;

procedure TScrollView.OnMouseUp(var e: TUIEvent);
begin
  FThumbDrag := False;
end;

procedure TScrollView.OnMouseWheel(var e: TUIEvent);
begin
  SetScrollY(FScrollY - e.Wheel * 24);
  e.Handled := True;
end;

{ ---- TPopupMenu ---- }

constructor TPopupMenu.Create(AParent: TWidget);
begin
  inherited Create(AParent);
  FItemH := 18;
end;

procedure TPopupMenu.Setup(const items: array of String; at: TPoint; cb: TPopupProc);
var i, w, cw, deskW, deskH: LongInt; d: TDesktop;
begin
  SetLength(FItems, Length(items));
  w := 0;
  for i := 0 to High(items) do begin
    FItems[i] := items[i];
    cw := Length(items[i]) * 12;      { font advance is 6px at scale 2 }
    if cw > w then w := cw;
  end;
  FOnSelect := cb;
  w := w + 16;
  d := Root;
  deskW := RectW(d.Bounds); deskH := RectH(d.Bounds);
  if at.X + w > deskW then at.X := deskW - w;
  if at.Y + Length(items) * FItemH + 4 > deskH then
    at.Y := deskH - (Length(items) * FItemH + 4);
  SetBounds(at.X, at.Y, at.X + w, at.Y + Length(items) * FItemH + 4);
end;

procedure TPopupMenu.Draw(cv: TCanvas; theme: TTheme);
var r: TRect; i: LongInt;
begin
  r := AbsRect;
  cv.FillRect(r, clWhite);
  cv.FrameRect(r, clBlack);
  for i := 0 to High(FItems) do
    cv.DrawText(r.Left + 8, r.Top + 4 + i * FItemH, FItems[i], clBlack);
end;

procedure TPopupMenu.OnMouseDown(var e: TUIEvent);
var idx: LongInt; r: TRect;
begin
  r := AbsRect;
  idx := (e.Pos.Y - (r.Top + 2)) div FItemH;
  if (idx >= 0) and (idx <= High(FItems)) and (FOnSelect <> nil) then
    FOnSelect(idx);
end;

end.
