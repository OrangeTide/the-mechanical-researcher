/* libquaoar.h : client library for the quaoar display protocol */
/* Copyright (c) 2026 — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

#ifndef LIBQUAOAR_H
#define LIBQUAOAR_H

#define QU_MAX_MSG 65536

typedef struct qu_ctx qu_ctx;

/**
 * Event callback: called when the display sends an event for a widget.
 * event is one of: "click", "text", "scroll", "close", "paste", "xy".
 * value is event-specific (text content, scroll position, etc.) or NULL.
 */
typedef void (*qu_event_fn)(qu_ctx *ctx, int id, const char *event,
                            const char *value, void *arg);

/** Connect to quaoar-server. display may be NULL to use QUAOAR_DISPLAY. */
qu_ctx *qu_connect(const char *display);

/** Disconnect and free resources. */
void qu_disconnect(qu_ctx *ctx);

/** Get the socket fd for use with poll()/select(). */
int qu_fd(qu_ctx *ctx);

/** Process pending events. Returns 0 on success, -1 on disconnect. */
int qu_process(qu_ctx *ctx);

/** Register a callback for events on a widget. */
void qu_on_event(qu_ctx *ctx, int id, qu_event_fn fn, void *arg);

/* widget creation — all return a widget ID */
int qu_window(qu_ctx *ctx, const char *title, int x, int y, int w, int h);
int qu_button(qu_ctx *ctx, int parent, const char *label,
          int x, int y, int w, int h);
int qu_label(qu_ctx *ctx, int parent, const char *text, int x, int y);
int qu_textarea(qu_ctx *ctx, int parent, int x, int y, int w, int h);
int qu_scrollbar(qu_ctx *ctx, int parent, int x, int y, int w, int h,
         char orient);
int qu_svg(qu_ctx *ctx, int parent, const char *markup);

/* widget updates */
void qu_set_text(qu_ctx *ctx, int id, const char *text);
void qu_set_prop(qu_ctx *ctx, int id, const char *key, const char *value);
void qu_remove(qu_ctx *ctx, int id);

/* clipboard */
void qu_set_clipboard(qu_ctx *ctx, const char *text);

#endif /* LIBQUAOAR_H */
