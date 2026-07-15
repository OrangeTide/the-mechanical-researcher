/* netchan_test.c : loopback tests for netchan protocol */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#include "netchan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int tests_run;
static int tests_passed;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %-50s ", #name); \
        fflush(stdout); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("OK\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
        return; \
    } while (0)

#define CHECK(cond, msg) \
    do { if (!(cond)) FAIL(msg); } while (0)

/* Build a UDP-packed nc_addr (see nc_udp.h) from a host-order IPv4 and port.
 * In these loopback tests the address is only an opaque routing token; no
 * real socket is involved. */
static struct nc_addr
make_addr(uint32_t ip, uint16_t port)
{
    struct nc_addr a;
    memset(&a, 0, sizeof(a));
    a.len = 7;
    a.a[0] = 4;
    a.a[1] = (ip >> 24) & 0xff;
    a.a[2] = (ip >> 16) & 0xff;
    a.a[3] = (ip >> 8) & 0xff;
    a.a[4] = ip & 0xff;
    a.a[5] = (port >> 8) & 0xff;
    a.a[6] = port & 0xff;
    return a;
}

/****************************************************************
 * Loopback -- pump packets between client and server
 ****************************************************************/

static int
pump(struct netchan_conn *from, struct netchan_conn *to,
     const struct nc_addr *from_addr)
{
    uint8_t buf[2048];
    struct nc_addr dst;
    int count = 0;

    for (;;) {
        size_t n = netchan_send_next(from, buf, sizeof(buf), &dst);
        if (n == 0) break;
        netchan_feed(to, buf, n, from_addr);
        count++;
    }
    return count;
}

static void
pump_both(struct netchan_conn *client, struct netchan_conn *server,
          const struct nc_addr *client_addr, const struct nc_addr *server_addr)
{
    for (int i = 0; i < 10; i++) {
        int c2s = pump(client, server, client_addr);
        int s2c = pump(server, client, server_addr);
        if (c2s == 0 && s2c == 0) break;
    }
}

/****************************************************************
 * Tests
 ****************************************************************/

static void
test_handshake(void)
{
    TEST(handshake);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);
    CHECK(client && server, "alloc failed");

    struct nc_addr caddr = make_addr(0x7f000001, 10000);
    struct nc_addr saddr = make_addr(0x7f000001, 20000);

    netchan_connect(client, &saddr);
    CHECK(netchan_state(client) == NETCHAN_STATE_CONNECTING, "client not connecting");

    /* client -> server: CONNECT_INIT */
    pump(client, server, &caddr);
    CHECK(netchan_state(server) == NETCHAN_STATE_CONNECTING, "server not connecting");

    /* server accepts */
    struct netchan_event ev;
    netchan_accept(server);
    CHECK(netchan_state(server) == NETCHAN_STATE_CONNECTED, "server not connected");

    /* drain server's CONNECTED event */
    CHECK(netchan_poll(server, &ev) && ev.type == NETCHAN_EV_CONNECTED,
          "no server connected event");

    /* server -> client: CONNECT_ACCEPT */
    pump(server, client, &saddr);
    CHECK(netchan_state(client) == NETCHAN_STATE_CONNECTED, "client not connected");

    /* drain client's CONNECTED event */
    CHECK(netchan_poll(client, &ev) && ev.type == NETCHAN_EV_CONNECTED,
          "no client connected event");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

static void
test_unreliable_datagram(void)
{
    TEST(unreliable_datagram);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10001);
    struct nc_addr saddr = make_addr(0x7f000001, 20001);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump(server, client, &saddr);

    CHECK(netchan_state(client) == NETCHAN_STATE_CONNECTED, "not connected");

    /* drain handshake events */
    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* client opens unreliable send channel */
    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_UNRELIABLE,
                                               NETCHAN_DIR_SEND, "test");
    CHECK(ch != NULL, "chan_open failed");

    /* send channel_open to server */
    pump(client, server, &caddr);

    /* server should have CHAN_OPEN event */
    CHECK(netchan_poll(server, &ev) && ev.type == NETCHAN_EV_CHAN_OPEN,
          "no chan_open event");
    struct netchan_chan *sch = ev.ch;
    CHECK(sch != NULL, "server channel is null");

    /* server sends window_update back (from chan_open processing) */
    pump(server, client, &saddr);

    /* channel should now be open (got window update) */
    CHECK(netchan_chan_state(ch) == 1, "client channel not open");

    /* write a message */
    const char *msg = "hello unreliable";
    int wr = netchan_chan_write(ch, msg, strlen(msg));
    CHECK(wr == (int)strlen(msg), "write failed");

    /* pump data to server */
    pump(client, server, &caddr);

    /* server should have DATA event */
    CHECK(netchan_poll(server, &ev) && ev.type == NETCHAN_EV_DATA,
          "no data event");

    /* read the data */
    char buf[256];
    int rd = netchan_chan_read(sch, buf, sizeof(buf));
    CHECK(rd == (int)strlen(msg), "read wrong size");
    CHECK(memcmp(buf, msg, rd) == 0, "data mismatch");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

static void
test_reliable_datagram(void)
{
    TEST(reliable_datagram);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10002);
    struct nc_addr saddr = make_addr(0x7f000001, 20002);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server,
              &caddr,
              &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* client opens reliable send channel */
    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_RELIABLE,
                                               NETCHAN_DIR_SEND, "gamestate");
    CHECK(ch != NULL, "chan_open failed");

    /* exchange channel_open + window_update */
    pump_both(client, server,
              &caddr,
              &saddr);

    /* drain events */
    while (netchan_poll(server, &ev)) {}
    while (netchan_poll(client, &ev)) {}

    CHECK(netchan_chan_state(ch) == 1, "client channel not open");

    /* send a message */
    const char *msg = "reliable payload here";
    int wr = netchan_chan_write(ch, msg, strlen(msg));
    CHECK(wr == (int)strlen(msg), "write failed");

    /* pump data and ACK */
    pump_both(client, server,
              &caddr,
              &saddr);

    /* find the server's channel */
    struct netchan_chan *sch = NULL;
    while (netchan_poll(server, &ev)) {
        if (ev.type == NETCHAN_EV_DATA && ev.ch)
            sch = ev.ch;
    }
    CHECK(sch != NULL, "no data event on server");

    char buf[256];
    int rd = netchan_chan_read(sch, buf, sizeof(buf));
    CHECK(rd == (int)strlen(msg), "read wrong size");
    CHECK(memcmp(buf, msg, rd) == 0, "data mismatch");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

/* Virtual clock seeded from the same source netchan records timestamps with
 * (CLOCK_MONOTONIC via nc_now_ms), so a now_ms we feed to netchan_service
 * lines up with the sent_ms it stamped on outgoing messages. */
static uint32_t
vclock_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Regression test for the retransmit bug: a lost reliable message must be
 * resent.  The whole first transmission (a multi-fragment message spanning
 * several packets) is dropped, then the retransmit timer is fired and a
 * normal pump must deliver every byte in order.  Against the buggy code the
 * send cursor advanced past the message on transmit, so it fell out of the
 * in-flight window and could never be resent -- this test hangs undelivered. */
static void
test_reliable_retransmit(void)
{
    TEST(reliable_retransmit);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    /* Neutralise idle timeout so the virtual-clock jump can't disconnect us;
     * keep a short, known retransmit interval. */
    struct netchan_cfg cfg = {
        .mtu = 1200,
        .chan_window = 65536,
        .idle_timeout_ms = 3600000,
        .retransmit_ms = 100,
    };
    netchan_config(client, &cfg);
    netchan_config(server, &cfg);

    struct nc_addr caddr = make_addr(0x7f000001, 10010);
    struct nc_addr saddr = make_addr(0x7f000001, 20010);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server, &caddr, &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_RELIABLE,
                                               NETCHAN_DIR_SEND, "gamestate");
    CHECK(ch != NULL, "chan_open failed");
    pump_both(client, server, &caddr, &saddr);
    while (netchan_poll(server, &ev)) {}
    while (netchan_poll(client, &ev)) {}
    CHECK(netchan_chan_state(ch) == 1, "client channel not open");

    /* A message spanning several fragments (larger than the 1200-byte MTU). */
    unsigned char msg[2000];
    for (size_t i = 0; i < sizeof(msg); i++)
        msg[i] = (unsigned char)(i * 31 + 7);
    int wr = netchan_chan_write(ch, msg, sizeof(msg));
    CHECK(wr == (int)sizeof(msg), "write failed");

    /* Drop the entire first transmission: pull every packet the client wants
     * to send and throw it away. */
    uint8_t junk[2048];
    struct nc_addr dst;
    int dropped = 0;
    for (;;) {
        size_t n = netchan_send_next(client, junk, sizeof(junk), &dst);
        if (n == 0) break;
        dropped++;
    }
    CHECK(dropped > 0, "client sent nothing to drop");

    /* Nothing sendable remains until the retransmit timer fires (proves the
     * header-only spin bug is also fixed: send_next returns 0 here). */
    CHECK(netchan_send_next(client, junk, sizeof(junk), &dst) == 0,
          "client spins with nothing to send");

    /* Fire the retransmit timer. */
    uint32_t now = vclock_now() + cfg.retransmit_ms + 50;
    netchan_service(client, now);

    /* Normal delivery + ACK must now succeed. */
    pump_both(client, server, &caddr, &saddr);

    struct netchan_chan *sch = NULL;
    while (netchan_poll(server, &ev)) {
        if (ev.type == NETCHAN_EV_DATA && ev.ch)
            sch = ev.ch;
    }
    CHECK(sch != NULL, "no data event on server after retransmit");

    unsigned char buf[2000];
    int rd = netchan_chan_read(sch, buf, sizeof(buf));
    CHECK(rd == (int)sizeof(msg), "read wrong size after retransmit");
    CHECK(memcmp(buf, msg, sizeof(msg)) == 0, "data mismatch after retransmit");

    struct netchan_chan_stats st;
    netchan_chan_stats(ch, &st);
    CHECK(st.retransmissions > 0, "no retransmission recorded");
    CHECK(st.msgs_acked == 1, "message not acked after retransmit");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

/* Regression test for the attempts counter: when the peer is gone, a reliable
 * message must stop retransmitting after NC_MAX_RT_ATTEMPTS and the channel
 * must be declared dead.  If send_next reset attempts on every resend the
 * give-up threshold would never be reached and this loop would never finish. */
static void
test_reliable_giveup(void)
{
    TEST(reliable_giveup);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct netchan_cfg cfg = {
        .mtu = 1200,
        .chan_window = 65536,
        .idle_timeout_ms = 3600000,
        .retransmit_ms = 100,
    };
    netchan_config(client, &cfg);
    netchan_config(server, &cfg);

    struct nc_addr caddr = make_addr(0x7f000001, 10011);
    struct nc_addr saddr = make_addr(0x7f000001, 20011);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server, &caddr, &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_RELIABLE,
                                               NETCHAN_DIR_SEND, "gamestate");
    CHECK(ch != NULL, "chan_open failed");
    pump_both(client, server, &caddr, &saddr);
    while (netchan_poll(server, &ev)) {}
    while (netchan_poll(client, &ev)) {}
    CHECK(netchan_chan_state(ch) == 1, "client channel not open");

    const char *msg = "into the void";
    CHECK(netchan_chan_write(ch, msg, strlen(msg)) == (int)strlen(msg),
          "write failed");

    /* Drop every transmission and keep firing the retransmit timer.  The
     * channel must give up within a bounded number of rounds. */
    uint8_t junk[2048];
    struct nc_addr dst;
    uint32_t vnow = vclock_now();
    int dead = 0;
    for (int round = 0; round < 32 && !dead; round++) {
        while (netchan_send_next(client, junk, sizeof(junk), &dst) > 0)
            ;
        vnow += cfg.retransmit_ms * 64; /* past any backed-off timeout */
        netchan_service(client, vnow);
        while (netchan_poll(client, &ev))
            if (ev.type == NETCHAN_EV_CHAN_CLOSE)
                dead = 1;
    }
    CHECK(dead, "channel never gave up on a dead peer");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

static void
test_bidirectional_channels(void)
{
    TEST(bidirectional_channels);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10003);
    struct nc_addr saddr = make_addr(0x7f000001, 20003);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server,
              &caddr,
              &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* client opens a SEND channel (client sends, server receives) */
    struct netchan_chan *c_send = netchan_chan_open(client, NETCHAN_RELIABLE,
                                                    NETCHAN_DIR_SEND, "input");
    /* client opens a RECV channel (server sends, client receives) */
    struct netchan_chan *c_recv = netchan_chan_open(client, NETCHAN_RELIABLE,
                                                    NETCHAN_DIR_RECV, "state");
    CHECK(c_send && c_recv, "chan_open failed");

    /* exchange setup */
    pump_both(client, server,
              &caddr,
              &saddr);

    /* find server's channels */
    struct netchan_chan *s_recv_input = NULL;
    struct netchan_chan *s_send_state = NULL;
    while (netchan_poll(server, &ev)) {
        if (ev.type == NETCHAN_EV_CHAN_OPEN && ev.ch) {
            if (netchan_chan_id(ev.ch) == netchan_chan_id(c_send))
                s_recv_input = ev.ch;
            else if (netchan_chan_id(ev.ch) == netchan_chan_id(c_recv))
                s_send_state = ev.ch;
        }
    }
    while (netchan_poll(client, &ev)) {}

    CHECK(s_recv_input != NULL, "server didn't get input channel");
    CHECK(s_send_state != NULL, "server didn't get state channel");

    /* give another round for window updates to settle */
    pump_both(client, server,
              &caddr,
              &saddr);
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* client -> server: input */
    const char *input = "move_left";
    netchan_chan_write(c_send, input, strlen(input));
    pump_both(client, server,
              &caddr,
              &saddr);

    /* read on server */
    char buf[256];
    int rd;
    while (netchan_poll(server, &ev)) {}
    rd = netchan_chan_read(s_recv_input, buf, sizeof(buf));
    CHECK(rd == (int)strlen(input), "input read wrong size");
    CHECK(memcmp(buf, input, rd) == 0, "input data mismatch");

    /* server -> client: state */
    const char *state = "pos:10,5";
    netchan_chan_write(s_send_state, state, strlen(state));
    pump_both(client, server,
              &caddr,
              &saddr);

    while (netchan_poll(client, &ev)) {}
    rd = netchan_chan_read(c_recv, buf, sizeof(buf));
    CHECK(rd == (int)strlen(state), "state read wrong size");
    CHECK(memcmp(buf, state, rd) == 0, "state data mismatch");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

static void
test_multiple_messages(void)
{
    TEST(multiple_reliable_messages);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10004);
    struct nc_addr saddr = make_addr(0x7f000001, 20004);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server,
              &caddr,
              &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_RELIABLE,
                                               NETCHAN_DIR_SEND, "bulk");
    pump_both(client, server,
              &caddr,
              &saddr);
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    CHECK(netchan_chan_state(ch) == 1, "channel not open");

    /* send 10 messages */
    for (int i = 0; i < 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "message-%d", i);
        int wr = netchan_chan_write(ch, msg, strlen(msg));
        CHECK(wr > 0, "write failed");
    }

    /* pump everything */
    pump_both(client, server,
              &caddr,
              &saddr);

    /* find server channel and read all messages */
    struct netchan_chan *sch = NULL;
    while (netchan_poll(server, &ev)) {
        if (ev.type == NETCHAN_EV_DATA && ev.ch)
            sch = ev.ch;
    }
    CHECK(sch != NULL, "no server channel with data");

    char buf[256];
    for (int i = 0; i < 10; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "message-%d", i);
        int rd = netchan_chan_read(sch, buf, sizeof(buf));
        CHECK(rd == (int)strlen(expected), "read wrong size");
        CHECK(memcmp(buf, expected, rd) == 0, "data mismatch");
    }

    /* should be empty */
    CHECK(netchan_chan_read(sch, buf, sizeof(buf)) == 0, "extra data");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

static void
test_connection_migration(void)
{
    TEST(connection_migration);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10005);
    struct nc_addr saddr = make_addr(0x7f000001, 20005);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server,
              &caddr,
              &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* open a channel so migration validation passes */
    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_UNRELIABLE,
                                               NETCHAN_DIR_SEND, "pos");
    pump_both(client, server,
              &caddr,
              &saddr);
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* write data from new address */
    const char *msg = "from_new_addr";
    netchan_chan_write(ch, msg, strlen(msg));

    /* client now has a new address */
    struct nc_addr new_caddr = make_addr(0x0a000001, 30000);
    pump(client, server, &new_caddr);

    /* server should still process the data */
    int found = 0;
    while (netchan_poll(server, &ev)) {
        if (ev.type == NETCHAN_EV_DATA)
            found = 1;
    }
    CHECK(found, "data not received after migration");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

static void
test_channel_close(void)
{
    TEST(channel_close);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10006);
    struct nc_addr saddr = make_addr(0x7f000001, 20006);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server,
              &caddr,
              &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_RELIABLE,
                                               NETCHAN_DIR_SEND, "temp");
    pump_both(client, server,
              &caddr,
              &saddr);
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* close the channel */
    netchan_chan_close(ch);
    pump(client, server, &caddr);

    /* server should get CHAN_CLOSE */
    int found_close = 0;
    while (netchan_poll(server, &ev)) {
        if (ev.type == NETCHAN_EV_CHAN_CLOSE)
            found_close = 1;
    }
    CHECK(found_close, "no chan_close event");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

static void
test_peek_id(void)
{
    TEST(peek_id);

    /* build a fake INIT packet */
    uint8_t pkt[16];
    pkt[0] = 0x01; /* INIT flag */
    pkt[1] = 0xDE;
    pkt[2] = 0xAD;
    pkt[3] = 0xBE;
    pkt[4] = 0xEF;

    uint32_t id = netchan_peek_id(pkt, 5);
    CHECK(id == 0xDEADBEEF, "peek_id wrong value");

    /* too short */
    CHECK(netchan_peek_id(pkt, 3) == 0, "peek_id should fail on short pkt");

    PASS();
}

static void
test_graceful_disconnect(void)
{
    TEST(graceful_disconnect);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10007);
    struct nc_addr saddr = make_addr(0x7f000001, 20007);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server,
              &caddr,
              &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* client closes -- sends DISCONNECT */
    netchan_close(client);

    /* the close queued a DISCONNECT frame, but also freed the conn.
     * We need a different approach: close sends disconnect then frees.
     * The server won't see it because we can't pump after free.
     * This tests that close doesn't crash. */

    netchan_close(server);
    PASS();
}

static void
test_stats(void)
{
    TEST(stats);

    struct netchan_conn *client = netchan_open(0);
    struct netchan_conn *server = netchan_open(1);

    struct nc_addr caddr = make_addr(0x7f000001, 10010);
    struct nc_addr saddr = make_addr(0x7f000001, 20010);

    netchan_connect(client, &saddr);
    pump(client, server, &caddr);
    netchan_accept(server);
    pump_both(client, server,
              &caddr,
              &saddr);

    struct netchan_event ev;
    while (netchan_poll(client, &ev)) {}
    while (netchan_poll(server, &ev)) {}

    /* conn stats should have non-zero packet counts from handshake */
    struct netchan_conn_stats cs;
    netchan_conn_stats(client, &cs);
    CHECK(cs.pkts_sent > 0, "client pkts_sent should be > 0 after handshake");
    CHECK(cs.pkts_recv > 0, "client pkts_recv should be > 0 after handshake");

    /* open a reliable channel and send 3 messages */
    struct netchan_chan *ch = netchan_chan_open(client, NETCHAN_RELIABLE,
                                               NETCHAN_DIR_SEND, "stats-test");
    CHECK(ch != NULL, "chan_open failed");
    pump_both(client, server,
              &caddr,
              &saddr);
    while (netchan_poll(server, &ev)) {}
    while (netchan_poll(client, &ev)) {}

    for (int i = 0; i < 3; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "msg-%d", i);
        int wr = netchan_chan_write(ch, msg, strlen(msg));
        CHECK(wr > 0, "write failed");
        pump_both(client, server,
                  &caddr,
                  &saddr);
    }

    /* drain server events and read data */
    struct netchan_chan *sch = NULL;
    while (netchan_poll(server, &ev)) {
        if (ev.type == NETCHAN_EV_DATA && ev.ch)
            sch = ev.ch;
    }
    CHECK(sch != NULL, "no data on server");

    char buf[256];
    while (netchan_chan_read(sch, buf, sizeof(buf)) > 0) {}

    /* check channel stats */
    struct netchan_chan_stats send_stats;
    netchan_chan_stats(ch, &send_stats);
    CHECK(send_stats.msgs_sent == 3, "expected 3 msgs_sent");
    CHECK(send_stats.msgs_acked == 3, "expected 3 msgs_acked");
    CHECK(send_stats.retransmissions == 0, "unexpected retransmissions");

    struct netchan_chan_stats recv_stats;
    netchan_chan_stats(sch, &recv_stats);
    CHECK(recv_stats.msgs_recv == 3, "expected 3 msgs_recv");

    netchan_close(client);
    netchan_close(server);
    PASS();
}

int
main(void)
{
    printf("netchan tests:\n");

    test_handshake();
    test_unreliable_datagram();
    test_reliable_datagram();
    test_reliable_retransmit();
    test_reliable_giveup();
    test_bidirectional_channels();
    test_multiple_messages();
    test_connection_migration();
    test_channel_close();
    test_peek_id();
    test_graceful_disconnect();
    test_stats();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
