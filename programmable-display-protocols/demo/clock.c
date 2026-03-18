/* clock.c : analog clock widget using the quaoar display protocol */
/* Copyright (c) 2026 — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */
/* Clock design based on clock16() from OrangeTide/fbgfx */

#include "libquaoar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <poll.h>

static int quit;
static int clock_id;

static void
on_close(qu_ctx *ctx, int id, const char *event,
         const char *value, void *arg)
{
    (void)ctx; (void)id; (void)event; (void)value; (void)arg;
    quit = 1;
}

/** Emit a clock hand with SMIL rotation.
 *
 * The hand is drawn pointing straight up (12 o'clock). SMIL
 * rotates it into position and sweeps it continuously. C
 * provides the starting angle as h*30+m*0.5 (hour), m*6+s*0.1
 * (minute), or s*6 (second) — no trigonometry needed.
 */
static int
hand(char *buf, int max, int ox, int oy, int length,
     double from, double period, int width, const char *color)
{
    return snprintf(buf, max,
        "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
        "stroke=\"%s\" stroke-width=\"%d\" "
        "stroke-linecap=\"round\">"
        "<animateTransform attributeName=\"transform\" "
        "type=\"rotate\" "
        "from=\"%.1f %d %d\" to=\"%.1f %d %d\" "
        "dur=\"%.1fs\" repeatCount=\"indefinite\"/>"
        "</line>",
        ox, oy - length, ox, oy,
        color, width,
        from, ox, oy, from + 360.0, ox, oy,
        period);
}

/** Build and send the clock SVG. All positioning is done by
 * SVG rotate transforms — C only provides time values, SMIL
 * converts to visual positions.
 */
static void
send_clock(qu_ctx *ctx, int win)
{
    char svg[8192];
    int p = 0;
    int r = 80;
    int ox = 100, oy = 105;
    time_t now;
    struct tm tm;
    char date[40];

    if (clock_id) {
        qu_remove(ctx, clock_id);
        clock_id = 0;
    }

    time(&now);
    tm = *localtime(&now);
    strftime(date, sizeof(date), "%Y.%m.%d", &tm);

    /* starting angles — pure arithmetic, no trig */
    double sec_angle = tm.tm_sec * 6.0;
    double min_angle = tm.tm_min * 6.0 + tm.tm_sec * 0.1;
    double hr_angle = (tm.tm_hour % 12) * 30.0 + tm.tm_min * 0.5;

    /* clock face */
    p += snprintf(svg + p, sizeof(svg) - p,
        "<g>"
        "<circle cx=\"%d\" cy=\"%d\" r=\"%d\" "
        "fill=\"#1a1a1e\" stroke=\"#555\" stroke-width=\"1\"/>",
        ox, oy, r + 10);

    /* jewel markers — one circle at 12 o'clock, rotated into
     * position by transform. no cos/sin needed. */
    for (int i = 0; i < 12; i++) {
        p += snprintf(svg + p, sizeof(svg) - p,
            "<circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"%s\" "
            "transform=\"rotate(%d %d %d)\"/>",
            ox, oy - r,
            (i % 3 == 0) ? 4 : 2,
            (i == 0) ? "#ee0" : "#558",
            i * 30, ox, oy);
    }

    /* hands — drawn pointing up, SMIL rotates them */
    p += hand(svg + p, sizeof(svg) - p,
        ox, oy, r / 2,
        hr_angle, 43200.0, 3, "#d6d");

    p += hand(svg + p, sizeof(svg) - p,
        ox, oy, r * 3 / 4,
        min_angle, 3600.0, 2, "#5e5");

    p += hand(svg + p, sizeof(svg) - p,
        ox, oy, r * 17 / 20,
        sec_angle, 60.0, 1, "#eee");

    /* center dot */
    p += snprintf(svg + p, sizeof(svg) - p,
        "<circle cx=\"%d\" cy=\"%d\" r=\"3\" fill=\"#eee\"/>",
        ox, oy);

    /* date below center */
    p += snprintf(svg + p, sizeof(svg) - p,
        "<text x=\"%d\" y=\"%d\" fill=\"#999\" font-size=\"10\" "
        "text-anchor=\"middle\" "
        "font-family=\"'IBM Plex Mono', monospace\">%s</text>",
        ox, oy + r / 3, date);

    snprintf(svg + p, sizeof(svg) - p, "</g>");

    clock_id = qu_svg(ctx, win, svg);
}

/** Milliseconds until the next minute boundary. */
static int
ms_until_next_minute(void)
{
    time_t now;
    struct tm tm;

    time(&now);
    tm = *localtime(&now);
    return (60 - tm.tm_sec) * 1000;
}

int
main(void)
{
    qu_ctx *ctx = qu_connect(NULL);

    if (!ctx) {
        fprintf(stderr, "clock: cannot connect to display\n");
        return 1;
    }

    int win = qu_window(ctx, "Clock", 60, 60, 210, 240);

    qu_on_event(ctx, win, on_close, NULL);
    send_clock(ctx, win);

    struct pollfd pfd = { .fd = qu_fd(ctx), .events = POLLIN };

    while (!quit) {
        int timeout = ms_until_next_minute();
        int ret = poll(&pfd, 1, timeout);

        if (ret < 0)
            break;
        if (ret == 0) {
            send_clock(ctx, win);
            continue;
        }
        if (pfd.revents & (POLLIN | POLLHUP))
            if (qu_process(ctx) < 0)
                break;
    }

    qu_disconnect(ctx);
    return 0;
}
