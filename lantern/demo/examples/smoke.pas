// SPDX-License-Identifier: 0BSD OR CC0-1.0
program smoke;

{ A build-and-sanity test for the parts of Lantern that are implemented: the
  static arena sizing, the connection freelist, the stateless admission
  cookie, and the little-endian wire codec. It does no networking, because
  the handshake and channel state machines are still stubs. }

{$mode objfpc}{$H+}

uses
  LanternTypes, LanternWire, LanternCore;

var
  fails: LongInt = 0;

procedure Check(cond: Boolean; const what: string);
begin
  if cond then
    writeln('  ok   ', what)
  else begin
    writeln('  FAIL ', what);
    Inc(fails);
  end;
end;

procedure ShowSizing;
var
  cfg: TLanternConfig;
begin
  writeln('Arena size by player count (MTU 1200, window 8/8, 2 channels):');
  cfg := LanternDefaultConfig;
  cfg.MaxConns := 8;   writeln('  8 players   : ', LanternArenaSize(cfg) div 1024, ' KB');
  cfg.MaxConns := 32;  writeln('  32 players  : ', LanternArenaSize(cfg) div 1024, ' KB');
  cfg.MaxConns := 128; writeln('  128 players : ', LanternArenaSize(cfg) div 1024, ' KB');
  writeln;
end;

procedure TestArena;
var
  cfg: TLanternConfig;
  srv: PLanternServer;
begin
  writeln('Arena and freelist:');
  cfg := LanternDefaultConfig;
  cfg.MaxConns := 16;
  srv := LanternServerCreate(cfg, True);
  Check(srv <> nil, 'server created from config');
  if srv <> nil then begin
    Check(LanternFreeCount(srv) = 16, 'freelist starts full (16 slots)');
    Check(LanternActiveCount(srv) = 0, 'no active connections at start');
    LanternServerDestroy(srv);
    writeln('  ok   server destroyed (single FreeMem)');
  end;
  writeln;
end;

procedure TestCookie;
var
  cfg: TLanternConfig;
  srv: PLanternServer;
  addr: TLanternAddr;
  c: QWord;
  i: LongInt;
begin
  writeln('Admission cookie (SipHash-2-4):');
  cfg := LanternDefaultConfig;
  srv := LanternServerCreate(cfg, True);
  if srv = nil then begin Inc(fails); exit; end;

  FillChar(addr, SizeOf(addr), 0);
  addr.Len := 6;
  for i := 0 to 5 do addr.A[i] := Byte(10 + i);

  c := LanternCookie(srv, addr, 12345, 1);
  Check(LanternCookieVerify(srv, addr, 12345, 1, c),
        'a freshly issued cookie verifies');
  Check(not LanternCookieVerify(srv, addr, 99999, 1, c),
        'a cookie for a different nonce is rejected');
  addr.A[0] := 200;
  Check(not LanternCookieVerify(srv, addr, 12345, 1, c),
        'a cookie for a different address is rejected');

  LanternServerDestroy(srv);
  writeln;
end;

procedure TestWire;
var
  buf: array[0 .. 63] of Byte;
  c: TWireCursor;
  payload: array[0 .. 2] of Byte;
  view: PByte;
begin
  writeln('Wire codec round-trip:');
  payload[0] := $AA; payload[1] := $BB; payload[2] := $CC;

  WireInit(c, @buf[0], SizeOf(buf));
  WirePutU8(c, $12);
  WirePutU16(c, $3456);
  WirePutU32(c, $789ABCDE);
  WirePutU64(c, QWord($0102030405060708));
  WirePutBytes(c, @payload[0], 3);
  Check(not c.Err, 'all writes fit the buffer');

  WireInit(c, @buf[0], SizeOf(buf));
  Check(WireGetU8(c) = $12, 'u8 round-trips');
  Check(WireGetU16(c) = $3456, 'u16 round-trips');
  Check(WireGetU32(c) = $789ABCDE, 'u32 round-trips');
  Check(WireGetU64(c) = QWord($0102030405060708), 'u64 round-trips');
  view := WireGetView(c, 3);
  Check((view <> nil) and (view[0] = $AA) and (view[2] = $CC),
        'bytes view round-trips');
  Check(not c.Err, 'no underrun');
  writeln;
end;

begin
  writeln('== Lantern smoke test ==');
  writeln;
  ShowSizing;
  TestArena;
  TestCookie;
  TestWire;
  if fails = 0 then
    writeln('All checks passed.')
  else begin
    writeln(fails, ' check(s) FAILED.');
    halt(1);
  end;
end.
