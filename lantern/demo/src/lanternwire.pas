unit LanternWire;

{ Little-endian read/write over a bounded buffer.

  Every target CPU is little-endian, so the wire is little-endian and the
  hot path does no byte swapping. A cursor carries the buffer, its capacity,
  the current position, and a sticky error flag. Once a put overflows or a
  get underruns, err stays set and every later call is a cheap no-op, so a
  caller can write a whole message and check err once at the end. This is
  the runtime the generated Encode/Decode routines are built on. }

{$mode objfpc}{$H+}

interface

type
  TWireCursor = record
    buf : PByte;      { base of the buffer                              }
    cap : PtrUInt;    { bytes available from base                       }
    pos : PtrUInt;    { next byte to read or write                      }
    err : Boolean;    { sticky: set on overflow or underrun             }
  end;

procedure WireInit(out c: TWireCursor; buf: Pointer; cap: PtrUInt);

procedure WirePutU8 (var c: TWireCursor; v: Byte);
procedure WirePutU16(var c: TWireCursor; v: Word);
procedure WirePutU32(var c: TWireCursor; v: LongWord);
procedure WirePutU64(var c: TWireCursor; v: QWord);
procedure WirePutBytes(var c: TWireCursor; src: Pointer; n: PtrUInt);

function WireGetU8 (var c: TWireCursor): Byte;
function WireGetU16(var c: TWireCursor): Word;
function WireGetU32(var c: TWireCursor): LongWord;
function WireGetU64(var c: TWireCursor): QWord;

{ Return a pointer to n bytes in place and advance past them. Used for
  zero-copy bytes/string fields: the view is valid until the buffer is
  reused. Returns nil (and sets err) if fewer than n bytes remain. }
function WireGetView(var c: TWireCursor; n: PtrUInt): Pointer;

implementation

procedure WireInit(out c: TWireCursor; buf: Pointer; cap: PtrUInt);
begin
  c.buf := PByte(buf);
  c.cap := cap;
  c.pos := 0;
  c.err := False;
end;

{ ---- Writers ---- }

procedure WirePutU8(var c: TWireCursor; v: Byte);
begin
  if c.err or (c.pos + 1 > c.cap) then begin
    c.err := True;
    exit;
  end;
  c.buf[c.pos] := v;
  Inc(c.pos);
end;

procedure WirePutU16(var c: TWireCursor; v: Word);
begin
  WirePutU8(c, Byte(v));
  WirePutU8(c, Byte(v shr 8));
end;

procedure WirePutU32(var c: TWireCursor; v: LongWord);
begin
  WirePutU16(c, Word(v));
  WirePutU16(c, Word(v shr 16));
end;

procedure WirePutU64(var c: TWireCursor; v: QWord);
begin
  WirePutU32(c, LongWord(v));
  WirePutU32(c, LongWord(v shr 32));
end;

procedure WirePutBytes(var c: TWireCursor; src: Pointer; n: PtrUInt);
begin
  if c.err or (c.pos + n > c.cap) then begin
    c.err := True;
    exit;
  end;
  if n > 0 then
    Move(src^, c.buf[c.pos], n);
  Inc(c.pos, n);
end;

{ ---- Readers ---- }

function WireGetU8(var c: TWireCursor): Byte;
begin
  if c.err or (c.pos + 1 > c.cap) then begin
    c.err := True;
    exit(0);
  end;
  Result := c.buf[c.pos];
  Inc(c.pos);
end;

function WireGetU16(var c: TWireCursor): Word;
var
  lo, hi: Byte;
begin
  lo := WireGetU8(c);
  hi := WireGetU8(c);
  Result := Word(lo) or (Word(hi) shl 8);
end;

function WireGetU32(var c: TWireCursor): LongWord;
var
  lo, hi: Word;
begin
  lo := WireGetU16(c);
  hi := WireGetU16(c);
  Result := LongWord(lo) or (LongWord(hi) shl 16);
end;

function WireGetU64(var c: TWireCursor): QWord;
var
  lo, hi: LongWord;
begin
  lo := WireGetU32(c);
  hi := WireGetU32(c);
  Result := QWord(lo) or (QWord(hi) shl 32);
end;

function WireGetView(var c: TWireCursor; n: PtrUInt): Pointer;
begin
  if c.err or (c.pos + n > c.cap) then begin
    c.err := True;
    exit(nil);
  end;
  Result := @c.buf[c.pos];
  Inc(c.pos, n);
end;

end.
