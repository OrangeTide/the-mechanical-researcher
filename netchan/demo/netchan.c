/* netchan.c : multiplexed UDP channels for game networking */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#include "netchan.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/****************************************************************
 * Internal constants
 ****************************************************************/

#define NC_VERSION          0x01
#define NC_PKT_FLAG_INIT    0x01
#define NC_PKT_FLAG_PKTNUM  0x02
#define NC_HDR_INIT_SIZE    5
#define NC_HDR_FULL_SIZE    7

#define NC_FRAME_PADDING         0x00
#define NC_FRAME_CONNECT_INIT    0x01
#define NC_FRAME_CONNECT_ACCEPT  0x02
#define NC_FRAME_CONNECT_REDIRECT 0x03
#define NC_FRAME_DISCONNECT      0x04
#define NC_FRAME_PING            0x05
#define NC_FRAME_PONG            0x06
#define NC_FRAME_CHANNEL_OPEN    0x07
#define NC_FRAME_CHANNEL_CLOSE   0x08
#define NC_FRAME_DATA            0x09
#define NC_FRAME_ACK             0x0A
#define NC_FRAME_WINDOW_UPDATE   0x0B

#define NC_MAX_CHANNELS     256
#define NC_EVENT_QUEUE      16
#define NC_CTRL_BUF_SIZE    1024
#define NC_OUTGOING_SLOTS   64
#define NC_RECV_QUEUE       64
#define NC_REORDER_SLOTS    64
#define NC_FRAG_SLOTS       4
#define NC_MAX_FRAGS        32

#define NC_DEFAULT_MTU      1200
#define NC_DEFAULT_WINDOW   65536
#define NC_DEFAULT_IDLE_MS  30000
#define NC_DEFAULT_RT_MS    100
#define NC_MAX_RT_MS        1000
#define NC_MAX_RT_ATTEMPTS  5
#define NC_CONNECT_RETRY_MS 500
#define NC_CONNECT_RETRIES  5
#define NC_PING_INTERVAL_MS 5000

/****************************************************************
 * Byte I/O helpers
 ****************************************************************/

static void
wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void
wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint16_t
rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t
rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

/****************************************************************
 * Time helper
 ****************************************************************/

static uint32_t
nc_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static uint32_t
nc_random_id(void)
{
    uint32_t id = 0;
    FILE *f = fopen("/dev/urandom", "r");
    if (f) {
        if (fread(&id, sizeof(id), 1, f) != 1)
            id = 0;
        fclose(f);
    }
    if (id == 0)
        id = (uint32_t)rand() ^ ((uint32_t)rand() << 16);
    if (id == 0)
        id = 1;
    return id;
}

/****************************************************************
 * Internal structures
 ****************************************************************/

struct nc_outgoing {
    uint16_t seq;
    uint8_t *data;
    size_t   len;
    uint32_t sent_ms;
    uint8_t  attempts;
    uint8_t  frag_sent;
    uint8_t  frag_total;
    uint8_t  active;
};

struct nc_recv_entry {
    uint8_t *data;
    size_t   len;
};

struct nc_reorder {
    uint16_t seq;
    uint8_t *data;
    size_t   len;
    uint8_t  valid;
};

struct nc_frag_asm {
    uint16_t seq;
    uint8_t  total;
    uint8_t  have;
    uint32_t bitmask;
    uint8_t *data;
    size_t   alloc;
    size_t  *offsets;
    uint8_t  active;
};

struct netchan_chan {
    struct netchan_conn *conn;
    uint8_t  id;
    int      type;
    int      dir;
    int      role; // 0 = sender, 1 = receiver (local perspective)
    int      state;
    char     content_type[64];

    /* sender state */
    uint16_t send_seq;
    uint32_t send_window;
    uint32_t bytes_in_flight;
    struct nc_outgoing outgoing[NC_OUTGOING_SLOTS];
    int      out_head, out_tail;

    /* receiver state */
    uint16_t recv_seq;
    uint32_t recv_window;
    uint32_t recv_buffered;
    struct nc_recv_entry recv_queue[NC_RECV_QUEUE];
    int      rq_head, rq_tail;
    struct nc_reorder reorder[NC_REORDER_SLOTS];
    struct nc_frag_asm frags[NC_FRAG_SLOTS];
    uint8_t  need_ack;
    uint16_t ack_seq;
    uint8_t  need_window_update;
};

struct netchan_conn {
    uint32_t local_id;
    uint32_t remote_id;
    int      server;
    int      state;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    uint16_t next_pkt_num;
    uint32_t last_recv_ms;
    uint32_t last_send_ms;
    uint32_t connect_sent_ms;
    uint8_t  connect_attempts;
    uint32_t ping_sent_ms;
    uint32_t ping_opaque;
    uint32_t rtt_ms;
    struct netchan_cfg cfg;

    struct netchan_chan *channels[NC_MAX_CHANNELS];
    uint8_t next_chan_id;

    struct netchan_event events[NC_EVENT_QUEUE];
    int ev_head, ev_tail;

    uint8_t ctrl_buf[NC_CTRL_BUF_SIZE];
    size_t  ctrl_len;
};

/****************************************************************
 * Event queue
 ****************************************************************/

static void
ev_push(struct netchan_conn *c, int type, struct netchan_chan *ch)
{
    int next = (c->ev_tail + 1) % NC_EVENT_QUEUE;
    if (next == c->ev_head)
        return;
    memset(&c->events[c->ev_tail], 0, sizeof(c->events[0]));
    c->events[c->ev_tail].type = type;
    c->events[c->ev_tail].ch = ch;
    c->ev_tail = next;
}

static void
ev_push_redirect(struct netchan_conn *c, const struct sockaddr *addr,
                 socklen_t addrlen, uint32_t conn_id)
{
    int next = (c->ev_tail + 1) % NC_EVENT_QUEUE;
    if (next == c->ev_head)
        return;
    memset(&c->events[c->ev_tail], 0, sizeof(c->events[0]));
    c->events[c->ev_tail].type = NETCHAN_EV_REDIRECT;
    memcpy(&c->events[c->ev_tail].redirect_addr, addr, addrlen);
    c->events[c->ev_tail].redirect_addrlen = addrlen;
    c->events[c->ev_tail].redirect_conn_id = conn_id;
    c->ev_tail = next;
}

/****************************************************************
 * Control frame queue -- append serialized frames for next send
 ****************************************************************/

static uint8_t *
ctrl_reserve(struct netchan_conn *c, size_t n)
{
    if (c->ctrl_len + n > NC_CTRL_BUF_SIZE)
        return NULL;
    uint8_t *p = c->ctrl_buf + c->ctrl_len;
    c->ctrl_len += n;
    return p;
}

/****************************************************************
 * Frame encoding
 ****************************************************************/

static void
ctrl_connect_init(struct netchan_conn *c)
{
    uint8_t *p = ctrl_reserve(c, 6);
    if (!p) return;
    p[0] = NC_FRAME_CONNECT_INIT;
    wr32(p + 1, c->local_id);
    p[5] = NC_VERSION;
}

static void
ctrl_connect_accept(struct netchan_conn *c)
{
    uint8_t *p = ctrl_reserve(c, 10);
    if (!p) return;
    p[0] = NC_FRAME_CONNECT_ACCEPT;
    wr32(p + 1, c->local_id);
    p[5] = NC_VERSION;
    wr32(p + 6, c->cfg.idle_timeout_ms);
}

static void
ctrl_disconnect(struct netchan_conn *c, uint16_t reason)
{
    uint8_t *p = ctrl_reserve(c, 3);
    if (!p) return;
    p[0] = NC_FRAME_DISCONNECT;
    wr16(p + 1, reason);
}

static void
ctrl_ping(struct netchan_conn *c, uint32_t opaque)
{
    uint8_t *p = ctrl_reserve(c, 5);
    if (!p) return;
    p[0] = NC_FRAME_PING;
    wr32(p + 1, opaque);
}

static void
ctrl_pong(struct netchan_conn *c, uint32_t opaque)
{
    uint8_t *p = ctrl_reserve(c, 5);
    if (!p) return;
    p[0] = NC_FRAME_PONG;
    wr32(p + 1, opaque);
}

static void
ctrl_channel_open(struct netchan_conn *c, struct netchan_chan *ch)
{
    size_t ct_len = strlen(ch->content_type);
    if (ct_len > 63) ct_len = 63;
    uint8_t *p = ctrl_reserve(c, 5 + ct_len);
    if (!p) return;
    p[0] = NC_FRAME_CHANNEL_OPEN;
    p[1] = ch->id;
    p[2] = (uint8_t)ch->type;
    p[3] = (uint8_t)ch->dir;
    p[4] = (uint8_t)ct_len;
    memcpy(p + 5, ch->content_type, ct_len);
}

static void
ctrl_channel_close(struct netchan_conn *c, uint8_t chan_id, uint16_t error)
{
    uint8_t *p = ctrl_reserve(c, 4);
    if (!p) return;
    p[0] = NC_FRAME_CHANNEL_CLOSE;
    p[1] = chan_id;
    wr16(p + 2, error);
}

static void
ctrl_window_update(struct netchan_conn *c, uint8_t chan_id, uint32_t window)
{
    uint8_t *p = ctrl_reserve(c, 6);
    if (!p) return;
    p[0] = NC_FRAME_WINDOW_UPDATE;
    p[1] = chan_id;
    wr32(p + 2, window);
}

/****************************************************************
 * Channel helpers
 ****************************************************************/

static size_t
max_frag_payload(struct netchan_conn *c)
{
    /* packet header (7) + data frame header (8) */
    return c->cfg.mtu - NC_HDR_FULL_SIZE - 8;
}

static struct netchan_chan *
chan_new(struct netchan_conn *c, uint8_t id, int type, int dir,
        int role, const char *content_type)
{
    struct netchan_chan *ch = calloc(1, sizeof(*ch));
    if (!ch) return NULL;
    ch->conn = c;
    ch->id = id;
    ch->type = type;
    ch->dir = dir;
    ch->role = role;
    ch->state = 0; /* opening */
    if (content_type)
        snprintf(ch->content_type, sizeof(ch->content_type), "%s", content_type);
    ch->send_window = 0;
    ch->recv_window = c->cfg.chan_window;
    c->channels[id] = ch;
    return ch;
}

static void
chan_free(struct netchan_chan *ch)
{
    if (!ch) return;
    for (int i = 0; i < NC_OUTGOING_SLOTS; i++) {
        if (ch->outgoing[i].active)
            free(ch->outgoing[i].data);
    }
    for (int i = 0; i < NC_RECV_QUEUE; i++) {
        free(ch->recv_queue[i].data);
    }
    for (int i = 0; i < NC_REORDER_SLOTS; i++) {
        free(ch->reorder[i].data);
    }
    for (int i = 0; i < NC_FRAG_SLOTS; i++) {
        free(ch->frags[i].data);
        free(ch->frags[i].offsets);
    }
    if (ch->conn)
        ch->conn->channels[ch->id] = NULL;
    free(ch);
}

static int
chan_recv_enqueue(struct netchan_chan *ch, const uint8_t *data, size_t len)
{
    int next = (ch->rq_tail + 1) % NC_RECV_QUEUE;
    if (next == ch->rq_head)
        return NETCHAN_ERR;
    ch->recv_queue[ch->rq_tail].data = malloc(len);
    if (!ch->recv_queue[ch->rq_tail].data)
        return NETCHAN_ERR_NOMEM;
    memcpy(ch->recv_queue[ch->rq_tail].data, data, len);
    ch->recv_queue[ch->rq_tail].len = len;
    ch->rq_tail = next;
    ch->recv_buffered += len;
    return NETCHAN_OK;
}

static void
chan_deliver_reordered(struct netchan_chan *ch)
{
    for (;;) {
        int slot = ch->recv_seq % NC_REORDER_SLOTS;
        struct nc_reorder *r = &ch->reorder[slot];
        if (!r->valid || r->seq != ch->recv_seq)
            break;
        chan_recv_enqueue(ch, r->data, r->len);
        ev_push(ch->conn, NETCHAN_EV_DATA, ch);
        free(r->data);
        r->data = NULL;
        r->valid = 0;
        ch->recv_seq++;
    }
}

/****************************************************************
 * Fragment reassembly
 ****************************************************************/

static struct nc_frag_asm *
frag_find(struct netchan_chan *ch, uint16_t seq)
{
    for (int i = 0; i < NC_FRAG_SLOTS; i++) {
        if (ch->frags[i].active && ch->frags[i].seq == seq)
            return &ch->frags[i];
    }
    return NULL;
}

static struct nc_frag_asm *
frag_alloc(struct netchan_chan *ch, uint16_t seq, uint8_t total)
{
    for (int i = 0; i < NC_FRAG_SLOTS; i++) {
        if (!ch->frags[i].active) {
            struct nc_frag_asm *f = &ch->frags[i];
            memset(f, 0, sizeof(*f));
            f->seq = seq;
            f->total = total;
            f->active = 1;
            size_t payload_max = max_frag_payload(ch->conn);
            f->alloc = payload_max * total;
            f->data = malloc(f->alloc);
            f->offsets = calloc(total, sizeof(size_t));
            if (!f->data || !f->offsets) {
                free(f->data);
                free(f->offsets);
                memset(f, 0, sizeof(*f));
                return NULL;
            }
            return f;
        }
    }
    return NULL;
}

static void
frag_free(struct nc_frag_asm *f)
{
    free(f->data);
    free(f->offsets);
    memset(f, 0, sizeof(*f));
}

/****************************************************************
 * Frame processing
 ****************************************************************/

static int
process_connect_init(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 6) return NETCHAN_ERR_PROTO;
    if (!c->server) return NETCHAN_ERR_PROTO;
    uint32_t client_id = rd32(p + 1);
    uint8_t version = p[5];
    if (version != NC_VERSION) return NETCHAN_ERR_PROTO;
    c->remote_id = client_id;
    if (c->state == NETCHAN_STATE_NEW) {
        c->local_id = nc_random_id();
        c->state = NETCHAN_STATE_CONNECTING;
    }
    return 6;
}

static int
process_connect_accept(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 10) return NETCHAN_ERR_PROTO;
    if (c->server) return NETCHAN_ERR_PROTO;
    uint32_t server_id = rd32(p + 1);
    uint8_t version = p[5];
    (void)version;
    uint32_t idle = rd32(p + 6);
    (void)idle;
    c->remote_id = server_id;
    c->state = NETCHAN_STATE_CONNECTED;
    ev_push(c, NETCHAN_EV_CONNECTED, NULL);
    return 10;
}

static int
process_connect_redirect(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 12) return NETCHAN_ERR_PROTO;
    uint32_t new_id = rd32(p + 1);
    uint8_t addr_type = p[5];
    struct sockaddr_storage sa;
    socklen_t sa_len;
    memset(&sa, 0, sizeof(sa));

    if (addr_type == 4) {
        if (len < 12) return NETCHAN_ERR_PROTO;
        struct sockaddr_in *sin = (struct sockaddr_in *)&sa;
        sin->sin_family = AF_INET;
        memcpy(&sin->sin_addr, p + 6, 4);
        sin->sin_port = htons(rd16(p + 10));
        sa_len = sizeof(*sin);
        ev_push_redirect(c, (struct sockaddr *)&sa, sa_len, new_id);
        return 12;
    } else if (addr_type == 6) {
        if (len < 24) return NETCHAN_ERR_PROTO;
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&sa;
        sin6->sin6_family = AF_INET6;
        memcpy(&sin6->sin6_addr, p + 6, 16);
        sin6->sin6_port = htons(rd16(p + 22));
        sa_len = sizeof(*sin6);
        ev_push_redirect(c, (struct sockaddr *)&sa, sa_len, new_id);
        return 24;
    }
    return NETCHAN_ERR_PROTO;
}

static int
process_disconnect(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 3) return NETCHAN_ERR_PROTO;
    (void)p;
    c->state = NETCHAN_STATE_CLOSED;
    ev_push(c, NETCHAN_EV_DISCONNECTED, NULL);
    return 3;
}

static int
process_ping(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 5) return NETCHAN_ERR_PROTO;
    uint32_t opaque = rd32(p + 1);
    ctrl_pong(c, opaque);
    return 5;
}

static int
process_pong(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 5) return NETCHAN_ERR_PROTO;
    uint32_t opaque = rd32(p + 1);
    if (opaque == c->ping_opaque && c->ping_sent_ms) {
        uint32_t now = nc_now_ms();
        c->rtt_ms = now - c->ping_sent_ms;
        c->ping_sent_ms = 0;
    }
    return 5;
}

static int
process_channel_open(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 5) return NETCHAN_ERR_PROTO;
    uint8_t chan_id = p[1];
    int type = p[2];
    int dir = p[3];
    uint8_t ct_len = p[4];
    if (len < (size_t)(5 + ct_len)) return NETCHAN_ERR_PROTO;

    if (c->channels[chan_id]) {
        ctrl_channel_close(c, chan_id, 1);
        return 5 + ct_len;
    }

    /* remote's direction is flipped for us */
    int local_role = (dir == NETCHAN_DIR_SEND) ? 1 : 0; /* they send, we recv */
    char ct[64] = {0};
    if (ct_len > 0) {
        size_t n = ct_len < 63 ? ct_len : 63;
        memcpy(ct, p + 5, n);
    }

    struct netchan_chan *ch = chan_new(c, chan_id, type, dir, local_role, ct);
    if (!ch) {
        ctrl_channel_close(c, chan_id, 1);
        return 5 + ct_len;
    }
    ch->state = 1; /* open */

    if (local_role == 1) {
        /* we are receiver: send initial window update */
        ctrl_window_update(c, chan_id, ch->recv_window);
    }

    ev_push(c, NETCHAN_EV_CHAN_OPEN, ch);
    return 5 + ct_len;
}

static int
process_channel_close(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 4) return NETCHAN_ERR_PROTO;
    uint8_t chan_id = p[1];
    struct netchan_chan *ch = c->channels[chan_id];
    if (ch) {
        ch->state = 3; /* closed */
        ev_push(c, NETCHAN_EV_CHAN_CLOSE, ch);
    }
    return 4;
}

static int
process_data(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 8) return NETCHAN_ERR_PROTO;
    uint8_t chan_id = p[1];
    uint16_t seq = rd16(p + 2);
    uint16_t data_len = rd16(p + 4);
    uint8_t frag_idx = p[6];
    uint8_t frag_total = p[7];
    if (len < (size_t)(8 + data_len)) return NETCHAN_ERR_PROTO;
    const uint8_t *payload = p + 8;

    struct netchan_chan *ch = c->channels[chan_id];
    if (!ch || ch->role != 1) return 8 + data_len;

    if (ch->type == NETCHAN_UNRELIABLE) {
        if (frag_total <= 1) {
            chan_recv_enqueue(ch, payload, data_len);
            ev_push(c, NETCHAN_EV_DATA, ch);
        } else {
            struct nc_frag_asm *f = frag_find(ch, seq);
            if (!f) f = frag_alloc(ch, seq, frag_total);
            if (f && frag_idx < f->total && !(f->bitmask & (1u << frag_idx))) {
                size_t off = (size_t)frag_idx * max_frag_payload(c);
                if (off + data_len <= f->alloc) {
                    memcpy(f->data + off, payload, data_len);
                    f->offsets[frag_idx] = data_len;
                    f->bitmask |= (1u << frag_idx);
                    f->have++;
                    if (f->have == f->total) {
                        size_t total = 0;
                        for (int i = 0; i < f->total; i++)
                            total += f->offsets[i];
                        /* reassemble contiguously */
                        uint8_t *assembled = malloc(total);
                        if (assembled) {
                            size_t pos = 0;
                            size_t fp = max_frag_payload(c);
                            for (int i = 0; i < f->total; i++) {
                                memcpy(assembled + pos, f->data + i * fp,
                                       f->offsets[i]);
                                pos += f->offsets[i];
                            }
                            chan_recv_enqueue(ch, assembled, total);
                            ev_push(c, NETCHAN_EV_DATA, ch);
                            free(assembled);
                        }
                        frag_free(f);
                    }
                }
            }
        }
        return 8 + data_len;
    }

    /* reliable datagram */
    if (frag_total <= 1) {
        /* unfragmented */
        if (seq == ch->recv_seq) {
            chan_recv_enqueue(ch, payload, data_len);
            ev_push(c, NETCHAN_EV_DATA, ch);
            ch->recv_seq++;
            chan_deliver_reordered(ch);
        } else {
            int16_t diff = (int16_t)(seq - ch->recv_seq);
            if (diff > 0 && diff < NC_REORDER_SLOTS) {
                int slot = seq % NC_REORDER_SLOTS;
                if (!ch->reorder[slot].valid) {
                    ch->reorder[slot].seq = seq;
                    ch->reorder[slot].data = malloc(data_len);
                    if (ch->reorder[slot].data) {
                        memcpy(ch->reorder[slot].data, payload, data_len);
                        ch->reorder[slot].len = data_len;
                        ch->reorder[slot].valid = 1;
                    }
                }
            }
        }
    } else {
        /* fragmented reliable */
        struct nc_frag_asm *f = frag_find(ch, seq);
        if (!f) f = frag_alloc(ch, seq, frag_total);
        if (f && frag_idx < f->total && !(f->bitmask & (1u << frag_idx))) {
            size_t off = (size_t)frag_idx * max_frag_payload(c);
            if (off + data_len <= f->alloc) {
                memcpy(f->data + off, payload, data_len);
                f->offsets[frag_idx] = data_len;
                f->bitmask |= (1u << frag_idx);
                f->have++;
                if (f->have == f->total) {
                    size_t total = 0;
                    for (int i = 0; i < f->total; i++)
                        total += f->offsets[i];
                    uint8_t *assembled = malloc(total);
                    if (assembled) {
                        size_t pos = 0;
                        size_t fp = max_frag_payload(c);
                        for (int i = 0; i < f->total; i++) {
                            memcpy(assembled + pos, f->data + i * fp,
                                   f->offsets[i]);
                            pos += f->offsets[i];
                        }
                        /* deliver in order */
                        if (seq == ch->recv_seq) {
                            chan_recv_enqueue(ch, assembled, total);
                            ev_push(c, NETCHAN_EV_DATA, ch);
                            ch->recv_seq++;
                            chan_deliver_reordered(ch);
                        } else {
                            int16_t diff = (int16_t)(seq - ch->recv_seq);
                            if (diff > 0 && diff < NC_REORDER_SLOTS) {
                                int slot = seq % NC_REORDER_SLOTS;
                                if (!ch->reorder[slot].valid) {
                                    ch->reorder[slot].seq = seq;
                                    ch->reorder[slot].data = malloc(total);
                                    if (ch->reorder[slot].data) {
                                        memcpy(ch->reorder[slot].data,
                                               assembled, total);
                                        ch->reorder[slot].len = total;
                                        ch->reorder[slot].valid = 1;
                                    }
                                }
                            }
                        }
                        free(assembled);
                    }
                    frag_free(f);
                }
            }
        }
    }

    ch->need_ack = 1;
    ch->ack_seq = ch->recv_seq > 0 ? ch->recv_seq - 1 : 0;
    return 8 + data_len;
}

static int
process_ack(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 4) return NETCHAN_ERR_PROTO;
    uint8_t chan_id = p[1];
    uint16_t acked_seq = rd16(p + 2);

    struct netchan_chan *ch = c->channels[chan_id];
    if (!ch || ch->role != 0) return 4;

    for (int i = 0; i < NC_OUTGOING_SLOTS; i++) {
        struct nc_outgoing *o = &ch->outgoing[i];
        if (!o->active) continue;
        int16_t diff = (int16_t)(o->seq - acked_seq);
        if (diff <= 0) {
            ch->bytes_in_flight -= o->len;
            free(o->data);
            o->data = NULL;
            o->active = 0;
        }
    }

    /* advance head past cleared slots */
    while (ch->out_head != ch->out_tail &&
           !ch->outgoing[ch->out_head].active)
        ch->out_head = (ch->out_head + 1) % NC_OUTGOING_SLOTS;

    return 4;
}

static int
process_window_update(struct netchan_conn *c, const uint8_t *p, size_t len)
{
    if (len < 6) return NETCHAN_ERR_PROTO;
    uint8_t chan_id = p[1];
    uint32_t window = rd32(p + 2);

    struct netchan_chan *ch = c->channels[chan_id];
    if (!ch) return 6;

    if (ch->role == 0) {
        ch->send_window = window;
        if (ch->state == 0)
            ch->state = 1; /* first window update = channel accepted */
    }
    return 6;
}

/****************************************************************
 * Packet parsing
 ****************************************************************/

static int
parse_frames(struct netchan_conn *c, const uint8_t *data, size_t len)
{
    size_t pos = 0;
    while (pos < len) {
        if (data[pos] == NC_FRAME_PADDING) {
            pos++;
            continue;
        }
        int consumed;
        switch (data[pos]) {
        case NC_FRAME_CONNECT_INIT:
            consumed = process_connect_init(c, data + pos, len - pos);
            break;
        case NC_FRAME_CONNECT_ACCEPT:
            consumed = process_connect_accept(c, data + pos, len - pos);
            break;
        case NC_FRAME_CONNECT_REDIRECT:
            consumed = process_connect_redirect(c, data + pos, len - pos);
            break;
        case NC_FRAME_DISCONNECT:
            consumed = process_disconnect(c, data + pos, len - pos);
            break;
        case NC_FRAME_PING:
            consumed = process_ping(c, data + pos, len - pos);
            break;
        case NC_FRAME_PONG:
            consumed = process_pong(c, data + pos, len - pos);
            break;
        case NC_FRAME_CHANNEL_OPEN:
            consumed = process_channel_open(c, data + pos, len - pos);
            break;
        case NC_FRAME_CHANNEL_CLOSE:
            consumed = process_channel_close(c, data + pos, len - pos);
            break;
        case NC_FRAME_DATA:
            consumed = process_data(c, data + pos, len - pos);
            break;
        case NC_FRAME_ACK:
            consumed = process_ack(c, data + pos, len - pos);
            break;
        case NC_FRAME_WINDOW_UPDATE:
            consumed = process_window_update(c, data + pos, len - pos);
            break;
        default:
            return NETCHAN_ERR_PROTO;
        }
        if (consumed < 0) return consumed;
        pos += consumed;
    }
    return NETCHAN_OK;
}

/****************************************************************
 * Connection migration validation
 ****************************************************************/

static int
validate_migration(struct netchan_conn *c, const uint8_t *frames, size_t len)
{
    size_t pos = 0;
    while (pos < len) {
        uint8_t ftype = frames[pos];
        if (ftype == NC_FRAME_DATA && pos + 4 <= len) {
            uint8_t chan_id = frames[pos + 1];
            if (c->channels[chan_id])
                return 1;
        }
        if (ftype == NC_FRAME_ACK && pos + 2 <= len) {
            uint8_t chan_id = frames[pos + 1];
            if (c->channels[chan_id])
                return 1;
        }
        break;
    }
    return 0;
}

/****************************************************************
 * Public API -- Configuration
 ****************************************************************/

void
netchan_cfg_default(struct netchan_cfg *cfg)
{
    cfg->mtu = NC_DEFAULT_MTU;
    cfg->chan_window = NC_DEFAULT_WINDOW;
    cfg->idle_timeout_ms = NC_DEFAULT_IDLE_MS;
    cfg->retransmit_ms = NC_DEFAULT_RT_MS;
}

/****************************************************************
 * Public API -- Connection lifecycle
 ****************************************************************/

struct netchan_conn *
netchan_open(int server)
{
    struct netchan_conn *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->server = server;
    c->state = NETCHAN_STATE_NEW;
    c->next_chan_id = server ? 1 : 0;
    c->rtt_ms = 100;
    netchan_cfg_default(&c->cfg);
    return c;
}

void
netchan_close(struct netchan_conn *c)
{
    if (!c) return;
    if (c->state == NETCHAN_STATE_CONNECTED)
        ctrl_disconnect(c, 0);
    for (int i = 0; i < NC_MAX_CHANNELS; i++)
        chan_free(c->channels[i]);
    free(c);
}

void
netchan_config(struct netchan_conn *c, const struct netchan_cfg *cfg)
{
    c->cfg = *cfg;
}

int
netchan_state(struct netchan_conn *c)
{
    return c->state;
}

uint32_t
netchan_id(struct netchan_conn *c)
{
    return c->local_id;
}

int
netchan_connect(struct netchan_conn *c, const struct sockaddr *addr,
                socklen_t addrlen)
{
    if (c->server) return NETCHAN_ERR;
    if (c->state != NETCHAN_STATE_NEW) return NETCHAN_ERR;
    c->local_id = nc_random_id();
    memcpy(&c->peer_addr, addr, addrlen);
    c->peer_addr_len = addrlen;
    c->state = NETCHAN_STATE_CONNECTING;
    ctrl_connect_init(c);
    c->connect_sent_ms = nc_now_ms();
    c->connect_attempts = 1;
    return NETCHAN_OK;
}

int
netchan_accept(struct netchan_conn *c)
{
    if (!c->server) return NETCHAN_ERR;
    if (c->state != NETCHAN_STATE_CONNECTING) return NETCHAN_ERR;
    ctrl_connect_accept(c);
    c->state = NETCHAN_STATE_CONNECTED;
    ev_push(c, NETCHAN_EV_CONNECTED, NULL);
    return NETCHAN_OK;
}

/****************************************************************
 * Public API -- Packet I/O
 ****************************************************************/

uint32_t
netchan_peek_id(const void *pkt, size_t len)
{
    const uint8_t *p = pkt;
    if (len < NC_HDR_INIT_SIZE) return 0;
    return rd32(p + 1);
}

int
netchan_feed(struct netchan_conn *c, const void *pkt, size_t len,
             const struct sockaddr *from, socklen_t fromlen)
{
    const uint8_t *p = pkt;
    if (len < NC_HDR_INIT_SIZE) return NETCHAN_ERR_PROTO;

    uint8_t flags = p[0];
    uint32_t conn_id = rd32(p + 1);

    size_t hdr_size;
    if (flags & NC_PKT_FLAG_INIT) {
        hdr_size = NC_HDR_INIT_SIZE;
    } else {
        if (len < NC_HDR_FULL_SIZE) return NETCHAN_ERR_PROTO;
        hdr_size = NC_HDR_FULL_SIZE;
    }

    /* for non-INIT packets, verify conn_id matches our local_id */
    if (!(flags & NC_PKT_FLAG_INIT) && conn_id != c->local_id)
        return NETCHAN_ERR_PROTO;

    /* connection migration: update peer address if changed */
    if (from && fromlen > 0 && c->peer_addr_len > 0 &&
        c->state == NETCHAN_STATE_CONNECTED) {
        if (fromlen != c->peer_addr_len ||
            memcmp(from, &c->peer_addr, fromlen) != 0) {
            if (validate_migration(c, p + hdr_size, len - hdr_size)) {
                memcpy(&c->peer_addr, from, fromlen);
                c->peer_addr_len = fromlen;
            }
        }
    }

    /* store peer address on first packet (server side) */
    if (c->peer_addr_len == 0 && from && fromlen > 0) {
        memcpy(&c->peer_addr, from, fromlen);
        c->peer_addr_len = fromlen;
    }

    c->last_recv_ms = nc_now_ms();
    return parse_frames(c, p + hdr_size, len - hdr_size);
}

size_t
netchan_send_next(struct netchan_conn *c, void *buf, size_t buflen,
                  struct sockaddr *to, socklen_t *tolen)
{
    /* nothing to send? check ctrl queue and channel data */
    int has_data = 0;
    if (c->ctrl_len > 0)
        has_data = 1;
    if (!has_data) {
        for (int i = 0; i < NC_MAX_CHANNELS; i++) {
            struct netchan_chan *ch = c->channels[i];
            if (!ch || ch->role != 0) continue;
            if (ch->out_head != ch->out_tail)
                has_data = 1;
            if (ch->need_ack || ch->need_window_update)
                has_data = 1;
        }
        /* also check receiver channels for ACK/window */
        for (int i = 0; i < NC_MAX_CHANNELS; i++) {
            struct netchan_chan *ch = c->channels[i];
            if (!ch || ch->role != 1) continue;
            if (ch->need_ack || ch->need_window_update)
                has_data = 1;
        }
    }
    if (!has_data)
        return 0;

    uint8_t *p = buf;
    int is_init = (c->state == NETCHAN_STATE_CONNECTING && !c->server);
    size_t hdr_size = is_init ? NC_HDR_INIT_SIZE : NC_HDR_FULL_SIZE;
    if (buflen < hdr_size)
        return 0;

    /* write packet header */
    uint8_t pkt_flags = 0;
    if (is_init) {
        pkt_flags |= NC_PKT_FLAG_INIT;
        p[0] = pkt_flags;
        wr32(p + 1, c->local_id);
    } else {
        pkt_flags |= NC_PKT_FLAG_PKTNUM;
        p[0] = pkt_flags;
        wr32(p + 1, c->remote_id);
        wr16(p + 5, c->next_pkt_num++);
    }

    size_t pos = hdr_size;
    size_t mtu = c->cfg.mtu;
    if (mtu > buflen) mtu = buflen;

    /* flush control frames */
    if (c->ctrl_len > 0) {
        size_t avail = mtu - pos;
        size_t n = c->ctrl_len < avail ? c->ctrl_len : avail;
        memcpy(p + pos, c->ctrl_buf, n);
        pos += n;
        if (n < c->ctrl_len) {
            memmove(c->ctrl_buf, c->ctrl_buf + n, c->ctrl_len - n);
            c->ctrl_len -= n;
        } else {
            c->ctrl_len = 0;
        }
    }

    /* flush pending ACKs and window updates from channels */
    for (int i = 0; i < NC_MAX_CHANNELS && pos + 6 <= mtu; i++) {
        struct netchan_chan *ch = c->channels[i];
        if (!ch) continue;
        if (ch->need_ack && pos + 4 <= mtu) {
            p[pos++] = NC_FRAME_ACK;
            p[pos++] = ch->id;
            wr16(p + pos, ch->ack_seq);
            pos += 2;
            ch->need_ack = 0;
        }
        if (ch->need_window_update && pos + 6 <= mtu) {
            uint32_t window = ch->recv_window > ch->recv_buffered ?
                              ch->recv_window - ch->recv_buffered : 0;
            p[pos++] = NC_FRAME_WINDOW_UPDATE;
            p[pos++] = ch->id;
            wr32(p + pos, window);
            pos += 4;
            ch->need_window_update = 0;
        }
    }

    /* send DATA from channels */
    for (int i = 0; i < NC_MAX_CHANNELS && pos + 9 <= mtu; i++) {
        struct netchan_chan *ch = c->channels[i];
        if (!ch || ch->role != 0 || ch->state != 1) continue;
        if (ch->out_head == ch->out_tail) continue;

        struct nc_outgoing *o = &ch->outgoing[ch->out_head];
        if (!o->active) continue;

        size_t frag_payload = max_frag_payload(c);
        size_t avail = mtu - pos - 8;
        if (avail == 0) break;

        uint8_t fi = o->frag_sent;
        size_t offset = (size_t)fi * frag_payload;
        size_t dlen = o->len - offset;
        if (dlen > frag_payload) dlen = frag_payload;
        if (dlen > avail) break; /* can't fit this fragment */

        p[pos++] = NC_FRAME_DATA;
        p[pos++] = ch->id;
        wr16(p + pos, o->seq); pos += 2;
        wr16(p + pos, (uint16_t)dlen); pos += 2;
        p[pos++] = fi;
        p[pos++] = o->frag_total;
        memcpy(p + pos, o->data + offset, dlen);
        pos += dlen;

        o->frag_sent++;
        o->sent_ms = nc_now_ms();
        o->attempts = 1;

        if (o->frag_sent >= o->frag_total) {
            if (ch->type == NETCHAN_UNRELIABLE) {
                free(o->data);
                o->data = NULL;
                o->active = 0;
                ch->out_head = (ch->out_head + 1) % NC_OUTGOING_SLOTS;
            } else {
                /* keep in outgoing for retransmit until ACKed */
                ch->out_head = (ch->out_head + 1) % NC_OUTGOING_SLOTS;
            }
        }
    }

    /* report destination address */
    if (to && tolen && c->peer_addr_len > 0) {
        socklen_t copylen = c->peer_addr_len;
        if (copylen > *tolen) copylen = *tolen;
        memcpy(to, &c->peer_addr, copylen);
        *tolen = c->peer_addr_len;
    }

    c->last_send_ms = nc_now_ms();
    return pos;
}

int
netchan_service(struct netchan_conn *c, uint32_t now_ms)
{
    int next_ms = -1;

    /* connection handshake retry */
    if (c->state == NETCHAN_STATE_CONNECTING && !c->server) {
        uint32_t elapsed = now_ms - c->connect_sent_ms;
        if (elapsed >= NC_CONNECT_RETRY_MS) {
            if (c->connect_attempts >= NC_CONNECT_RETRIES) {
                c->state = NETCHAN_STATE_CLOSED;
                ev_push(c, NETCHAN_EV_DISCONNECTED, NULL);
                return -1;
            }
            ctrl_connect_init(c);
            c->connect_sent_ms = now_ms;
            c->connect_attempts++;
        }
        int remain = NC_CONNECT_RETRY_MS - elapsed;
        if (remain < 0) remain = 0;
        if (next_ms < 0 || remain < next_ms) next_ms = remain;
    }

    /* idle timeout */
    if (c->state == NETCHAN_STATE_CONNECTED && c->last_recv_ms > 0) {
        uint32_t idle = now_ms - c->last_recv_ms;
        if (idle >= c->cfg.idle_timeout_ms) {
            c->state = NETCHAN_STATE_CLOSED;
            ev_push(c, NETCHAN_EV_DISCONNECTED, NULL);
            return -1;
        }
        int remain = c->cfg.idle_timeout_ms - idle;
        if (next_ms < 0 || remain < next_ms) next_ms = remain;
    }

    /* keepalive ping */
    if (c->state == NETCHAN_STATE_CONNECTED && c->last_send_ms > 0) {
        uint32_t since_send = now_ms - c->last_send_ms;
        if (since_send >= NC_PING_INTERVAL_MS && c->ping_sent_ms == 0) {
            c->ping_opaque = nc_random_id();
            ctrl_ping(c, c->ping_opaque);
            c->ping_sent_ms = now_ms;
        }
        if (since_send < NC_PING_INTERVAL_MS) {
            int remain = NC_PING_INTERVAL_MS - since_send;
            if (next_ms < 0 || remain < next_ms) next_ms = remain;
        }
    }

    /* retransmit for reliable channels */
    for (int i = 0; i < NC_MAX_CHANNELS; i++) {
        struct netchan_chan *ch = c->channels[i];
        if (!ch || ch->role != 0 || ch->type != NETCHAN_RELIABLE) continue;

        for (int j = 0; j < NC_OUTGOING_SLOTS; j++) {
            struct nc_outgoing *o = &ch->outgoing[j];
            if (!o->active || o->sent_ms == 0) continue;

            uint32_t timeout = c->cfg.retransmit_ms << (o->attempts - 1);
            if (timeout > NC_MAX_RT_MS) timeout = NC_MAX_RT_MS;
            uint32_t elapsed = now_ms - o->sent_ms;

            if (elapsed >= timeout) {
                if (o->attempts >= NC_MAX_RT_ATTEMPTS) {
                    ch->state = 3; /* dead */
                    ev_push(c, NETCHAN_EV_CHAN_CLOSE, ch);
                    break;
                }
                /* re-queue for sending */
                o->frag_sent = 0;
                o->sent_ms = now_ms;
                o->attempts++;
            } else {
                int remain = timeout - elapsed;
                if (next_ms < 0 || (int)remain < next_ms)
                    next_ms = remain;
            }
        }
    }

    return next_ms;
}

/****************************************************************
 * Public API -- Channel
 ****************************************************************/

struct netchan_chan *
netchan_chan_open(struct netchan_conn *c, int type, int dir,
                 const char *content_type)
{
    if (c->state != NETCHAN_STATE_CONNECTED) return NULL;

    uint8_t id = c->next_chan_id;
    if (c->channels[id]) return NULL;
    c->next_chan_id += 2;

    int local_role = (dir == NETCHAN_DIR_SEND) ? 0 : 1;
    struct netchan_chan *ch = chan_new(c, id, type, dir, local_role,
                                     content_type);
    if (!ch) return NULL;

    ctrl_channel_open(c, ch);

    if (local_role == 1) {
        /* we are receiver: send initial window update */
        ctrl_window_update(c, id, ch->recv_window);
        ch->state = 1; /* open immediately for receiver side */
    }

    return ch;
}

void
netchan_chan_close(struct netchan_chan *ch)
{
    if (!ch || ch->state >= 2) return;
    ctrl_channel_close(ch->conn, ch->id, 0);
    ch->state = 2; /* closing */
}

int
netchan_chan_id(struct netchan_chan *ch)
{
    return ch->id;
}

int
netchan_chan_type(struct netchan_chan *ch)
{
    return ch->type;
}

int
netchan_chan_state(struct netchan_chan *ch)
{
    return ch->state;
}

int
netchan_chan_write(struct netchan_chan *ch, const void *data, size_t len)
{
    if (!ch || ch->role != 0) return NETCHAN_ERR;
    if (ch->state == 3) return NETCHAN_ERR_CLOSED;
    if (len == 0) return 0;

    /* flow control: check window */
    if (ch->type == NETCHAN_RELIABLE && ch->state == 1) {
        if (ch->bytes_in_flight + len > ch->send_window)
            return NETCHAN_ERR_FLOW;
    }

    /* check send queue capacity */
    int next = (ch->out_tail + 1) % NC_OUTGOING_SLOTS;
    if (next == ch->out_head)
        return NETCHAN_ERR_AGAIN;

    struct nc_outgoing *o = &ch->outgoing[ch->out_tail];
    memset(o, 0, sizeof(*o));
    o->data = malloc(len);
    if (!o->data) return NETCHAN_ERR_NOMEM;
    memcpy(o->data, data, len);
    o->len = len;
    o->seq = ch->send_seq++;
    o->active = 1;

    size_t frag_payload = max_frag_payload(ch->conn);
    o->frag_total = (uint8_t)((len + frag_payload - 1) / frag_payload);
    if (o->frag_total == 0) o->frag_total = 1;
    if (o->frag_total > NC_MAX_FRAGS) {
        free(o->data);
        memset(o, 0, sizeof(*o));
        return NETCHAN_ERR_TOOBIG;
    }

    if (ch->type == NETCHAN_RELIABLE)
        ch->bytes_in_flight += len;

    ch->out_tail = next;
    return (int)len;
}

int
netchan_chan_read(struct netchan_chan *ch, void *buf, size_t buflen)
{
    if (!ch || ch->role != 1) return NETCHAN_ERR;
    if (ch->rq_head == ch->rq_tail) return 0;

    struct nc_recv_entry *e = &ch->recv_queue[ch->rq_head];
    size_t n = e->len < buflen ? e->len : buflen;
    memcpy(buf, e->data, n);
    free(e->data);
    e->data = NULL;
    e->len = 0;
    ch->rq_head = (ch->rq_head + 1) % NC_RECV_QUEUE;

    ch->recv_buffered -= n;
    ch->need_window_update = 1;

    return (int)n;
}

/****************************************************************
 * Public API -- Events
 ****************************************************************/

int
netchan_poll(struct netchan_conn *c, struct netchan_event *ev)
{
    if (c->ev_head == c->ev_tail) {
        if (ev) ev->type = NETCHAN_EV_NONE;
        return 0;
    }
    if (ev)
        *ev = c->events[c->ev_head];
    c->ev_head = (c->ev_head + 1) % NC_EVENT_QUEUE;
    return 1;
}
