// SPDX-License-Identifier: 0BSD OR CC0-1.0
unit LanternAuth;

{ Optional peer authentication, reused in shape from netchan-v4.

  Two layers sit on top of LanternCrypto:

    - A server long-term X25519 host key, carried in the handshake as a
      static identity and authenticated by the es Diffie-Hellman step rather
      than a signature. This is Noise's NX pattern: no signature on the wire,
      no extra round trip. The client verifies it trust-on-first-use against
      a known_hosts file and warns loudly if a remembered key ever changes.

    - A login step where the client offers an Ed25519 public key, signing a
      digest that includes the crypto session id so a signature cannot be
      replayed to another server, with a password as the fallback.

  All stubs for now. The host-key store is a text file, one host and key per
  line, read and appended without any heap growth during a session. }

{$mode objfpc}{$H+}

interface

uses
  LanternTypes, LanternCrypto;

const
  khOK       = 0;   { host key matches a remembered entry                  }
  khNew      = 1;   { host not seen before; caller decides to trust (TOFU) }
  khChanged  = 2;   { host known but key differs: refuse and warn          }

{ Check a presented host key against known_hosts. Returns one of kh*. }
function AuthCheckHostKey(const hostName: string; const key: TLanternKey): LongInt;

{ Append a newly trusted host key to known_hosts. }
function AuthRememberHostKey(const hostName: string; const key: TLanternKey): Boolean;

{ Client side: produce a login offer (pubkey signature or password). }
function AuthClientLogin(var s: TLanternSession; var buf; cap: PtrUInt): LongInt;

{ Server side: verify a login offer against an authorized-keys list. }
function AuthServerVerify(var s: TLanternSession; const buf; len: PtrUInt): LongInt;

implementation

function AuthCheckHostKey(const hostName: string; const key: TLanternKey): LongInt;
begin
  { TODO: scan known_hosts for hostName; compare the 32-byte key. }
  Result := khNew;
end;

function AuthRememberHostKey(const hostName: string; const key: TLanternKey): Boolean;
begin
  { TODO: append "hostName base64(key)" to known_hosts. }
  Result := False;
end;

function AuthClientLogin(var s: TLanternSession; var buf; cap: PtrUInt): LongInt;
begin
  { TODO: Ed25519-sign a digest that includes the session id, or fall back
    to sending a password over the already-encrypted channel. }
  Result := LanternErr;
end;

function AuthServerVerify(var s: TLanternSession; const buf; len: PtrUInt): LongInt;
begin
  { TODO: verify the signature against authorized_keys, or check the
    password. }
  Result := LanternErr;
end;

end.
