/* game_net.h : netchan wire protocol and peer glue for the game */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

/*
 * Two netchan channels, opened by both peers in the same order:
 *   channel 0 (reliable)   welcome, map stream, chat, join/leave
 *   channel 1 (unreliable) client input, server state broadcast
 *
 * The server keeps one netchan connection per client and routes inbound
 * datagrams with nc_peek_id(). The application owns the transport (UDP on
 * the host, IPX on DOS): it feeds received datagrams in and pulls datagrams
 * to transmit out, so this layer never touches a socket.
 */

#ifndef GAME_NET_H
#define GAME_NET_H

#include <stdint.h>
#include <stddef.h>
#include "game.h"
#include "netchan.h"
#include "nc_addr.h"

#define GAME_TICK_MS 125        /* 8 Hz simulation/broadcast */

/* channel message types (first byte of every channel message) */
enum {
    GM_WELCOME = 1,             /* S->C reliable: assigned player index    */
    GM_MAP,                     /* S->C reliable: a map chunk              */
    GM_MAPEND,                  /* S->C reliable: map transfer complete    */
    GM_CHAT,                    /* both  reliable: a line of chat          */
    GM_INPUT,                   /* C->S unreliable: one input byte         */
    GM_STATE,                   /* S->C unreliable: full world state       */
};

#define CHAT_MAX 40

/****************************************************************
 * Server
 ****************************************************************/

struct gconn {
    struct netchan *nc;
    struct nc_addr addr;
    struct nc_chan *rel;
    struct nc_chan *unr;
    int player;                 /* player index, or -1                     */
    uint16_t map_off;           /* bytes of map streamed so far            */
    uint8_t used;
    uint8_t mapped;             /* map transfer finished                   */
};

struct gserver {
    struct gconn conn[MAX_PLAYERS];
    struct world world;
    uint32_t last_tick_ms;
    void (*on_chat)(int who, const char *text);   /* optional */
};

void gserver_init(struct gserver *s, uint16_t seed);
void gserver_feed(struct gserver *s, const void *pkt, size_t len,
                  const struct nc_addr *from);
void gserver_service(struct gserver *s, uint32_t now_ms);
size_t gserver_pull(struct gserver *s, void *buf, size_t buflen,
                    struct nc_addr *to);

/** Send a chat line as the host (who = host player index): relay it to all
 *  clients and fire the local on_chat callback. */
void gserver_say(struct gserver *s, int who, const char *text);

/****************************************************************
 * Client
 ****************************************************************/

struct gclient {
    struct netchan *nc;
    struct nc_chan *rel;
    struct nc_chan *unr;
    struct world world;         /* tiles from the map stream, entities from
                                 * the state broadcast (for rendering)      */
    int player;                 /* our player index, or -1 until welcomed   */
    uint16_t map_off;           /* bytes of map received                    */
    uint8_t have_map;
    void (*on_chat)(int who, const char *text);   /* optional */
};

void gclient_init(struct gclient *c);
int gclient_connect(struct gclient *c, const struct nc_addr *server);
void gclient_feed(struct gclient *c, const void *pkt, size_t len,
                  const struct nc_addr *from);
void gclient_service(struct gclient *c, uint32_t now_ms);
size_t gclient_pull(struct gclient *c, void *buf, size_t buflen,
                    struct nc_addr *to);
void gclient_input(struct gclient *c, uint8_t input);
void gclient_chat(struct gclient *c, const char *text);

#endif /* GAME_NET_H */
