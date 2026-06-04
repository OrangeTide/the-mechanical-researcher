/* thor.c : Caves-of-Thor-style networked game over netchan */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

/*
 * One instance hosts (and plays); the others join. Run:
 *   thor s          host a game
 *   thor            join a game (IPX: broadcast discovery)
 * On the host build, an optional server host/IP argument selects the server
 * for a client; it defaults to localhost.
 */

#include "game.h"
#include "game_net.h"
#include "render.h"
#include "plat.h"
#include <string.h>

/****************************************************************
 * Transport: IPX on DOS, UDP on the host
 ****************************************************************/

#if defined(__WATCOMC__)
#include "nc_ipx.h"
#include <i86.h>
#define GAME_SOCKET 0x6000
static struct nc_ipx tp;
static int  tp_open(int server)         { (void)server;
                                          return nc_ipx_open(&tp, GAME_SOCKET); }
static int  tp_recv(void *b, struct nc_addr *a)
                                        { return nc_ipx_recv(&tp, b, NC_MTU, a); }
static int  tp_send(const void *b, int n, const struct nc_addr *a)
                                        { return nc_ipx_send(&tp, b, (size_t)n, a); }
static void tp_close(void)              { nc_ipx_close(&tp); }
static void tp_server_addr(struct nc_addr *a, const char *host)
                                        { (void)host; nc_ipx_broadcast(&tp, a); }
static void frame_pace(void)
{
    volatile uint32_t __far *t = (volatile uint32_t __far *)
        MK_FP(0x0040, 0x006C);
    uint32_t s = *t;
    long guard = 0;
    /* spin until the BIOS tick advances (~55 ms); the guard keeps a frozen
     * tick from hanging the game outright */
    while (*t == s && ++guard < 20000L)
        ;
}
#else
#include "nc_udp.h"
#include <time.h>
#define GAME_PORT 18900
static struct nc_udp tp;
static int  tp_open(int server)         { return nc_udp_open(&tp, NULL,
                                              server ? GAME_PORT : 0); }
static int  tp_recv(void *b, struct nc_addr *a)
                                        { return nc_udp_recv(&tp, b, NC_MTU, a); }
static int  tp_send(const void *b, int n, const struct nc_addr *a)
                                        { return nc_udp_send(&tp, b, (size_t)n, a); }
static void tp_close(void)              { nc_udp_close(&tp); }
static void tp_server_addr(struct nc_addr *a, const char *host)
                                        { nc_udp_addr(host ? host : "127.0.0.1",
                                                      GAME_PORT, a); }
static void frame_pace(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 30L * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

/****************************************************************
 * Helpers
 ****************************************************************/

#define MAX_DRAIN 32            /* datagrams handled per frame, each way */

static int
sgn(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

static uint8_t
read_input(void)
{
    int dx = plat_key(K_RIGHT) - plat_key(K_LEFT);
    int dy = plat_key(K_DOWN) - plat_key(K_UP);
    uint8_t in = IN_DIR_NONE;
    if (dx || dy) {
        int k;
        for (k = 0; k < 8; k++)
            if (game_dx[k] == sgn(dx) && game_dy[k] == sgn(dy)) {
                in = (uint8_t)k;
                break;
            }
    }
    if (plat_key(K_FIRE))
        in |= IN_FIRE;
    return in;
}

static int do_dump;

static void
maybe_dump(int server, uint32_t *last, uint32_t now)
{
    (void)now;
    if (!do_dump)
        return;
    if (++(*last) < 32)         /* roughly a couple times a second */
        return;
    *last = 0;
    plat_dump(server ? "SVIEW.TXT" : "CVIEW.TXT");
}

/****************************************************************
 * Chat: a one-line entry box and a transient incoming line
 ****************************************************************/

#define CHAT_IN_MAX 30

static int chat_mode;
static char chat_in[CHAT_IN_MAX + 1];
static int chat_in_len;
static char chat_last[SCR_W];
static uint32_t chat_last_ms;

/* on_chat callback (who is a 0-based player index) */
static void
chat_received(int who, const char *text)
{
    int n = 0;
    chat_last[n++] = 'P';
    chat_last[n++] = (char)('1' + who);
    chat_last[n++] = ':';
    chat_last[n++] = ' ';
    while (*text && n < SCR_W - 1)
        chat_last[n++] = *text++;
    chat_last[n] = 0;
    chat_last_ms = plat_now_ms();
}

/* Drain typed characters. Returns 1 when a finished line is in chat_in. */
static int
chat_input(void)
{
    int c;
    while ((c = plat_getch()) != 0) {
        if (!chat_mode) {
            if (c == '\r') {            /* Enter opens the chat box */
                chat_mode = 1;
                chat_in_len = 0;
                chat_in[0] = 0;
            }
            continue;                   /* otherwise ignore typing in play */
        }
        if (c == '\r') {
            chat_mode = 0;
            return chat_in_len > 0;
        }
        if (c == 27) {                  /* Esc cancels */
            chat_mode = 0;
            chat_in_len = 0;
            chat_in[0] = 0;
            return 0;
        }
        if (c == 8) {                   /* backspace */
            if (chat_in_len > 0)
                chat_in[--chat_in_len] = 0;
        } else if (c >= 32 && c < 127 && chat_in_len < CHAT_IN_MAX) {
            chat_in[chat_in_len++] = (char)c;
            chat_in[chat_in_len] = 0;
        }
    }
    return 0;
}

/* The bottom-row status: the chat entry box, a recent incoming line, or
 * NULL to fall back to the default HUD. */
static const char *
chat_status(uint32_t now)
{
    static char s[SCR_W + 8];
    int n = 0, i;
    if (chat_mode) {
        const char *p = "Say: ";
        while (*p)
            s[n++] = *p++;
        for (i = 0; i < chat_in_len && n < SCR_W - 1; i++)
            s[n++] = chat_in[i];
        if (n < SCR_W - 1)
            s[n++] = '_';
        s[n] = 0;
        return s;
    }
    if (chat_last[0] && now - chat_last_ms < 5000UL)
        return chat_last;
    return 0;
}

static void
message(const char *s)
{
    int i;
    for (i = 0; i < SCR_W * SCR_H; i++)
        plat_put(i % SCR_W, i / SCR_W, ' ', ATTR(C_GREY, C_BLACK));
    for (i = 0; s[i] && i < SCR_W; i++)
        plat_put(i, SCR_H / 2, (unsigned char)s[i], ATTR(C_WHITE, C_BLACK));
    plat_present();
}

/****************************************************************
 * Server and client loops
 ****************************************************************/

static void
run_server(void)
{
    static struct gserver srv;
    uint8_t buf[NC_MTU];
    struct nc_addr from, to;
    int host_pi, n, d;

    uint32_t dump_t = 0;
    gserver_init(&srv, (uint16_t)plat_now_ms());
    srv.on_chat = chat_received;
    host_pi = game_add_player(&srv.world);

    for (;;) {
        uint32_t now = plat_now_ms();
        plat_poll();
        if (plat_key(K_QUIT))
            break;
        if (chat_input())
            gserver_say(&srv, host_pi, chat_in);
        game_set_input(&srv.world, host_pi,
                       chat_mode ? IN_DIR_NONE : read_input());

        for (d = 0; d < MAX_DRAIN && (n = tp_recv(buf, &from)) > 0; d++)
            gserver_feed(&srv, buf, (size_t)n, &from);
        gserver_service(&srv, now);
        for (d = 0; d < MAX_DRAIN &&
             (n = (int)gserver_pull(&srv, buf, sizeof(buf), &to)) > 0; d++)
            tp_send(buf, n, &to);

        render_world(&srv.world, host_pi, chat_status(now));
        maybe_dump(1, &dump_t, now);
        frame_pace();
    }
}

static void
run_client(const char *host)
{
    static struct gclient cli;
    uint8_t buf[NC_MTU];
    struct nc_addr from, to, server;
    int n;

    uint32_t dump_t = 0;
    int d;
    gclient_init(&cli);
    cli.on_chat = chat_received;
    tp_server_addr(&server, host);
    gclient_connect(&cli, &server);

    for (;;) {
        uint32_t now = plat_now_ms();
        plat_poll();
        if (plat_key(K_QUIT))
            break;

        for (d = 0; d < MAX_DRAIN && (n = tp_recv(buf, &from)) > 0; d++)
            gclient_feed(&cli, buf, (size_t)n, &from);
        gclient_service(&cli, now);
        if (chat_input())
            gclient_chat(&cli, chat_in);
        if (cli.player >= 0)
            gclient_input(&cli, chat_mode ? IN_DIR_NONE : read_input());
        for (d = 0; d < MAX_DRAIN &&
             (n = (int)gclient_pull(&cli, buf, sizeof(buf), &to)) > 0; d++)
            tp_send(buf, n, &to);

        if (cli.have_map && cli.player >= 0)
            render_world(&cli.world, cli.player, chat_status(now));
        else if (cli.player >= 0)
            message("loading map...");
        else
            message("connecting to host...");
        maybe_dump(0, &dump_t, now);
        frame_pace();
    }
}

int
main(int argc, char **argv)
{
    int server = 0, i;
    const char *host = 0;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == 's' || argv[i][0] == 'S')
            server = 1;
        else if (argv[i][0] == 'd')
            do_dump = 1;
        else
            host = argv[i];
    }

    if (tp_open(server) != 0) {
        /* plat not yet up; print plainly */
        return 1;
    }
    plat_init();
    if (server)
        run_server();
    else
        run_client(host);
    plat_shutdown();
    tp_close();
    return 0;
}
