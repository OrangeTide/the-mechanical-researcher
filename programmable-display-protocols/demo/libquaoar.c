/* libquaoar.c : client library implementation */
/* Copyright (c) 2026 — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

#include "libquaoar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>

#define MAX_HANDLERS  256
#define BUF_SIZE      (QU_MAX_MSG * 2)

/****************************************************************
 * Internal types
 ****************************************************************/

struct handler {
    int id;
    qu_event_fn fn;
    void *arg;
};

struct qu_ctx {
    int fd;
    int next_id;
    uint8_t rbuf[BUF_SIZE];
    int rlen;
    struct handler handlers[MAX_HANDLERS];
    int nhandlers;
};

/****************************************************************
 * Netstring framing
 ****************************************************************/

/** Send data as a netstring: "len:data," */
static int
ns_send(qu_ctx *ctx, const char *data, int len)
{
    char hdr[16];
    int hlen = snprintf(hdr, sizeof(hdr), "%d:", len);
    int total = hlen + len + 1;
    uint8_t frame[QU_MAX_MSG + 16];

    if (total > (int)sizeof(frame))
        return -1;
    memcpy(frame, hdr, hlen);
    memcpy(frame + hlen, data, len);
    frame[hlen + len] = ',';

    int sent = 0;
    while (sent < total) {
        int w = write(ctx->fd, frame + sent, total - sent);
        if (w <= 0)
            return -1;
        sent += w;
    }

    return 0;
}

/** Extract one netstring from buf. Returns payload length or -1. */
static int
ns_decode(const uint8_t *buf, int blen, uint8_t *msg, int max,
      int *consumed)
{
    int colon = -1;

    for (int i = 0; i < blen && i < 10; i++) {
        if (buf[i] == ':') {
            colon = i;
            break;
        }
        if (buf[i] < '0' || buf[i] > '9')
            return -1;
    }
    if (colon < 1)
        return -1;
    if (colon > 1 && buf[0] == '0')
        return -1; /* leading zero */

    int len = 0;
    for (int i = 0; i < colon; i++)
        len = len * 10 + (buf[i] - '0');

    int total = colon + 1 + len + 1;
    if ((blen < total) || /* incomplete */
        (buf[total - 1] != ',') ||
        (len > max))
        return -1;

    memcpy(msg, buf + colon + 1, len);
    *consumed = total;

    return len;
}

/****************************************************************
 * Send helpers
 ****************************************************************/

static int
qu_send(qu_ctx *ctx, const char *fmt, ...)
{
    char buf[QU_MAX_MSG];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n <= 0 || n >= (int)sizeof(buf))
        return -1;

    return ns_send(ctx, buf, n);
}

/** Escape a string for XML text content. */
static int
xml_escape(const char *in, char *out, int max)
{
    int p = 0;

    for (int i = 0; in[i] && p < max - 6; i++) {
        switch (in[i]) {
        case '<':  p += snprintf(out + p, max - p, "&lt;"); break;
        case '>':  p += snprintf(out + p, max - p, "&gt;"); break;
        case '&':  p += snprintf(out + p, max - p, "&amp;"); break;
        default:   out[p++] = in[i];
        }
    }
    out[p] = 0;

    return p;
}

/****************************************************************
 * Event dispatch
 ****************************************************************/

/** Parse "event id type\nvalue" and dispatch to handler. */
static void
dispatch_event(qu_ctx *ctx, const char *msg, int len)
{
    int pos = 0, id = 0;
    char event[64];
    int ei = 0;

    /* skip "event " prefix */
    while (pos < len && msg[pos] != ' ')
        pos++;
    if (pos >= len)
        return;
    pos++;

    /* read id */
    while (pos < len && msg[pos] >= '0' && msg[pos] <= '9')
        id = id * 10 + (msg[pos++] - '0');
    if (pos >= len) return;
    pos++;

    /* read event type (up to newline or end) */
    while (pos < len && msg[pos] != '\n' && ei < (int)sizeof(event) - 1)
        event[ei++] = msg[pos++];
    event[ei] = 0;

    /* payload after newline (may be NULL) */
    const char *value = NULL;
    if (pos < len && msg[pos] == '\n')
        value = msg + pos + 1;

    for (int i = 0; i < ctx->nhandlers; i++) {
        if (ctx->handlers[i].id == id && ctx->handlers[i].fn) {
            ctx->handlers[i].fn(ctx, id, event, value,
                        ctx->handlers[i].arg);
            return;
        }
    }
}

/****************************************************************
 * Public API
 ****************************************************************/

qu_ctx *
qu_connect(const char *display)
{
    if (!display) display = getenv("QUAOAR_DISPLAY");
    if (!display) display = "/tmp/quaoar-0";

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", display);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return NULL;
    }

    qu_ctx *ctx = calloc(1, sizeof(qu_ctx));
    ctx->fd = fd;
    ctx->next_id = 1;

    return ctx;
}

void
qu_disconnect(qu_ctx *ctx)
{
    if (!ctx)
        return;
    close(ctx->fd);
    free(ctx);
}

int
qu_fd(qu_ctx *ctx)
{
    return ctx->fd;
}

int
qu_process(qu_ctx *ctx)
{
    int space = BUF_SIZE - ctx->rlen;
    if (space <= 0) return -1;

    int r = read(ctx->fd, ctx->rbuf + ctx->rlen, space);
    if (r <= 0) return -1;
    ctx->rlen += r;

    while (ctx->rlen > 0) {
        uint8_t msg[QU_MAX_MSG];
        int consumed;
        int mlen = ns_decode(ctx->rbuf, ctx->rlen,
                     msg, sizeof(msg) - 1, &consumed);
        if (mlen < 0) break;
        if (mlen > 0) {
            msg[mlen] = 0;
            dispatch_event(ctx, (const char *)msg, mlen);
        }
        if (consumed > 0) {
            memmove(ctx->rbuf, ctx->rbuf + consumed,
                ctx->rlen - consumed);
            ctx->rlen -= consumed;
        }
    }

    return 0;
}

void
qu_on_event(qu_ctx *ctx, int id, qu_event_fn fn, void *arg)
{
    /* replace existing handler for this id */
    for (int i = 0; i < ctx->nhandlers; i++) {
        if (ctx->handlers[i].id == id) {
            ctx->handlers[i].fn = fn;
            ctx->handlers[i].arg = arg;
            return;
        }
    }
    if (ctx->nhandlers < MAX_HANDLERS) {
        ctx->handlers[ctx->nhandlers].id = id;
        ctx->handlers[ctx->nhandlers].fn = fn;
        ctx->handlers[ctx->nhandlers].arg = arg;
        ctx->nhandlers++;
    }
}

/****************************************************************
 * Widget creation
 ****************************************************************/

int
qu_window(qu_ctx *ctx, const char *title, int x, int y, int w, int h)
{
    int id = ctx->next_id++;

    qu_send(ctx, "window %d %d %d %d %d\n%s",
        id, x, y, w, h, title);

    return id;
}

int
qu_button(qu_ctx *ctx, int parent, const char *label,
      int x, int y, int w, int h)
{
    int id = ctx->next_id++;
    char xesc[256], svg[2048];

    xml_escape(label, xesc, sizeof(xesc));
    snprintf(svg, sizeof(svg),
        "<g transform=\"translate(%d,%d)\" "
        "style=\"cursor:pointer\">"
        "<rect width=\"%d\" height=\"%d\" rx=\"4\" "
        "fill=\"#5a5a62\" stroke=\"#777\" stroke-width=\"1\">"
        "<set attributeName=\"fill\" to=\"#6a6a72\" "
        "begin=\"mouseover\" end=\"mouseout\"/>"
        "<set attributeName=\"fill\" to=\"#4a4a52\" "
        "begin=\"mousedown\" end=\"mouseup\"/>"
        "</rect>"
        "<text x=\"%d\" y=\"%d\" fill=\"#eee\" font-size=\"12\" "
        "text-anchor=\"middle\" "
        "font-family=\"system-ui, sans-serif\" "
        "pointer-events=\"none\" "
        "data-qu-text=\"1\">%s</text>"
        "</g>",
        x, y, w, h, w / 2, h / 2 + 4, xesc);
    qu_send(ctx, "svg %d %d\n%s", id, parent, svg);
    qu_send(ctx, "listen %d click", id);

    return id;
}

int
qu_label(qu_ctx *ctx, int parent, const char *text, int x, int y)
{
    int id = ctx->next_id++;
    char xesc[1024], svg[2048];

    xml_escape(text, xesc, sizeof(xesc));
    snprintf(svg, sizeof(svg),
        "<text x=\"%d\" y=\"%d\" fill=\"#ddd\" font-size=\"13\" "
        "font-family=\"system-ui, sans-serif\" "
        "data-qu-text=\"1\">%s</text>",
        x, y + 14, xesc);
    qu_send(ctx, "svg %d %d\n%s", id, parent, svg);

    return id;
}

int
qu_textarea(qu_ctx *ctx, int parent, int x, int y, int w, int h)
{
    int id = ctx->next_id++;

    qu_send(ctx, "textarea %d %d %d %d %d %d",
        id, parent, x, y, w, h);

    return id;
}

int
qu_scrollbar(qu_ctx *ctx, int parent, int x, int y, int w, int h,
         char orient)
{
    int id = ctx->next_id++;
    int vert = (orient == 'v');
    int thumb_len = vert ? h / 5 : w / 5;
    int track_len;
    char svg[2048];

    if (thumb_len < 20)
        thumb_len = 20;
    track_len = (vert ? h : w) - thumb_len;

    snprintf(svg, sizeof(svg),
        "<g transform=\"translate(%d,%d)\">"
        "<rect width=\"%d\" height=\"%d\" rx=\"3\" "
        "fill=\"#3a3a3e\" stroke=\"#555\" stroke-width=\"1\"/>"
        "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
        "rx=\"3\" fill=\"#6a6a72\" style=\"cursor:pointer\" "
        "data-qu-drag=\"%c\" data-qu-track=\"%d\">"
        "<set attributeName=\"fill\" to=\"#7a7a82\" "
        "begin=\"mouseover\" end=\"mouseout\"/>"
        "</rect>"
        "</g>",
        x, y, w, h,
        vert ? 1 : 0, vert ? 0 : 1,
        vert ? w - 2 : thumb_len,
        vert ? thumb_len : h - 2,
        orient, track_len);
    qu_send(ctx, "svg %d %d\n%s", id, parent, svg);

    return id;
}

int
qu_svg(qu_ctx *ctx, int parent, const char *markup)
{
    int id = ctx->next_id++;

    qu_send(ctx, "svg %d %d\n%s", id, parent, markup);

    return id;
}

/****************************************************************
 * Widget updates
 ****************************************************************/

void
qu_set_text(qu_ctx *ctx, int id, const char *text)
{
    qu_send(ctx, "update %d text\n%s", id, text);
}

void
qu_set_prop(qu_ctx *ctx, int id, const char *key, const char *value)
{
    qu_send(ctx, "update %d %s\n%s", id, key, value);
}

void
qu_remove(qu_ctx *ctx, int id)
{
    qu_send(ctx, "remove %d", id);
}

void
qu_set_clipboard(qu_ctx *ctx, const char *text)
{
    qu_send(ctx, "clipboard\n%s", text);
}
