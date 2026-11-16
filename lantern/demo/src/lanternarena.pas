// SPDX-License-Identifier: 0BSD OR CC0-1.0
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
    Base : PByte;     { the single GetMem block, nil until created       }
    Cap  : PtrUInt;   { total bytes                                      }
    Used : PtrUInt;   { bytes handed out so far                          }
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
  a.Base := nil;
  a.Cap  := 0;
  a.Used := 0;
  if totalBytes = 0 then exit(False);
  a.Base := GetMem(totalBytes);
  if a.Base = nil then exit(False);
  FillChar(a.Base^, totalBytes, 0);
  a.Cap := totalBytes;
  Result := True;
end;

procedure ArenaDestroy(var a: TArena);
begin
  if a.Base <> nil then
    FreeMem(a.Base);
  a.Base := nil;
  a.Cap  := 0;
  a.Used := 0;
end;

function ArenaAlloc(var a: TArena; n: PtrUInt): Pointer;
var
  start: PtrUInt;
begin
  start := AlignUp(a.Used);
  if start + n > a.Cap then exit(nil);
  Result := a.Base + start;
  a.Used := start + n;
end;

function ArenaRemaining(const a: TArena): PtrUInt;
begin
  Result := a.Cap - a.Used;
end;

end.
