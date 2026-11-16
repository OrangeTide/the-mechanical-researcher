// SPDX-License-Identifier: 0BSD OR CC0-1.0
unit LanternUDP;

{ A thin, optional UDP transport for Linux, macOS, and Windows.

  The core owns no socket; this helper is convenience glue. It maps a
  TLanternAddr (IPv4: ip(4) network order + port(2) network order, len 6) to
  and from a sockaddr, opens a non-blocking datagram socket, and drains it.
  Only this unit is platform-specific, and it is small because the opaque
  address keeps the core from ever naming a sockaddr. The Unix path is
  compile-tested; the Windows path mirrors it through winsock2 and is marked
  for verification. }

{$mode objfpc}{$H+}

interface

uses
  LanternTypes;

type
  TLanternSocket = record
    fd    : LongInt;
    valid : Boolean;
  end;

{ Open a UDP socket. If bindPort <> 0 the socket is bound to it (a server);
  otherwise it is left unbound (a client). Non-blocking either way. }
function UdpOpen(out sk: TLanternSocket; bindPort: Word): Boolean;
procedure UdpClose(var sk: TLanternSocket);

{ Send to a TLanternAddr. Returns bytes sent or a negative error. }
function UdpSend(var sk: TLanternSocket; const buf; len: PtrUInt;
                const dest: TLanternAddr): LongInt;

{ Receive one datagram. Returns bytes read (0 if none are waiting), filling
  'from'. Non-blocking: 0 means try again later, not end of stream. }
function UdpRecv(var sk: TLanternSocket; var buf; cap: PtrUInt;
                out from: TLanternAddr): LongInt;

{ Build a TLanternAddr from dotted-quad text and a host-order port. }
function UdpMakeAddr(const ip: string; port: Word; out addr: TLanternAddr): Boolean;

implementation

uses
  {$IFDEF UNIX} BaseUnix, Sockets {$ENDIF}
  {$IFDEF WINDOWS} WinSock2 {$ENDIF};

function UdpMakeAddr(const ip: string; port: Word; out addr: TLanternAddr): Boolean;
var
  n: LongWord;
begin
  FillChar(addr, SizeOf(addr), 0);
  n := LongWord(StrToNetAddr(ip));        { returns in network byte order }
  if (n = 0) and (ip <> '0.0.0.0') then exit(False);
  addr.len := 6;
  addr.a[0] := Byte(n);
  addr.a[1] := Byte(n shr 8);
  addr.a[2] := Byte(n shr 16);
  addr.a[3] := Byte(n shr 24);
  addr.a[4] := Byte(port shr 8);          { port on the wire is big-endian }
  addr.a[5] := Byte(port);
  Result := True;
end;

procedure AddrToSockAddr(const addr: TLanternAddr; out sa: TInetSockAddr);
begin
  FillChar(sa, SizeOf(sa), 0);
  sa.sin_family := AF_INET;
  { a[0..3] already hold the IP in network order; a[4..5] the port in
    network order. Copy them straight across. }
  PByte(@sa.sin_addr)[0] := addr.a[0];
  PByte(@sa.sin_addr)[1] := addr.a[1];
  PByte(@sa.sin_addr)[2] := addr.a[2];
  PByte(@sa.sin_addr)[3] := addr.a[3];
  PByte(@sa.sin_port)[0] := addr.a[4];
  PByte(@sa.sin_port)[1] := addr.a[5];
end;

procedure SockAddrToAddr(const sa: TInetSockAddr; out addr: TLanternAddr);
begin
  FillChar(addr, SizeOf(addr), 0);
  addr.len := 6;
  addr.a[0] := PByte(@sa.sin_addr)[0];
  addr.a[1] := PByte(@sa.sin_addr)[1];
  addr.a[2] := PByte(@sa.sin_addr)[2];
  addr.a[3] := PByte(@sa.sin_addr)[3];
  addr.a[4] := PByte(@sa.sin_port)[0];
  addr.a[5] := PByte(@sa.sin_port)[1];
end;

{$IFDEF UNIX}
function UdpOpen(out sk: TLanternSocket; bindPort: Word): Boolean;
var
  fd, fl: LongInt;
  sa: TInetSockAddr;
begin
  sk.valid := False;
  sk.fd := -1;
  fd := fpSocket(AF_INET, SOCK_DGRAM, 0);
  if fd < 0 then exit(False);
  fl := fpFcntl(fd, F_GETFL, 0);
  fpFcntl(fd, F_SETFL, fl or O_NONBLOCK);
  if bindPort <> 0 then begin
    FillChar(sa, SizeOf(sa), 0);
    sa.sin_family := AF_INET;
    sa.sin_addr.s_addr := 0;             { INADDR_ANY }
    PByte(@sa.sin_port)[0] := Byte(bindPort shr 8);
    PByte(@sa.sin_port)[1] := Byte(bindPort);
    if fpBind(fd, @sa, SizeOf(sa)) <> 0 then begin
      fpClose(fd);
      exit(False);
    end;
  end;
  sk.fd := fd;
  sk.valid := True;
  Result := True;
end;

procedure UdpClose(var sk: TLanternSocket);
begin
  if sk.valid then fpClose(sk.fd);
  sk.valid := False;
  sk.fd := -1;
end;

function UdpSend(var sk: TLanternSocket; const buf; len: PtrUInt;
                const dest: TLanternAddr): LongInt;
var
  sa: TInetSockAddr;
  r: ssize_t;
begin
  if not sk.valid then exit(LanternErr);
  AddrToSockAddr(dest, sa);
  r := fpSendTo(sk.fd, @buf, len, 0, @sa, SizeOf(sa));
  Result := LongInt(r);
end;

function UdpRecv(var sk: TLanternSocket; var buf; cap: PtrUInt;
                out from: TLanternAddr): LongInt;
var
  sa: TInetSockAddr;
  sl: TSockLen;
  r: ssize_t;
begin
  FillChar(from, SizeOf(from), 0);
  if not sk.valid then exit(LanternErr);
  sl := SizeOf(sa);
  r := fpRecvFrom(sk.fd, @buf, cap, 0, @sa, @sl);
  if r < 0 then begin
    if fpGetErrno = ESysEAGAIN then exit(0);   { nothing waiting }
    exit(LanternErr);
  end;
  SockAddrToAddr(sa, from);
  Result := LongInt(r);
end;
{$ENDIF}

{$IFDEF WINDOWS}
{ TODO: verify this path on Windows. The calls mirror the Unix branch
  through winsock2: WSAStartup once at program start, socket/bind/sendto/
  recvfrom, ioctlsocket(FIONBIO) for non-blocking, closesocket to close,
  and WSAGetLastError = WSAEWOULDBLOCK for "nothing waiting". }
function UdpOpen(out sk: TLanternSocket; bindPort: Word): Boolean;
begin
  sk.valid := False;
  sk.fd := -1;
  Result := False;
end;

procedure UdpClose(var sk: TLanternSocket);
begin
  sk.valid := False;
end;

function UdpSend(var sk: TLanternSocket; const buf; len: PtrUInt;
                const dest: TLanternAddr): LongInt;
begin
  Result := LanternErr;
end;

function UdpRecv(var sk: TLanternSocket; var buf; cap: PtrUInt;
                out from: TLanternAddr): LongInt;
begin
  FillChar(from, SizeOf(from), 0);
  Result := LanternErr;
end;
{$ENDIF}

end.
