/* Generated from proto.idl - do not edit */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#ifndef PROTO_H
#define PROTO_H

#include "microser.h"

struct player_input {
    uint32_t seq;
    uint8_t input;
};

int player_input_encode(const struct player_input *msg, uint8_t *buf, int len);
int player_input_decode(struct player_input *msg, const uint8_t *buf, int len);

struct welcome {
    uint16_t your_player;
    uint16_t seed;
};

int welcome_encode(const struct welcome *msg, uint8_t *buf, int len);
int welcome_decode(struct welcome *msg, const uint8_t *buf, int len);

struct snapshot {
    uint32_t tick;
    const uint8_t *state;
    uint16_t state_len;
};

int snapshot_encode(const struct snapshot *msg, uint8_t *buf, int len);
int snapshot_decode(struct snapshot *msg, const uint8_t *buf, int len);

#endif /* PROTO_H */
