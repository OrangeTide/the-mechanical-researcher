/* test_netchan.c : host loopback tests for the netchan core (over UDP) */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#include "netchan.h"
#include "nc_udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond, msg) do {                       \
        if (cond) {                                 \
            printf("  ok   %s\n", msg);             \
        } else {                                    \
            printf("  FAIL %s\n", msg);             \
            g_fail++;                               \
        }                                           \
    } while (0)

/* Deterministic drop: drop every g_drop'th datagram (0 = no loss). */
static unsigned g_seq;
static int g_drop;

static void
recv_pump(struct nc_udp *u, struct netchan *nc)
{
    uint8_t buf[NC_MTU];
    struct nc_addr from;
    int n;
    while ((n = nc_udp_recv(u, buf, sizeof(buf), &from)) > 0)
        nc_feed(nc, buf, (size_t)n, &from);
}

static void
send_pump(struct nc_udp *u, struct netchan *nc)
{
    uint8_t buf[NC_MTU];
    struct nc_addr to;
    int n;
    while ((n = (int)nc_send_next(nc, buf, sizeof(buf), &to)) > 0) {
        g_seq++;
        if (g_drop && (g_seq % (unsigned)g_drop) == 0)
            continue;           /* simulate packet loss */
        nc_udp_send(u, buf, (size_t)n, &to);
    }
}

int
main(void)
{
    struct nc_udp su, cu;
    struct nc_addr saddr;
    struct netchan *s, *cl;
    struct nc_chan *s_rel, *s_unr, *c_rel, *c_unr;
    uint32_t now = 0;
    int i;

    srand(20260601u);

    if (nc_udp_open(&su, "127.0.0.1", 0) != 0 ||
        nc_udp_open(&cu, "127.0.0.1", 0) != 0) {
        printf("socket setup failed\n");
        return 2;
    }
    nc_udp_local(&su, &saddr);

    s = nc_open(1);
    cl = nc_open(0);
    s_rel = nc_chan_open(s, NC_RELIABLE);
    s_unr = nc_chan_open(s, NC_UNRELIABLE);
    c_rel = nc_chan_open(cl, NC_RELIABLE);
    c_unr = nc_chan_open(cl, NC_UNRELIABLE);

    printf("netchan core tests (UDP loopback)\n");
    printf("MTU=%d WINDOW=%d MAXMSG=%d\n", NC_MTU, NC_WINDOW, NC_MAXMSG);

    /* --- handshake --- */
    nc_connect(cl, &saddr);
    for (i = 0; i < 100; i++) {
        if (nc_state(s) == NC_STATE_CONNECTED &&
            nc_state(cl) == NC_STATE_CONNECTED)
            break;
        nc_service(s, now);
        nc_service(cl, now);
        recv_pump(&cu, cl);
        recv_pump(&su, s);
        send_pump(&cu, cl);
        send_pump(&su, s);
        now += 20;
    }
    CHECK(nc_state(cl) == NC_STATE_CONNECTED, "client connected");
    CHECK(nc_state(s) == NC_STATE_CONNECTED, "server connected");

    /* --- reliable in-order delivery under 25% packet loss --- */
    g_drop = 4;
    {
        const int N = 40;
        int sent = 0, recvd = 0, order_ok = 1;
        char rb[64];
        for (i = 0; i < 4000 && recvd < N; i++) {
            while (sent < N) {
                char m[16];
                int ml = sprintf(m, "R%03d", sent);
                if (nc_write(c_rel, m, (size_t)ml) == NC_ERR_AGAIN)
                    break;
                sent++;
            }
            nc_service(s, now);
            nc_service(cl, now);
            recv_pump(&cu, cl);
            recv_pump(&su, s);
            send_pump(&cu, cl);
            send_pump(&su, s);
            {
                int n;
                while ((n = nc_read(s_rel, rb, sizeof(rb))) > 0) {
                    char want[16];
                    int wl = sprintf(want, "R%03d", recvd);
                    if (n != wl || memcmp(rb, want, (size_t)n) != 0)
                        order_ok = 0;
                    recvd++;
                }
            }
            now += 20;
        }
        CHECK(recvd == N, "reliable: all 40 messages delivered under loss");
        CHECK(order_ok, "reliable: delivered strictly in order");
    }

    /* --- unreliable delivery (no induced loss) ---
     * One datagram per tick, drained each tick, the way a game loop uses
     * an unreliable channel. The TX ring is deliberately shallow. */
    g_drop = 0;
    {
        const int N = 10;
        int got = 0, j;
        char rb[64];
        for (j = 0; j < N; j++) {
            char m[16];
            int ml = sprintf(m, "U%03d", j), n;
            nc_write(c_unr, m, (size_t)ml);
            nc_service(s, now);
            nc_service(cl, now);
            recv_pump(&cu, cl);
            recv_pump(&su, s);
            send_pump(&cu, cl);
            send_pump(&su, s);
            while ((n = nc_read(s_unr, rb, sizeof(rb))) > 0)
                got++;
            now += 20;
        }
        /* drain any in flight */
        for (i = 0; i < 5 && got < N; i++) {
            int n;
            nc_service(s, now);
            recv_pump(&su, s);
            while ((n = nc_read(s_unr, rb, sizeof(rb))) > 0)
                got++;
            now += 20;
        }
        CHECK(got == N, "unreliable: 10 datagrams delivered, 1/tick lossless");
    }

    /* --- bidirectional: server -> client reliable --- */
    g_drop = 0;
    {
        const int N = 5;
        int got = 0, j;
        char rb[64];
        for (j = 0; j < N; j++) {
            char m[16];
            int ml = sprintf(m, "S%03d", j);
            nc_write(s_rel, m, (size_t)ml);
        }
        for (i = 0; i < 50 && got < N; i++) {
            int n;
            nc_service(s, now);
            nc_service(cl, now);
            recv_pump(&cu, cl);
            recv_pump(&su, s);
            send_pump(&cu, cl);
            send_pump(&su, s);
            while ((n = nc_read(c_rel, rb, sizeof(rb))) > 0)
                got++;
            now += 20;
        }
        CHECK(got == N, "reliable: server-to-client delivery works");
    }

    nc_close(s);
    nc_close(cl);
    nc_udp_close(&su);
    nc_udp_close(&cu);

    printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
    return g_fail ? 1 : 0;
}
