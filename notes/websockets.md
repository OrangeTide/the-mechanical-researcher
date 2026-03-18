# WebSocket Notes

## Compression: permessage-deflate (RFC 7692)

WebSocket compression is negotiated during the HTTP upgrade handshake via
`Sec-WebSocket-Extensions: permessage-deflate`. Both sides agree on
parameters (context takeover, window bits), then frames are compressed
with zlib/DEFLATE transparently.

### Reverse proxy handles compression

When running behind a reverse proxy, the proxy negotiates
permessage-deflate with the browser and speaks plain (uncompressed)
WebSocket to the backend. This keeps the backend simple — no zlib
dependency needed.

**nginx:**

```nginx
location /ws {
    proxy_pass http://127.0.0.1:9090;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";

    # strip extensions so backend sees plain WebSocket
    proxy_set_header Sec-WebSocket-Extensions "";
}
```

**caddy:** negotiates permessage-deflate automatically on WebSocket
connections, no configuration needed.

**haproxy:** does not support WebSocket compression natively.

### Compression effectiveness

Text-heavy protocols (SVG markup, space-delimited headers like Quaoar's)
compress well with DEFLATE — typically 60-80% reduction. Binary protocols
or already-compressed data (images, video) see little benefit.

### Context takeover

permessage-deflate supports "context takeover" where the zlib dictionary
persists across messages. This improves compression when messages share
repeated patterns (e.g., SVG attribute names, element tags), but uses
more memory per connection. Most deployments use
`server_no_context_takeover` to trade compression ratio for lower memory.

## Framing

WebSocket frames have a small header (2-14 bytes) with opcode, payload
length, and optional masking key. Client-to-server frames are always
masked (XOR with a 4-byte key). Server-to-client frames are unmasked.

Frame types:
- 0x1: text (UTF-8)
- 0x2: binary
- 0x8: close
- 0x9: ping
- 0xA: pong

Large messages can be fragmented across multiple frames (FIN bit = 0 for
continuation frames, FIN = 1 for the final frame).

## Quaoar implications

- quaoar-server implements WebSocket framing directly (HTTP upgrade,
  SHA-1 handshake, frame read/write) in ~150 lines of C
- no compression in standalone mode — fine for localhost development
- compression handled by reverse proxy in production deployment
- protocol messages are text frames (opcode 0x1)
- no fragmentation support needed — messages are well under 64 KB
  (QU_MAX_MSG = 65536)
