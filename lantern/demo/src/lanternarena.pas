unit LanternArena;

{ One block, carved once.

  The whole library allocates from a single GetMem made at startup. After
  that there is no heap traffic, so the process cannot fragment its heap or
  fail an allocation in the middle of a match. This unit is the generic bump
  allocator; LanternCore decides the layout and computes the total.

  There is no per-allocation free. The arena is freed as a whole when the
  server is destroyed. Reuse of a connection slot is a freelist in the core,
  not a call back into the arena. }

{$mode objfpc}{$H+}

interface

type
  TArena = record
    base : PByte;     { the single GetMem block, nil until created       }
    cap  : PtrUInt;   { total bytes                                      }
    used : PtrUInt;   { bytes handed out so far                          }
  end;

const
  ArenaAlign = 8;     { every allocation starts 8-byte aligned           }

{ Allocate and zero one block of totalBytes. Returns False if GetMem
  fails or totalBytes is zero. }
function ArenaCreate(out a: TArena; totalBytes: PtrUInt): Boolean;

{ Release the whole block. Safe to call on a zeroed/failed arena. }
procedure ArenaDestroy(var a: TArena);

{ Hand out n bytes, aligned. Returns nil if the block is exhausted, which
  only happens if the total was mis-computed, so the caller treats nil as a
  fatal setup error rather than a runtime condition. }
function ArenaAlloc(var a: TArena; n: PtrUInt): Pointer;

function ArenaRemaining(const a: TArena): PtrUInt;

implementation

function AlignUp(v: PtrUInt): PtrUInt;
begin
  Result := (v + (ArenaAlign - 1)) and not PtrUInt(ArenaAlign - 1);
end;

function ArenaCreate(out a: TArena; totalBytes: PtrUInt): Boolean;
begin
  a.base := nil;
  a.cap  := 0;
  a.used := 0;
  if totalBytes = 0 then exit(False);
  a.base := GetMem(totalBytes);
  if a.base = nil then exit(False);
  FillChar(a.base^, totalBytes, 0);
  a.cap := totalBytes;
  Result := True;
end;

procedure ArenaDestroy(var a: TArena);
begin
  if a.base <> nil then
    FreeMem(a.base);
  a.base := nil;
  a.cap  := 0;
  a.used := 0;
end;

function ArenaAlloc(var a: TArena; n: PtrUInt): Pointer;
var
  start: PtrUInt;
begin
  start := AlignUp(a.used);
  if start + n > a.cap then exit(nil);
  Result := a.base + start;
  a.used := start + n;
end;

function ArenaRemaining(const a: TArena): PtrUInt;
begin
  Result := a.cap - a.used;
end;

end.
