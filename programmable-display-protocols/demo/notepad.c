/* notepad.c : minimal text editor using the quaoar display protocol */
/* Copyright (c) 2026 — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

#include "libquaoar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#define SAVE_PATH "/tmp/quaoar-notepad.txt"

static int textarea_id;
static char text_buf[QU_MAX_MSG] = "";

static volatile int quit = 0;

static void
on_close(qu_ctx *ctx, int id, const char *event, const char *value,
     void *arg)
{
    (void)ctx; (void)id; (void)event; (void)value; (void)arg;
    quit = 1;
}

static void
on_save(qu_ctx *ctx, int id, const char *event, const char *value,
    void *arg)
{
    (void)ctx; (void)id; (void)event; (void)value; (void)arg;

    FILE *f = fopen(SAVE_PATH, "w");
    if (f) {
        fputs(text_buf, f);
        fclose(f);
        fprintf(stderr, "saved to %s\n", SAVE_PATH);
    }
}

static void
on_load(qu_ctx *ctx, int id, const char *event, const char *value,
    void *arg)
{
    (void)id; (void)event; (void)value; (void)arg;

    FILE *f = fopen(SAVE_PATH, "r");
    if (f) {
        size_t n = fread(text_buf, 1, sizeof(text_buf) - 1, f);
        text_buf[n] = 0;
        fclose(f);
        qu_set_text(ctx, textarea_id, text_buf);
        fprintf(stderr, "loaded from %s\n", SAVE_PATH);
    }
}

static void
on_text(qu_ctx *ctx, int id, const char *event, const char *value,
    void *arg)
{
    (void)ctx; (void)id; (void)event; (void)arg;

    if (value)
        snprintf(text_buf, sizeof(text_buf), "%s", value);
}

int
main(void)
{
    qu_ctx *ctx = qu_connect(NULL);
    if (!ctx) {
        fprintf(stderr, "cannot connect to quaoar display\n");

        return 1;
    }

    int win = qu_window(ctx, "Notepad", 80, 60, 480, 340);
    int save_btn = qu_button(ctx, win, "Save", 10, 10, 70, 28);
    int load_btn = qu_button(ctx, win, "Load", 90, 10, 70, 28);
    textarea_id  = qu_textarea(ctx, win, 10, 48, 460, 272);

        /* load the text file on start */
        on_load(ctx, 0 /* unused */, NULL /* unused */, NULL /* unused */, NULL /* unused */);

        /* set the event handlers */
    qu_on_event(ctx, win, on_close, NULL);
    qu_on_event(ctx, save_btn, on_save, NULL);
    qu_on_event(ctx, load_btn, on_load, NULL);
    qu_on_event(ctx, textarea_id, on_text, NULL);

    struct pollfd pfd = { .fd = qu_fd(ctx), .events = POLLIN };
    while (!quit && poll(&pfd, 1, -1) >= 0) {
        if (pfd.revents & (POLLIN | POLLHUP))
            if (qu_process(ctx) < 0)
                break;
    }

    qu_disconnect(ctx);

    return 0;
}
