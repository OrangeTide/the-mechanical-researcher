/* nc_ipx.h : 16-bit MS-DOS IPX transport for netchan (polling, no ESR) */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#ifndef NC_IPX_H
#define NC_IPX_H

#include <stddef.h>
#include <stdint.h>
#include "nc_addr.h"
#include "netchan.h"        /* for NC_MTU */

/*
 * Reaches the real-mode IPX driver through the standard far-call entry
 * point obtained with INT 2Fh AX=7A00h (Novell IPX/SPX, also emulated by
 * DOSBox). Receive uses a pool of pre-posted "listen" ECBs that the caller
 * polls; there is no Event Service Routine.
 *
 * All ECBs and packet buffers live in one conventional-memory block so the
 * driver can address them by real-mode segment:offset. nc_addr packs an IPX
 * address as network(4) + node(6) + socket(2), big-endian, len = 12.
 */

/* Tunables: pool depths. Each buffer is 30 (IPX header) + NC_MTU bytes. */
#ifndef NC_IPX_RECV
#define NC_IPX_RECV 6
#endif
#ifndef NC_IPX_SEND
#define NC_IPX_SEND 4
#endif

struct nc_ipx {
    uint16_t socket;        /* our socket number (host order)              */
    uint8_t  net[4];        /* local network number                        */
    uint8_t  node[6];       /* local node (MAC) address                    */
    uint16_t next_recv;     /* round-robin poll cursor                     */
};

/** Probe for the IPX driver. Returns 1 if present, 0 otherwise. */
int nc_ipx_available(void);

/** Open the socket and post the listen pool. Returns 0 on success, -1 on
 *  failure (no driver, out of memory, or socket open error). */
int nc_ipx_open(struct nc_ipx *x, unsigned socket);

void nc_ipx_close(struct nc_ipx *x);

/** Poll for one received datagram. Returns length, 0 if none, -1 on error.
 *  The IPX payload (after the 30-byte header) is copied into buf. */
int nc_ipx_recv(struct nc_ipx *x, void *buf, size_t buflen,
                struct nc_addr *from);

/** Send one datagram to an IPX address. Returns bytes sent, or -1/0. */
int nc_ipx_send(struct nc_ipx *x, const void *buf, size_t len,
                const struct nc_addr *to);

/** Our own IPX address on the given socket. */
void nc_ipx_local(struct nc_ipx *x, struct nc_addr *out);

/** The broadcast address (node FF:FF:FF:FF:FF:FF) on the given socket. */
void nc_ipx_broadcast(struct nc_ipx *x, struct nc_addr *out);

#endif /* NC_IPX_H */
