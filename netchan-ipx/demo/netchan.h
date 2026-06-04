/* netchan.h : multiplexed reliable/unreliable channels over IPX or UDP */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#ifndef NETCHAN_H
#define NETCHAN_H

#include <stdint.h>
#include <stddef.h>
#include "nc_addr.h"

/****************************************************************
 * Compile-time tunables
 *
 * Override with -D on the command line to shrink the static
 * footprint for memory-constrained targets (e.g. 16-bit DOS).
 * The defaults are sized for a 4-player game.
 ****************************************************************/

#ifndef NC_MTU
#define NC_MTU 546          /* IPX media-independent payload guarantee   */
#endif
#ifndef NC_WINDOW
#define NC_WINDOW 8         /* reliable send window, in messages         */
#endif
#ifndef NC_MAX_CHAN
#define NC_MAX_CHAN 2       /* mux channels per connection               */
#endif
#ifndef NC_RECVQ
#define NC_RECVQ NC_WINDOW  /* per-channel recv slots; must hold a full
                             * reliable window or delivery can stall      */
#endif
#ifndef NC_UNREL_TXQ
#define NC_UNREL_TXQ 4      /* queued unreliable datagrams awaiting send */
#endif

/* Suggested peer-pool size for a server. The core is one connection per
 * peer; the application keeps an array of this many and routes with
 * nc_peek_id(). Not used by the core itself. */
#ifndef NC_MAX_CONN
#define NC_MAX_CONN 4
#endif

/* Largest application datagram: one packet header + one record header. */
#define NC_MAXMSG (NC_MTU - 8 - 6)

/****************************************************************
 * Constants
 ****************************************************************/

enum {
    NC_RELIABLE,            /* ordered, acked, retransmitted datagrams   */
    NC_UNRELIABLE,          /* fire-and-forget datagrams                 */
};

enum {
    NC_STATE_CLOSED,
    NC_STATE_CONNECTING,
    NC_STATE_CONNECTED,
    NC_STATE_CLOSING,
};

enum {
    NC_EV_NONE,
    NC_EV_CONNECTED,
    NC_EV_DISCONNECTED,
    NC_EV_DATA,             /* a channel has a datagram ready to read    */
    NC_EV_REDIRECT,         /* lobby handed us off to a game host        */
};

enum {
    NC_OK         =  0,
    NC_ERR        = -1,
    NC_ERR_NOMEM  = -2,
    NC_ERR_AGAIN  = -3,
    NC_ERR_CLOSED = -4,
    NC_ERR_TOOBIG = -5,
    NC_ERR_PROTO  = -6,
};

/****************************************************************
 * Types
 ****************************************************************/

struct netchan;
struct nc_chan;

struct nc_event {
    int type;
    struct nc_chan *ch;     /* channel for NC_EV_DATA                    */
    struct nc_addr redirect_addr;
    uint32_t redirect_id;
};

/****************************************************************
 * Connection lifecycle
 *
 * The application owns the transport. It feeds received packets in
 * with nc_feed() and pulls packets to transmit out with nc_send_next().
 * This keeps the core transport-agnostic: IPX on DOS, UDP on a host.
 *
 * A connection is one peer. A server keeps an array of connections and
 * routes inbound datagrams with nc_peek_id(): a header id of 0 is a new
 * connection attempt (feed it to a fresh server connection), otherwise
 * route to the connection whose id matches.
 ****************************************************************/

struct netchan *nc_open(int is_server);
void nc_close(struct netchan *c);
int nc_state(struct netchan *c);
uint32_t nc_id(struct netchan *c);

/** Begin a client handshake toward addr. */
int nc_connect(struct netchan *c, const struct nc_addr *addr);

/** Read the connection id from a raw datagram for server-side routing.
 *  Returns 0 for a new-connection (SYN) datagram. */
uint32_t nc_peek_id(const void *pkt, size_t len);

/** Feed one received datagram (from address) into the connection. */
int nc_feed(struct netchan *c, const void *pkt, size_t len,
            const struct nc_addr *from);

/** Fill buf with the next packet to transmit and its destination.
 *  Returns byte count, or 0 when nothing is pending. */
size_t nc_send_next(struct netchan *c, void *buf, size_t buflen,
                    struct nc_addr *to);

/** Service timers (retransmit, keepalive, timeout). now_ms is a free
 *  running millisecond clock. Returns ms until the next call is needed,
 *  or -1 when idle. */
int nc_service(struct netchan *c, uint32_t now_ms);

/****************************************************************
 * Channels
 *
 * Both peers must open the same channel layout in the same order: the
 * channel id is the open order (0, 1, ...). There is no negotiation.
 ****************************************************************/

struct nc_chan *nc_chan_open(struct netchan *c, int type);
void nc_chan_close(struct nc_chan *ch);
int nc_chan_id(struct nc_chan *ch);

/** Queue one datagram for sending. Returns bytes queued or an NC_ERR_*. */
int nc_write(struct nc_chan *ch, const void *data, size_t len);

/** Read the next received datagram. Returns bytes read, 0 if none. */
int nc_read(struct nc_chan *ch, void *buf, size_t buflen);

/****************************************************************
 * Events -- drain after nc_feed()
 ****************************************************************/

int nc_poll(struct netchan *c, struct nc_event *ev);

#endif /* NETCHAN_H */
