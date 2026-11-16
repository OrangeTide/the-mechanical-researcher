// SPDX-License-Identifier: 0BSD OR CC0-1.0
program lanterngen;

{ The Lantern IDL compiler, written in FreePascal.

  Reads a Pascal-flavored schema and emits a FreePascal unit of packed
  records and (eventually) their Encode/Decode routines. Usage:

    lanterngen schema.pidl -o SchemaProto.pas [-u UnitName]

  State: the lexer and the recursive-descent parser are complete and parse
  the full grammar (enum, message with a Pascal variant part, dispatch). The
  emitter writes the record types for real. The serializer bodies are
  stubbed; generating them from the parsed fields is the remaining work and
  is a mechanical walk of the same AST this file already builds. }

{$mode objfpc}{$H+}

uses
  SysUtils;

const
  MaxEnums   = 64;
  MaxVals    = 64;
  MaxMsgs    = 128;
  MaxFields  = 64;
  MaxArms    = 32;
  MaxDisp    = 32;
  MaxEntries = 64;

{ ---- Token kinds ---- }
const
  ttEOF    = 0;
  ttIdent  = 1;
  ttNumber = 2;
  ttColon  = 3;
  ttSemi   = 4;
  ttLBrack = 5;
  ttRBrack = 6;
  ttLParen = 7;
  ttRParen = 8;
  ttComma  = 9;
  ttEq     = 10;

type
  TTok = record
    Kind : LongInt;
    Text : string;      { lower-cased for idents, so keyword tests are easy }
    Raw  : string;      { original spelling, used for emitted identifiers   }
    Num  : LongInt;
    Line : LongInt;
  end;

  TField = record
    Name    : string;
    Typ     : string;   { idl type or an enum name                         }
    Tag     : LongInt;
  end;

  TArm = record
    Lbl        : string;
    FieldCount : LongInt;
    Fields     : array[0 .. MaxFields - 1] of TField;
  end;

  TMsg = record
    Name       : string;
    FieldCount : LongInt;
    Fields     : array[0 .. MaxFields - 1] of TField;
    HasVariant : Boolean;
    DiscName   : string;
    DiscType   : string;
    DiscTag    : LongInt;
    ArmCount   : LongInt;
    Arms       : array[0 .. MaxArms - 1] of TArm;
  end;

  TEnumVal = record
    Name : string;
    Val  : LongInt;
  end;

  TEnum = record
    Name     : string;
    ValCount : LongInt;
    Vals     : array[0 .. MaxVals - 1] of TEnumVal;
  end;

  TEntry = record
    Tag  : LongInt;
    Msg  : string;
  end;

  TDispatch = record
    Name       : string;
    EntryCount : LongInt;
    Entries    : array[0 .. MaxEntries - 1] of TEntry;
  end;

{ ---- Global state ---- }
var
  src      : string;
  srcPos   : LongInt;
  srcLine  : LongInt;
  cur      : TTok;
  inPath   : string;

  enums    : array[0 .. MaxEnums - 1] of TEnum;
  numEnums : LongInt;
  msgs     : array[0 .. MaxMsgs - 1] of TMsg;
  numMsgs  : LongInt;
  disps    : array[0 .. MaxDisp - 1] of TDispatch;
  numDisps : LongInt;

{ ---- Diagnostics ---- }

procedure Fail(const msg: string);
begin
  writeln(StdErr, inPath, ':', srcLine, ': error: ', msg);
  halt(1);
end;

{ ---- Scanner ---- }

function IsAlpha(c: Char): Boolean;
begin
  Result := ((c >= 'a') and (c <= 'z')) or ((c >= 'A') and (c <= 'Z')) or (c = '_');
end;

function IsDigit(c: Char): Boolean;
begin
  Result := (c >= '0') and (c <= '9');
end;

procedure SkipTrivia;
var
  done: Boolean;
begin
  done := False;
  while not done do begin
    done := True;
    while (srcPos <= Length(src)) and (src[srcPos] in [' ', #9, #13, #10]) do begin
      if src[srcPos] = #10 then Inc(srcLine);
      Inc(srcPos);
    end;
    { brace comment }
    if (srcPos <= Length(src)) and (src[srcPos] = '{') then begin
      while (srcPos <= Length(src)) and (src[srcPos] <> '}') do begin
        if src[srcPos] = #10 then Inc(srcLine);
        Inc(srcPos);
      end;
      if srcPos <= Length(src) then Inc(srcPos);   // consume the closing brace
      done := False;
    end;
    { line comment }
    if (srcPos + 1 <= Length(src)) and (src[srcPos] = '/') and (src[srcPos + 1] = '/') then begin
      while (srcPos <= Length(src)) and (src[srcPos] <> #10) do Inc(srcPos);
      done := False;
    end;
  end;
end;

procedure NextToken;
var
  start: LongInt;
  s: string;
begin
  SkipTrivia;
  cur.Line := srcLine;
  if srcPos > Length(src) then begin
    cur.Kind := ttEOF;
    cur.Text := '';
    exit;
  end;
  case src[srcPos] of
    ':': begin cur.Kind := ttColon;  Inc(srcPos); end;
    ';': begin cur.Kind := ttSemi;   Inc(srcPos); end;
    '[': begin cur.Kind := ttLBrack; Inc(srcPos); end;
    ']': begin cur.Kind := ttRBrack; Inc(srcPos); end;
    '(': begin cur.Kind := ttLParen; Inc(srcPos); end;
    ')': begin cur.Kind := ttRParen; Inc(srcPos); end;
    ',': begin cur.Kind := ttComma;  Inc(srcPos); end;
    '=': begin cur.Kind := ttEq;     Inc(srcPos); end;
  else
    if IsDigit(src[srcPos]) then begin
      start := srcPos;
      while (srcPos <= Length(src)) and IsDigit(src[srcPos]) do Inc(srcPos);
      s := Copy(src, start, srcPos - start);
      cur.Kind := ttNumber;
      cur.Num := StrToInt(s);
      cur.Text := s;
    end else if IsAlpha(src[srcPos]) then begin
      start := srcPos;
      while (srcPos <= Length(src)) and (IsAlpha(src[srcPos]) or IsDigit(src[srcPos])) do
        Inc(srcPos);
      s := Copy(src, start, srcPos - start);
      cur.Kind := ttIdent;
      cur.Raw := s;
      cur.Text := LowerCase(s);
    end else
      Fail('unexpected character "' + src[srcPos] + '"');
  end;
end;

function IsKw(const kw: string): Boolean;
begin
  Result := (cur.Kind = ttIdent) and (cur.Text = kw);
end;

procedure Expect(kind: LongInt; const what: string);
begin
  if cur.Kind <> kind then
    Fail('expected ' + what);
  NextToken;
end;

procedure ExpectKw(const kw: string);
begin
  if not IsKw(kw) then
    Fail('expected "' + kw + '"');
  NextToken;
end;

function ExpectIdent: string;
begin
  if cur.Kind <> ttIdent then Fail('expected an identifier');
  Result := cur.Raw;
  NextToken;
end;

function ExpectTag: LongInt;
begin
  { "[" number "]" }
  Expect(ttLBrack, '"["');
  if cur.Kind <> ttNumber then Fail('expected a wire tag number');
  Result := cur.Num;
  NextToken;
  Expect(ttRBrack, '"]"');
end;

{ ---- Parser ---- }

procedure ParseField(var f: TField);
begin
  f.Name := ExpectIdent;
  Expect(ttColon, '":"');
  f.Typ := ExpectIdent;
  f.Tag := ExpectTag;
  { The separating ";" is optional, so the last field before ")" or "end"
    may omit it, the way a Pascal variant record does. }
  if cur.Kind = ttSemi then NextToken;
end;

procedure ParseEnum(const nm: string);
var
  e: ^TEnum;
begin
  if numEnums >= MaxEnums then Fail('too many enums');
  e := @enums[numEnums];
  e^.Name := nm;
  e^.ValCount := 0;
  Expect(ttLParen, '"("');
  repeat
    if e^.ValCount >= MaxVals then Fail('too many enum values');
    e^.Vals[e^.ValCount].Name := ExpectIdent;
    Expect(ttEq, '"="');
    if cur.Kind <> ttNumber then Fail('expected an enum value number');
    e^.Vals[e^.ValCount].Val := cur.Num;
    NextToken;
    Inc(e^.ValCount);
    if cur.Kind = ttComma then NextToken
    else Break;
  until False;
  Expect(ttRParen, '")"');
  Expect(ttSemi, '";"');
  Inc(numEnums);
end;

procedure ParseMessage(const nm: string);
var
  m: ^TMsg;
  arm: ^TArm;
begin
  if numMsgs >= MaxMsgs then Fail('too many messages');
  m := @msgs[numMsgs];
  m^.Name := nm;
  m^.FieldCount := 0;
  m^.HasVariant := False;
  m^.ArmCount := 0;
  ExpectKw('message');
  { fixed fields until "case" or "end" }
  while (cur.Kind = ttIdent) and (not IsKw('case')) and (not IsKw('end')) do begin
    if m^.FieldCount >= MaxFields then Fail('too many fields');
    ParseField(m^.Fields[m^.FieldCount]);
    Inc(m^.FieldCount);
  end;
  if IsKw('case') then begin
    m^.HasVariant := True;
    NextToken;
    m^.DiscName := ExpectIdent;
    Expect(ttColon, '":"');
    m^.DiscType := ExpectIdent;
    m^.DiscTag := ExpectTag;
    ExpectKw('of');
    while not IsKw('end') do begin
      if m^.ArmCount >= MaxArms then Fail('too many variant arms');
      arm := @m^.Arms[m^.ArmCount];
      arm^.Lbl := ExpectIdent;
      arm^.FieldCount := 0;
      Expect(ttColon, '":"');
      Expect(ttLParen, '"("');
      while cur.Kind = ttIdent do begin
        if arm^.FieldCount >= MaxFields then Fail('too many arm fields');
        ParseField(arm^.Fields[arm^.FieldCount]);
        Inc(arm^.FieldCount);
      end;
      Expect(ttRParen, '")"');
      Expect(ttSemi, '";"');
      Inc(m^.ArmCount);
    end;
    ExpectKw('end');
    Expect(ttSemi, '";"');
  end;
  ExpectKw('end');
  Expect(ttSemi, '";"');
  Inc(numMsgs);
end;

procedure ParseDispatch;
var
  d: ^TDispatch;
begin
  if numDisps >= MaxDisp then Fail('too many dispatch tables');
  d := @disps[numDisps];
  ExpectKw('dispatch');
  d^.Name := ExpectIdent;
  d^.EntryCount := 0;
  while cur.Kind = ttNumber do begin
    if d^.EntryCount >= MaxEntries then Fail('too many dispatch entries');
    d^.Entries[d^.EntryCount].Tag := cur.Num;
    NextToken;
    Expect(ttColon, '":"');
    d^.Entries[d^.EntryCount].Msg := ExpectIdent;
    Expect(ttSemi, '";"');
    Inc(d^.EntryCount);
  end;
  ExpectKw('end');
  if cur.Kind = ttSemi then NextToken;   { a trailing ";" is allowed }
  Inc(numDisps);
end;

var
  unitName: string;

procedure ParseUnit;
var
  nm: string;
begin
  ExpectKw('unit');
  unitName := ExpectIdent;
  Expect(ttSemi, '";"');
  ExpectKw('type');
  while cur.Kind <> ttEOF do begin
    if IsKw('dispatch') then
      ParseDispatch
    else begin
      nm := ExpectIdent;
      Expect(ttEq, '"="');
      if cur.Kind = ttLParen then
        ParseEnum(nm)
      else if IsKw('message') then
        ParseMessage(nm)
      else
        Fail('expected "(" for an enum or "message"');
    end;
  end;
end;

{ ---- Emitter ---- }

function PasType(const t: string): string;
var
  lt: string;
begin
  lt := LowerCase(t);
  if lt = 'uint8'   then Result := 'Byte'
  else if lt = 'int8'    then Result := 'ShortInt'
  else if lt = 'uint16'  then Result := 'Word'
  else if lt = 'int16'   then Result := 'SmallInt'
  else if lt = 'uint32'  then Result := 'LongWord'
  else if lt = 'int32'   then Result := 'LongInt'
  else if lt = 'uint64'  then Result := 'QWord'
  else if lt = 'int64'   then Result := 'Int64'
  else if lt = 'bool'    then Result := 'Boolean'
  else if lt = 'float32' then Result := 'Single'
  else if lt = 'float64' then Result := 'Double'
  else if lt = 'bytes'   then Result := 'TLanternView'
  else if lt = 'string'  then Result := 'TLanternView'
  else Result := t;      { an enum name, emitted as-is }
end;

var
  outF: Text;

procedure EmitField(const f: TField; indent: string);
begin
  writeln(outF, indent, f.Name, ' : ', PasType(f.Typ),
          '; { tag ', f.Tag, ' }');
end;

procedure EmitMsgType(const m: TMsg);
var
  i, j: LongInt;
begin
  writeln(outF, '  ', m.Name, ' = packed record');
  for i := 0 to m.FieldCount - 1 do
    EmitField(m.Fields[i], '    ');
  if m.HasVariant then begin
    writeln(outF, '    case ', m.DiscName, ' : ', m.DiscType,
            ' of   { tag ', m.DiscTag, ' }');
    for i := 0 to m.ArmCount - 1 do begin
      write(outF, '      ', m.Arms[i].Lbl, ': (');
      if m.Arms[i].FieldCount = 0 then
        writeln(outF, ' );')
      else begin
        writeln(outF);
        for j := 0 to m.Arms[i].FieldCount - 1 do
          EmitField(m.Arms[i].Fields[j], '        ');
        writeln(outF, '      );');
      end;
    end;
  end;
  writeln(outF, '  end;');
  writeln(outF);
end;

procedure EmitEnum(const e: TEnum);
var
  i: LongInt;
begin
  write(outF, '  ', e.Name, ' = (');
  for i := 0 to e.ValCount - 1 do begin
    write(outF, e.Vals[i].Name, ' = ', e.Vals[i].Val);
    if i < e.ValCount - 1 then write(outF, ', ');
  end;
  writeln(outF, ');');
end;

procedure EmitUnit(const outPath: string);
var
  i: LongInt;
begin
  Assign(outF, outPath);
  Rewrite(outF);
  writeln(outF, 'unit ', unitName, ';');
  writeln(outF);
  writeln(outF, '{ Generated by lanterngen from ', ExtractFileName(inPath), '. Do not edit. }');
  writeln(outF);
  writeln(outF, '{$mode objfpc}{$H+}');
  writeln(outF);
  writeln(outF, 'interface');
  writeln(outF);
  writeln(outF, 'uses');
  writeln(outF, '  LanternWire;');
  writeln(outF);
  writeln(outF, 'type');
  writeln(outF, '  { A zero-copy view into the decode buffer. }');
  writeln(outF, '  TLanternView = record P: PByte; N: LongWord; end;');
  writeln(outF);
  for i := 0 to numEnums - 1 do
    EmitEnum(enums[i]);
  if numEnums > 0 then writeln(outF);
  for i := 0 to numMsgs - 1 do
    EmitMsgType(msgs[i]);

  { Serializer signatures. Bodies are stubs: generating them from the parsed
    fields is the remaining work. }
  for i := 0 to numMsgs - 1 do begin
    writeln(outF, 'function Encode', msgs[i].Name,
            '(var Buf; Cap: SizeInt; const M: ', msgs[i].Name, '): SizeInt;');
    writeln(outF, 'function Decode', msgs[i].Name,
            '(const Buf; Len: SizeInt; out M: ', msgs[i].Name, '): Boolean;');
  end;
  writeln(outF);
  writeln(outF, 'implementation');
  writeln(outF);
  for i := 0 to numMsgs - 1 do begin
    writeln(outF, 'function Encode', msgs[i].Name,
            '(var Buf; Cap: SizeInt; const M: ', msgs[i].Name, '): SizeInt;');
    writeln(outF, 'begin');
    writeln(outF, '  { TODO: generated body walks the fields in tag order. }');
    writeln(outF, '  Result := -1;');
    writeln(outF, 'end;');
    writeln(outF);
    writeln(outF, 'function Decode', msgs[i].Name,
            '(const Buf; Len: SizeInt; out M: ', msgs[i].Name, '): Boolean;');
    writeln(outF, 'begin');
    writeln(outF, '  FillChar(M, SizeOf(M), 0);');
    writeln(outF, '  { TODO: generated body skips unknown tags, tolerates truncation. }');
    writeln(outF, '  Result := False;');
    writeln(outF, 'end;');
    writeln(outF);
  end;
  writeln(outF, 'end.');
  Close(outF);
end;

{ ---- Driver ---- }

procedure LoadSource(const path: string);
var
  f: File;
  n: LongInt;
begin
  AssignFile(f, path);
  {$I-} Reset(f, 1); {$I+}
  if IOResult <> 0 then begin
    writeln(StdErr, 'lanterngen: cannot open ', path);
    halt(1);
  end;
  n := FileSize(f);
  SetLength(src, n);
  if n > 0 then
    BlockRead(f, src[1], n);
  CloseFile(f);
end;

var
  i: LongInt;
  outPath, forcedUnit: string;
begin
  inPath := '';
  outPath := '';
  forcedUnit := '';
  i := 1;
  while i <= ParamCount do begin
    if ParamStr(i) = '-o' then begin
      Inc(i);
      if i > ParamCount then Fail('-o needs an argument');
      outPath := ParamStr(i);
    end else if ParamStr(i) = '-u' then begin
      Inc(i);
      if i > ParamCount then Fail('-u needs an argument');
      forcedUnit := ParamStr(i);
    end else
      inPath := ParamStr(i);
    Inc(i);
  end;
  if inPath = '' then begin
    writeln(StdErr, 'usage: lanterngen schema.pidl -o Output.pas [-u UnitName]');
    halt(2);
  end;
  if outPath = '' then
    outPath := ChangeFileExt(inPath, '.pas');

  LoadSource(inPath);
  srcPos := 1;
  srcLine := 1;
  numEnums := 0;
  numMsgs := 0;
  numDisps := 0;
  NextToken;
  ParseUnit;
  if forcedUnit <> '' then unitName := forcedUnit;

  EmitUnit(outPath);
  writeln('lanterngen: wrote ', outPath, ' (', numEnums, ' enums, ',
          numMsgs, ' messages, ', numDisps, ' dispatch tables)');
end.
