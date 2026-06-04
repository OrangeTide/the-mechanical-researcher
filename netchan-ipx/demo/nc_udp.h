/* nc_udp.h : host (POSIX) UDP transport for netchan development */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#ifndef NC_UDP_H
#define NC_UDP_H

#include <stddef.h>
#include <stdint.h>
#include "nc_addr.h"

struct nc_udp {
    int fd;
};

/** Open a non-blocking UDP socket. bind_ip may be NULL (any); port 0 picks
 *  an ephemeral port. Returns NC-style 0 on success, -1 on failure. */
int nc_udp_open(struct nc_udp *u, const char *bind_ip, uint16_t port);

void nc_udp_close(struct nc_udp *u);

/** Receive one datagram. Returns length, 0 if none pending, -1 on error. */
int nc_udp_recv(struct nc_udp *u, void *buf, size_t buflen,
                struct nc_addr *from);

/** Send one datagram. Returns bytes sent or -1. */
int nc_udp_send(struct nc_udp *u, const void *buf, size_t len,
                const struct nc_addr *to);

/** Pack a dotted-quad host and port into an nc_addr. Returns 0 or -1. */
int nc_udp_addr(const char *ip, uint16_t port, struct nc_addr *out);

/** The local address actually bound (after an ephemeral bind). */
int nc_udp_local(struct nc_udp *u, struct nc_addr *out);

#endif /* NC_UDP_H */
