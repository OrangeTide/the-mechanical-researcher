# Write Your Own Web-Based Media Library Manager in C

## Introduction

Article about implementing a web-based media library. It should be informative
to the reader, and provide enough information that a reader could implement
their own from scratch or continue where we left off.

## Goals

  * Quickly ingest a media collection (music, videos, ebooks (PDF,epub,CBZ,etc),  ROMs(with or without zip), game ISO/ZIP/RAR, etc.)
    * by quickly - the rest of the site should still be interactive.
    * ideally, the site should see some somewhat real-time updates even if a very large batch of files are being imported.
  * Preserving user data is paramount. The original collection is often a primary copy and we should not modify the user's files or attempt to reorging them. Be very defensive when it comes to bugs that could lead to data loss or corruption.
    * if the imported folder hierarchy is not ideal for organization, then we should have a database on the side that maps our virtual file system to the real files. Or use safe copy in a content-addressable-storage system. (perhaps difficult for very large files, without an advanced filesystem)
  * Identify and leverage frameworks, third-party tools, and references that will speed the design and development process along.

## Features

  * Music library as primary feature.
    * Organize listings by various groups and tags.
    * User-constructed views that show or hide certain tags. (GUI tool for constructing the Boolean search expression)
    * Multiple profiles/accounts with different preferences.
  * Video library as supported feature, but perhaps less polished.
  * **Ingest model:** Read-only scan — index file paths + extracted metadata into SQLite, serve originals directly via Civetweb range requests. Never copy, move, or modify user files.
  * **Metadata extraction:** Vendored libid3tag for audio tags (ID3v1/v2). ffprobe as optional fallback for video metadata and edge cases.
  * **Ingest progress:** Server-Sent Events (SSE) via Civetweb chunked responses (`mg_printf`/`mg_send_chunk` with `text/event-stream`). Thread-per-connection model is fine for local use with few clients. Reference: https://hpbn.co/server-sent-events-sse/
  * **Future extensions (discussed in article, not in demo):** CAS with reflink/copy-on-write, symlink farms, hardlink dedup. These are over-designed for a weekend project but worth knowing about.
  * Minimal but elegant UI. Use simple controls and simply layout. Text and icon-based interfaces, with the album art and screen captures being the primary art. saving us from having to create any art ourselves.
  * in-browser Media playback. User saved playlists possible (for at least the music and video media types)
  * Themeable UI: let users make it their own with colors, dark/light mode, custom fonts, hide/disable unwanted elements, etc.

## Decisions

  * **Backend:** Civetweb (C) — vendored single-file (`civetweb.c` + `civetweb.h`), MIT license. Built-in range requests (HTTP 206), WebSockets, MIME types, static file serving. Self-contained and will compile with any C compiler for decades.
  * **Database:** SQLite — vendored amalgamation (`sqlite3.c` + `sqlite3.h`). Single-file, zero-config, perfect fit for self-contained C project.
  * **Frontend:** Preact + HTM — React-like components with no build step (htm tagged templates loaded from CDN or vendored). Keeps the "weekend project" feel.
  * **Scope:** Hybrid — working demo covers core (ingest, browse, play music+video), article covers design rationale for extending (ebooks, ROMs, CAS, theming, playlists, multi-user).
  * **Media focus:** Music (primary) + Video (secondary). Architecture designed to generalize to other types.

### Considered Alternatives

  * Zap (Zig) — io_uring performance, but Zig pre-1.0 with frequent breaking changes, no range request support, poor vendoring story. Performance irrelevant for local media server.
  * cpp-httplib (C++) — header-only, range requests, but no WebSocket support.
  * libmicrohttpd (C) — GNU, battle-tested, security audited, but lower-level (more wiring needed), WebSocket support removed in v1.0.2.
  * lwan (C) — extremely fast and tiny, but no range requests or WebSockets.
  * Rust frameworks (Axum, Warp, Actix-web) — good but Cargo dependency tree is hard to vendor for long-term reproducibility.
  * Node/Deno — large runtime, not self-contained.
  * facil.io (C) — frequent breaking changes in C STL branch.

## Audience

  * Mid-level experience developer, looking for a project where it is possible to have a useful result in a weekend of work (4-12 hours).
