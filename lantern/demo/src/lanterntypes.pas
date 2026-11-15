unit LanternTypes;

{ Shared constants and public value types for the Lantern channel library.

  This unit has no dependencies and names no operating system. Everything the
  core, the transport, and the generated message code agree on lives here. }

{$mode objfpc}{$H+}

interface

const
  { ---- Result codes ----
    Success is 0 and generic failure is -1, so a caller that only tests
    "< 0" is still correct without reading this list. The specific causes
    are negative so they never collide with a byte count returned on
    success by routines like SendNext. }
  LanternOK        =  0;
  LanternErr       = -1;
  LanternErrNoMem  = -2;   { arena cannot satisfy the request            }
  LanternErrAgain  = -3;   { nothing to do yet; try later                }
  LanternErrClosed = -4;   { connection is not open                      }
  LanternErrTooBig = -5;   { message or datagram exceeds the MTU budget  }
  LanternErrProto  = -6;   { malformed or unexpected packet              }
  LanternErrFull   = -7;   { no free connection slot (arena at capacity) }

  { ---- Connection states ---- }
  psClosed     = 0;
  psConnecting = 1;   { client: INIT sent, waiting for ACCEPT           }
  psConnected  = 2;
  psClosing    = 3;

  { ---- Channel modes ---- }
  cmReliable   = 0;   { ordered, acked, retransmitted                   }
  cmUnreliable = 1;   { fire and forget                                 }

  { ---- Poll event kinds ---- }
  peNone         = 0;
  peConnected    = 1;
  peDisconnected = 2;
  peData         = 3;   { a channel has a datagram ready to read         }
  peRedirect     = 4;   { lobby handed us off to a game host             }

  { ---- Wire frame types ----
    One leading byte per frame. Multiple frames share one datagram. }
  ftPadding          = $00;
  ftConnectInit      = $01;   { client -> server, carries client epk     }
  ftConnectChallenge = $02;   { server -> client, stateless cookie        }
  ftConnectInit2     = $03;   { client -> server, echoes the cookie       }
  ftConnectAccept    = $04;   { server -> client, slot allocated          }
  ftConnectRefuse    = $05;   { server -> client, arena full              }
  ftDisconnect       = $06;
  ftPing             = $07;
  ftPong             = $08;
  ftChanOpen         = $09;
  ftChanClose        = $0A;
  ftData             = $0B;
  ftAck              = $0C;
  ftWindowUpdate     = $0D;

  { Largest transport address we carry: IPv4 ip(4) + port(2) today, with
    room for an IPv6 ip(16) + port(2) later. The core never interprets the
    bytes; each transport packs its own address and sets len. }
  LanternAddrMax = 18;

type
  { An opaque transport address, compared and copied over the first len
    bytes. This is the seam that keeps the core from naming a sockaddr. }
  TLanternAddr = record
    len: Byte;
    a: array[0 .. LanternAddrMax - 1] of Byte;
  end;

  { Start-time configuration. The arena size is computed from these fields
    and allocated exactly once. Shrink them to spend less RAM and less work
    per tick; a server that wants eight players should not pay for more. }
  TLanternConfig = record
    MaxConns   : Word;   { connection slots to reserve                   }
    MTU        : Word;   { datagram payload budget, e.g. 1200            }
    SendWindow : Word;   { in-flight reliable messages per channel       }
    RecvWindow : Word;   { reorder/hold slots per reliable channel       }
    MaxChan    : Byte;   { channels multiplexed per connection           }
    UnrelTxq   : Byte;   { queued unreliable datagrams per channel       }
  end;

{ A sensible default sized for a small self-hosted match. Callers copy this
  and adjust, rather than filling every field by hand. }
function LanternDefaultConfig: TLanternConfig;

{ Bytewise address compare over the shorter length, then the bytes. }
function LanternAddrEqual(const x, y: TLanternAddr): Boolean;

implementation

function LanternDefaultConfig: TLanternConfig;
begin
  Result.MaxConns   := 8;
  Result.MTU        := 1200;
  Result.SendWindow := 8;
  Result.RecvWindow := 8;
  Result.MaxChan    := 2;
  Result.UnrelTxq   := 4;
end;

function LanternAddrEqual(const x, y: TLanternAddr): Boolean;
var
  i: Longint;
begin
  if x.len <> y.len then exit(False);
  for i := 0 to x.len - 1 do
    if x.a[i] <> y.a[i] then exit(False);
  Result := True;
end;

end.
