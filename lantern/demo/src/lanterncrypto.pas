// SPDX-License-Identifier: 0BSD OR CC0-1.0
unit LanternCrypto;

{ Optional encryption decorator, reused in shape from netchan-v3.

  The core produces plaintext datagrams. This unit seals them after
  LanternSendNext and opens them before LanternFeed, so the core never learns
  it is encrypted. The design is the WireGuard/Noise shape:

    - one X25519 ephemeral exchange per connection;
    - directional keys via BLAKE2b(shared | pk_lo | pk_hi | psk | label),
      with the two public keys sorted so both peers derive the same pair;
    - XChaCha20-Poly1305 per packet with a 64-bit counter nonce, so every
      datagram is independently decryptable;
    - an RFC 6479 sliding replay window;
    - an optional pre-shared key folded into the derivation for a closed game.

  The primitives come from Monocypher, bound as external C symbols and only
  compiled in when LANTERN_WITH_MONOCYPHER is defined, so the default build
  has no external dependency. Everything here is a stub awaiting that
  binding; the signatures are the contract the core calls through. }

{$mode objfpc}{$H+}

interface

uses
  LanternTypes;

const
  CryptoHelloBytes = 1 + 32;         { tag + ephemeral public key          }
  CryptoOverhead   = 1 + 8 + 16;     { tag + counter + Poly1305 tag        }

type
  TLanternKey = array[0 .. 31] of Byte;

  TLanternSession = record
    HaveKeys : Boolean;
    SendKey  : TLanternKey;
    RecvKey  : TLanternKey;
    SendCtr  : QWord;
    ReplayHi : QWord;              { high edge of the replay window         }
    ReplayBits: QWord;            { RFC 6479 bitmap, widened later          }
    { optional long-term identity, all zero when absent (netchan-v4) }
    StaticPub: TLanternKey;
    StaticSec: TLanternKey;
    Psk      : TLanternKey;
  end;

{ Begin a handshake: write our HELLO (tag + ephemeral public key). }
function CryptoHello(var s: TLanternSession; var buf; cap: PtrUInt): LongInt;

{ Consume the peer's HELLO and derive directional keys. }
function CryptoOnHello(var s: TLanternSession; const buf; len: PtrUInt): LongInt;

{ Seal one plaintext datagram in place-ish: writes tag+counter+ciphertext. }
function CryptoSeal(var s: TLanternSession; const plain; plainLen: PtrUInt;
                    var outBuf; cap: PtrUInt): LongInt;

{ Open one sealed datagram, rejecting replays. }
function CryptoOpen(var s: TLanternSession; const sealed; len: PtrUInt;
                    var outBuf; cap: PtrUInt): LongInt;

implementation

function CryptoHello(var s: TLanternSession; var buf; cap: PtrUInt): LongInt;
begin
  { TODO: generate an X25519 ephemeral key pair, write tag $01 and the
    public key. Needs Monocypher crypto_x25519_public_key. }
  Result := LanternErr;
end;

function CryptoOnHello(var s: TLanternSession; const buf; len: PtrUInt): LongInt;
begin
  { TODO: X25519(shared, our_eph_secret, their_eph_public), then two
    BLAKE2b derivations for sendKey/recvKey with the public keys sorted. }
  Result := LanternErr;
end;

function CryptoSeal(var s: TLanternSession; const plain; plainLen: PtrUInt;
                    var outBuf; cap: PtrUInt): LongInt;
begin
  { TODO: XChaCha20-Poly1305 encrypt with nonce = sendCtr, write
    tag $02 + counter + mac + ciphertext, then Inc(sendCtr). }
  Result := LanternErr;
end;

function CryptoOpen(var s: TLanternSession; const sealed; len: PtrUInt;
                    var outBuf; cap: PtrUInt): LongInt;
begin
  { TODO: check the replay window against the counter, decrypt-and-verify,
    and only on success slide the window forward. }
  Result := LanternErr;
end;

end.
