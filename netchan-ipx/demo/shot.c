/* shot.c : render one game frame as ANSI text (host tool, for screenshots) */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

/*
 * Sets up the simulation, runs a few ticks of "chase the nearest creature
 * and fire", then prints a single 40x25 frame with ANSI colors and CP437-ish
 * glyphs. Pipe through tools/term-screenshot to get an SVG for the article.
 */

#include "game.h"
#include <stdio.h>

#define VIEW_H (SCR_H_ROWS - 1)
#define SCR_W_COLS 40
#define SCR_H_ROWS 25

static struct world w;

static int
sgn(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

/* projectile glyph by travel direction, to match the in-game CP437 arrows:
 * Unicode cardinal arrows, slashes for the diagonals. Index with shot dir. */
static const char *const shot_arrow[8] = {
    "\xe2\x86\x91",     /* N  up    U+2191 */
    "/",                /* NE              */
    "\xe2\x86\x92",     /* E  right U+2192 */
    "\\",               /* SE              */
    "\xe2\x86\x93",     /* S  down  U+2193 */
    "/",                /* SW              */
    "\xe2\x86\x90",     /* W  left  U+2190 */
    "\\",               /* NW              */
};

int
main(void)
{
    int t, cx, cy, sx, sy, i;
    struct player *p;

    game_init(&w, 0xBEEF);
    game_add_player(&w);
    p = &w.players[0];

    /* play a few ticks so creatures close in and shots are in flight */
    for (t = 0; t < 12; t++) {
        int bd = 0x7fff, bdx = 0, bdy = 0;
        for (i = 0; i < MAX_CREATURES; i++) {
            struct creature *c = &w.creatures[i];
            int dx, dy, d;
            if (!c->alive)
                continue;
            dx = c->x - p->x;
            dy = c->y - p->y;
            d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (d < bd) { bd = d; bdx = dx; bdy = dy; }
        }
        {
            uint8_t in = IN_NONE;
            int k;
            for (k = 0; k < 8; k++)
                if (game_dx[k] == sgn(bdx) && game_dy[k] == sgn(bdy)) {
                    in = IN_MAKE(k, k);     /* move toward and fire at it */
                    break;
                }
            game_set_input(&w, 0, in);
        }
        game_tick(&w);
    }

    cx = p->x - SCR_W_COLS / 2;
    cy = p->y - VIEW_H / 2;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx > MAP_W - SCR_W_COLS) cx = MAP_W - SCR_W_COLS;
    if (cy > MAP_H - VIEW_H) cy = MAP_H - VIEW_H;

    for (sy = 0; sy < VIEW_H; sy++) {
        for (sx = 0; sx < SCR_W_COLS; sx++) {
            int wx = cx + sx, wy = cy + sy, drawn = 0;
            /* players */
            for (i = 0; i < MAX_PLAYERS; i++)
                if (w.players[i].alive && w.players[i].x == wx &&
                    w.players[i].y == wy) {
                    fputs("\033[1;32m@", stdout);
                    drawn = 1;
                    break;
                }
            if (drawn) continue;
            for (i = 0; i < MAX_SHOTS; i++)
                if (w.shots[i].alive && w.shots[i].x == wx &&
                    w.shots[i].y == wy) {
                    printf("\033[1;33m%s",
                           shot_arrow[w.shots[i].dir & 7]);
                    drawn = 1;
                    break;
                }
            if (drawn) continue;
            for (i = 0; i < MAX_CREATURES; i++)
                if (w.creatures[i].alive && w.creatures[i].x == wx &&
                    w.creatures[i].y == wy) {
                    fputs("\033[1;31mg", stdout);
                    drawn = 1;
                    break;
                }
            if (drawn) continue;
            switch (TILE(&w, wx, wy)) {
            case T_WALL:     fputs("\033[34m\xe2\x96\x92", stdout); break;
            case T_TREASURE: fputs("\033[33m$", stdout); break;
            default:         fputc(' ', stdout); break;
            }
        }
        fputs("\033[0m\n", stdout);
    }

    {
        int live = 0;
        for (i = 0; i < MAX_CREATURES; i++)
            live += w.creatures[i].alive;
        printf("\033[36mP1  HP:%d  SCORE:%u  creatures:%d\033[0m\n",
               p->hp, p->score, live);
    }
    return 0;
}
