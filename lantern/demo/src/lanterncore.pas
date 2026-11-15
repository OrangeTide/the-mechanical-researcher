unit LanternCore;

{ The transport-agnostic protocol core.

  The application owns the socket and the loop. It feeds received datagrams
  in with LanternFeed and pulls datagrams to send out with LanternSendNext,
  which keeps this unit from ever naming an operating system. A server keeps
  one connection slot per peer and routes inbound datagrams with LanternPeekId.

  State of the implementation: the arena, the connection freelist, the
  stateless admission cookie (SipHash-2-4), and connection-id routing are
  real. The channel send/receive windows, the retransmission timers, and the
  handshake state transitions are laid out but stubbed; each stub says what
  it owes. Every entry point that cares about time takes the caller's
  monotonic nowMs, so the library reads no clock of its own. That is the one
  rule netchan learned the hard way: a second clock inside the library
  underflows a timer and closes a brand-new connection. }

{$mode objfpc}{$H+}

interface

uses
  LanternTypes, LanternArena;

type
  PLanternServer = ^TLanternServer;
  PLanternConn   = ^TLanternConn;
  PLanternChan   = ^TLanternChan;

  TLanternChan = record
    inUse  : Boolean;
    id     : Byte;
    mode   : Longint;    { cmReliable or cmUnreliable                    }
    buf    : PByte;      { this channel's slice of the arena             }
    bufLen : PtrUInt;
    { TODO: send-window head/tail, recv reorder bitmap, unreliable txq
      head/tail. All index into buf; none allocates. }
  end;

  TLanternConn = record
    inUse      : Boolean;
    state      : Longint;        { psClosed .. psClosing                 }
    localId    : LongWord;       { our connection id                     }
    remoteId   : LongWord;       { peer's connection id                  }
    peer       : TLanternAddr;
    nextFree   : Longint;        { freelist link while not inUse, else -1 }
    lastRecvMs : QWord;
    lastSendMs : QWord;
    lastPingMs : QWord;
    rttMs      : LongWord;
    chans      : PLanternChan;    { array[cfg.MaxChan], carved in arena   }
  end;

  TLanternEvent = record
    kind : Longint;              { peNone .. peRedirect                  }
    ch   : PLanternChan;          { set for peData                        }
    conn : PLanternConn;
  end;

  TCookieKey = record
    k0, k1: QWord;
  end;

  TLanternServer = record
    arena      : TArena;
    cfg        : TLanternConfig;
    isServer   : Boolean;
    conns      : PLanternConn;    { array[cfg.MaxConns]                   }
    freeHead   : Longint;        { first free slot index, -1 if full     }
    freeCount  : Longint;
    { Stateless admission secret, two generations so a cookie issued just
      before a rotation still verifies. }
    cookieKey  : array[0 .. 1] of TCookieKey;
    cookieCur  : Longint;        { which generation is current           }
    cookieEpoch: QWord;
    { Minimal one-slot event queue for the skeleton. }
    ev         : TLanternEvent;
  end;

{ ---- Arena sizing (public so a tool or test can report it) ---- }

{ Bytes one reliable+unreliable channel needs for its windows and queue. }
function LanternPerChanBytes(const cfg: TLanternConfig): PtrUInt;
{ Bytes one connection needs, channels included. }
function LanternPerConnBytes(const cfg: TLanternConfig): PtrUInt;
{ Total bytes the single GetMem will request for this configuration. }
function LanternArenaSize(const cfg: TLanternConfig): PtrUInt;

{ ---- Lifecycle ---- }

{ Allocate the one arena and lay out every slot. Returns nil if the host is
  out of memory. isServer decides whether this end answers INITs or sends
  them. }
function LanternServerCreate(const cfg: TLanternConfig; isServer: Boolean): PLanternServer;
procedure LanternServerDestroy(srv: PLanternServer);

function LanternFreeCount(srv: PLanternServer): Longint;
function LanternActiveCount(srv: PLanternServer): Longint;

{ ---- Routing and I/O (application owns the socket) ---- }

{ Read the remote connection id from a raw datagram without parsing it. An
  id of 0 means a new-connection attempt; feed it to admission. Any other id
  routes to the slot whose localId matches. }
function LanternPeekId(const pkt; len: PtrUInt): LongWord;

{ Hand a received datagram to the core. from is the sender's address, nowMs
  the caller's monotonic clock. }
function LanternFeed(srv: PLanternServer; const pkt; len: PtrUInt;
                    const from: TLanternAddr; nowMs: QWord): Longint;

{ Fill buf with the next datagram to transmit and set 'to'. Returns the byte
  count, 0 if nothing is queued, or a negative error. }
function LanternSendNext(srv: PLanternServer; var buf; cap: PtrUInt;
                        out dest: TLanternAddr; nowMs: QWord): Longint;

{ Advance timers: retransmit, keepalive, idle timeout, cookie rotation. }
procedure LanternService(srv: PLanternServer; nowMs: QWord);

function LanternPoll(srv: PLanternServer; out ev: TLanternEvent): Boolean;

{ ---- Admission cookie (exposed for testing the anti-abuse path) ---- }

{ The stateless return-routability token: MAC(secret, addr | nonce | epoch).
  Computing and checking it is one SipHash and no allocation. }
function LanternCookie(srv: PLanternServer; const addr: TLanternAddr;
                      nonce, epoch: QWord): QWord;
function LanternCookieVerify(srv: PLanternServer; const addr: TLanternAddr;
                           nonce, epoch, cookie: QWord): Boolean;

implementation

{ ---- Element addressing in the arena arrays ---- }

function ConnAt(srv: PLanternServer; i: Longint): PLanternConn; inline;
begin
  Result := PLanternConn(PByte(srv^.conns) + PtrUInt(i) * SizeOf(TLanternConn));
end;

function ChanAt(conn: PLanternConn; i: Longint): PLanternChan; inline;
begin
  Result := PLanternChan(PByte(conn^.chans) + PtrUInt(i) * SizeOf(TLanternChan));
end;

{ ---- Sizing ---- }

function LanternPerChanBytes(const cfg: TLanternConfig): PtrUInt;
begin
  { One MTU-sized slot per reliable send-window entry, per reorder slot, and
    per queued unreliable datagram. An unreliable-only channel never fills
    the reliable windows, so a game that shrinks them pays far less. }
  Result := (PtrUInt(cfg.SendWindow) + cfg.RecvWindow + cfg.UnrelTxq)
            * PtrUInt(cfg.MTU);
end;

function LanternPerConnBytes(const cfg: TLanternConfig): PtrUInt;
begin
  Result := SizeOf(TLanternConn)
            + PtrUInt(cfg.MaxChan) * SizeOf(TLanternChan)
            + PtrUInt(cfg.MaxChan) * LanternPerChanBytes(cfg);
end;

function LanternArenaSize(const cfg: TLanternConfig): PtrUInt;
var
  allocs: PtrUInt;
begin
  { Each ArenaAlloc rounds the cursor up to ArenaAlign, so reserve one
    alignment gap per allocation: the server header, the conns array, and
    per connection one chans array plus one buffer per channel. }
  allocs := 2 + PtrUInt(cfg.MaxConns) * (1 + PtrUInt(cfg.MaxChan));
  Result := SizeOf(TLanternServer)
            + PtrUInt(cfg.MaxConns) * LanternPerConnBytes(cfg)
            + allocs * ArenaAlign;
end;

{ ---- SipHash-2-4, the cookie MAC ---- }

function SipHash24(k0, k1: QWord; data: PByte; len: PtrUInt): QWord;
var
  v0, v1, v2, v3, m, b: QWord;
  i, j, left: PtrUInt;

  procedure Round;
  begin
    v0 := v0 + v1; v1 := (v1 shl 13) or (v1 shr 51); v1 := v1 xor v0;
    v0 := (v0 shl 32) or (v0 shr 32);
    v2 := v2 + v3; v3 := (v3 shl 16) or (v3 shr 48); v3 := v3 xor v2;
    v0 := v0 + v3; v3 := (v3 shl 21) or (v3 shr 43); v3 := v3 xor v0;
    v2 := v2 + v1; v1 := (v1 shl 17) or (v1 shr 47); v1 := v1 xor v2;
    v2 := (v2 shl 32) or (v2 shr 32);
  end;

begin
  v0 := k0 xor QWord($736F6D6570736575);
  v1 := k1 xor QWord($646F72616E646F6D);
  v2 := k0 xor QWord($6C7967656E657261);
  v3 := k1 xor QWord($7465646279746573);

  i := 0;
  while i + 8 <= len do begin
    m := 0;
    for j := 0 to 7 do
      m := m or (QWord(data[i + j]) shl (8 * j));   { little-endian load }
    v3 := v3 xor m;
    Round; Round;
    v0 := v0 xor m;
    Inc(i, 8);
  end;

  b := QWord(len) shl 56;
  left := len - i;
  for j := 0 to left - 1 do
    b := b or (QWord(data[i + j]) shl (8 * j));
  v3 := v3 xor b;
  Round; Round;
  v0 := v0 xor b;

  v2 := v2 xor QWord($FF);
  Round; Round; Round; Round;
  Result := v0 xor v1 xor v2 xor v3;
end;

{ ---- Randomness for secrets ----
  TODO: this is placeholder entropy. Replace with the OS CSPRNG before this
  gates anything real: getrandom()/ /dev/urandom on Unix, BCryptGenRandom on
  Windows. A weak cookie secret lets an attacker forge return-routability. }
procedure LanternFillRandom(dst: PByte; n: PtrUInt);
var
  i: PtrUInt;
begin
  for i := 0 to n - 1 do
    dst[i] := Byte(Random(256));
end;

procedure CookieRotate(srv: PLanternServer);
var
  nextGen: Longint;
begin
  nextGen := 1 - srv^.cookieCur;
  LanternFillRandom(@srv^.cookieKey[nextGen], SizeOf(TCookieKey));
  srv^.cookieCur := nextGen;
  Inc(srv^.cookieEpoch);
end;

{ ---- Lifecycle ---- }

function LanternServerCreate(const cfg: TLanternConfig; isServer: Boolean): PLanternServer;
var
  a: TArena;
  srv: PLanternServer;
  conn: PLanternConn;
  chan: PLanternChan;
  i, j: Longint;
begin
  if not ArenaCreate(a, LanternArenaSize(cfg)) then exit(nil);

  srv := ArenaAlloc(a, SizeOf(TLanternServer));
  if srv = nil then begin ArenaDestroy(a); exit(nil); end;
  srv^.arena    := a;        { copy the handle into the header, which lives
                               inside the block it describes }
  srv^.cfg      := cfg;
  srv^.isServer := isServer;
  srv^.cookieCur := 0;
  srv^.cookieEpoch := 0;
  srv^.ev.kind := peNone;

  srv^.conns := ArenaAlloc(srv^.arena, PtrUInt(cfg.MaxConns) * SizeOf(TLanternConn));
  if srv^.conns = nil then begin a := srv^.arena; ArenaDestroy(a); exit(nil); end;

  for i := 0 to cfg.MaxConns - 1 do begin
    conn := ConnAt(srv, i);
    conn^.inUse    := False;
    conn^.state    := psClosed;
    conn^.nextFree := i + 1;
    conn^.chans := ArenaAlloc(srv^.arena, PtrUInt(cfg.MaxChan) * SizeOf(TLanternChan));
    if conn^.chans = nil then begin a := srv^.arena; ArenaDestroy(a); exit(nil); end;
    for j := 0 to cfg.MaxChan - 1 do begin
      chan := ChanAt(conn, j);
      chan^.inUse  := False;
      chan^.id     := j;
      chan^.buf    := ArenaAlloc(srv^.arena, LanternPerChanBytes(cfg));
      chan^.bufLen := LanternPerChanBytes(cfg);
      if chan^.buf = nil then begin a := srv^.arena; ArenaDestroy(a); exit(nil); end;
    end;
  end;
  if cfg.MaxConns > 0 then
    ConnAt(srv, cfg.MaxConns - 1)^.nextFree := -1;
  srv^.freeHead  := 0;
  srv^.freeCount := cfg.MaxConns;

  Randomize;
  CookieRotate(srv);         { seed the current generation }
  Result := srv;
end;

procedure LanternServerDestroy(srv: PLanternServer);
var
  a: TArena;
begin
  if srv = nil then exit;
  a := srv^.arena;           { read the handle before freeing the block it
                               is stored in }
  ArenaDestroy(a);
end;

{ ---- Slot freelist (the cheap admit/deny gate) ---- }

function TakeSlot(srv: PLanternServer): PLanternConn;
var
  idx: Longint;
  conn: PLanternConn;
begin
  if srv^.freeHead < 0 then exit(nil);     { arena full: O(1) refusal }
  idx := srv^.freeHead;
  conn := ConnAt(srv, idx);
  srv^.freeHead := conn^.nextFree;
  Dec(srv^.freeCount);
  conn^.inUse    := True;
  conn^.nextFree := -1;
  conn^.state    := psConnecting;
  Result := conn;
end;

procedure ReturnSlot(srv: PLanternServer; conn: PLanternConn; idx: Longint);
begin
  conn^.inUse    := False;
  conn^.state    := psClosed;
  conn^.nextFree := srv^.freeHead;
  srv^.freeHead  := idx;
  Inc(srv^.freeCount);
end;

function LanternFreeCount(srv: PLanternServer): Longint;
begin
  Result := srv^.freeCount;
end;

function LanternActiveCount(srv: PLanternServer): Longint;
begin
  Result := Longint(srv^.cfg.MaxConns) - srv^.freeCount;
end;

{ ---- Cookie ---- }

function LanternCookie(srv: PLanternServer; const addr: TLanternAddr;
                      nonce, epoch: QWord): QWord;
var
  tmp: array[0 .. LanternAddrMax + 16] of Byte;
  n, i: PtrUInt;
begin
  n := 0;
  tmp[n] := addr.len; Inc(n);
  for i := 0 to addr.len - 1 do begin
    tmp[n] := addr.a[i]; Inc(n);
  end;
  for i := 0 to 7 do begin
    tmp[n] := Byte(nonce shr (8 * i)); Inc(n);
  end;
  for i := 0 to 7 do begin
    tmp[n] := Byte(epoch shr (8 * i)); Inc(n);
  end;
  Result := SipHash24(srv^.cookieKey[srv^.cookieCur].k0,
                      srv^.cookieKey[srv^.cookieCur].k1, @tmp[0], n);
end;

function LanternCookieVerify(srv: PLanternServer; const addr: TLanternAddr;
                           nonce, epoch, cookie: QWord): Boolean;
var
  g: Longint;
  tmp: array[0 .. LanternAddrMax + 16] of Byte;
  n, i: PtrUInt;
  mac: QWord;
begin
  { Check both generations so a cookie issued just before a rotation still
    verifies. The input is assembled once and hashed under each key. }
  n := 0;
  tmp[n] := addr.len; Inc(n);
  for i := 0 to addr.len - 1 do begin tmp[n] := addr.a[i]; Inc(n); end;
  for i := 0 to 7 do begin tmp[n] := Byte(nonce shr (8 * i)); Inc(n); end;
  for i := 0 to 7 do begin tmp[n] := Byte(epoch shr (8 * i)); Inc(n); end;
  for g := 0 to 1 do begin
    mac := SipHash24(srv^.cookieKey[g].k0, srv^.cookieKey[g].k1, @tmp[0], n);
    if mac = cookie then exit(True);
  end;
  Result := False;
end;

{ ---- Routing and I/O ---- }

function LanternPeekId(const pkt; len: PtrUInt): LongWord;
var
  p: PByte;
begin
  { Datagram header, first draft:
      byte 0  = header kind: 0 = no-id (INIT family), 1 = data (has id)
      byte 1..4 = remote connection id, little-endian, when kind = 1
    TODO: finalize the header once the frame layout is pinned down. }
  if len < 1 then exit(0);
  p := @pkt;
  if (p[0] = 1) and (len >= 5) then
    Result := LongWord(p[1]) or (LongWord(p[2]) shl 8)
              or (LongWord(p[3]) shl 16) or (LongWord(p[4]) shl 24)
  else
    Result := 0;
end;

function LanternFeed(srv: PLanternServer; const pkt; len: PtrUInt;
                    const from: TLanternAddr; nowMs: QWord): Longint;
begin
  { TODO: parse the header, route by LanternPeekId, run admission for id 0
    (issue or verify a cookie, then TakeSlot or REFUSE), and dispatch frames
    to the matching channel. Must re-send CONNECT_ACCEPT when an established
    peer repeats its INIT, and must ignore a duplicate ACCEPT, so one lost
    packet never strands the handshake. Stamps lastRecvMs from nowMs. }
  if (srv = nil) or (len = 0) then exit(LanternErrProto);
  Result := LanternErrAgain;
end;

function LanternSendNext(srv: PLanternServer; var buf; cap: PtrUInt;
                        out dest: TLanternAddr; nowMs: QWord): Longint;
begin
  { TODO: pack queued frames from every channel into one datagram, padded to
    MTU, and set dest. Honors flow-control credit and the reliable send
    window. Stamps lastSendMs from nowMs. }
  FillChar(dest, SizeOf(dest), 0);
  if (srv = nil) or (cap = 0) then exit(LanternErr);
  Result := 0;             { nothing queued }
end;

procedure LanternService(srv: PLanternServer; nowMs: QWord);
begin
  { TODO: walk active connections; retransmit unacked reliable messages,
    send keepalive PINGs, close idle peers. Every elapsed-time test uses
    nowMs and treats a stamp in the near future as zero elapsed, so a clock
    that ticks between Feed and Service cannot underflow a timer. Rotate the
    cookie secret on its schedule. }
  if srv = nil then exit;
end;

function LanternPoll(srv: PLanternServer; out ev: TLanternEvent): Boolean;
begin
  if (srv = nil) or (srv^.ev.kind = peNone) then begin
    FillChar(ev, SizeOf(ev), 0);
    exit(False);
  end;
  ev := srv^.ev;
  srv^.ev.kind := peNone;
  Result := True;
end;

end.
