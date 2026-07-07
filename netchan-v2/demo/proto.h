/* Generated from proto.idl - do not edit */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#ifndef PROTO_H
#define PROTO_H

#include "microser.h"

typedef uint8_t entity_kind_t;
#define ENTITY_KIND_PLAYER 1
#define ENTITY_KIND_SHOT 2
#define ENTITY_KIND_ROCK 3

struct player_input {
    uint32_t seq;
    uint8_t buttons;
    uint8_t aim;
};

int player_input_encode(const struct player_input *msg, uint8_t *buf, int len);
int player_input_decode(struct player_input *msg, const uint8_t *buf, int len);

struct entity_state {
    uint32_t tick;
    uint16_t id;
    uint8_t kind;
    int16_t x;
    int16_t y;
    uint8_t hp;
};

int entity_state_encode(const struct entity_state *msg, uint8_t *buf, int len);
int entity_state_decode(struct entity_state *msg, const uint8_t *buf, int len);

struct welcome {
    uint16_t your_id;
    uint8_t max_players;
    const char *level;
    uint16_t level_len;
};

int welcome_encode(const struct welcome *msg, uint8_t *buf, int len);
int welcome_decode(struct welcome *msg, const uint8_t *buf, int len);

#endif /* PROTO_H */
