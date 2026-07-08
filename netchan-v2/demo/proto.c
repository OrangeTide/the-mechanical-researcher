/* Generated from proto.idl - do not edit */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#include "proto.h"
#include <string.h>

int player_input_encode(const struct player_input *msg, uint8_t *buf, int len)
{
    int pos = 2;

    pos = ms_write_tag_u32(buf, pos, len, 1, msg->seq);
    if (pos < 0) return -1;
    pos = ms_write_tag_u8(buf, pos, len, 2, msg->input);
    if (pos < 0) return -1;

    buf[0] = (uint8_t)((pos - 2) & 0xff);
    buf[1] = (uint8_t)(((pos - 2) >> 8) & 0xff);
    return pos;
}

int player_input_decode(struct player_input *msg, const uint8_t *buf, int len)
{
    int end, pos = 2;

    if (len < 2) return -1;
    end = (int)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8)) + 2;
    if (end > len) return -1;
    memset(msg, 0, sizeof(*msg));

    while (pos < end) {
        uint8_t tag = buf[pos++];
        switch (tag >> 3) {
        case 1:
            pos = ms_read_u32(buf, pos, end, &msg->seq);
            break;
        case 2:
            pos = ms_read_u8(buf, pos, end, &msg->input);
            break;
        default:
            pos = ms_skip(buf, pos, end, tag & 7);
            break;
        }
        if (pos < 0) return -1;
    }
    return end;
}

int welcome_encode(const struct welcome *msg, uint8_t *buf, int len)
{
    int pos = 2;

    pos = ms_write_tag_u16(buf, pos, len, 1, msg->your_player);
    if (pos < 0) return -1;
    pos = ms_write_tag_u16(buf, pos, len, 2, msg->seed);
    if (pos < 0) return -1;

    buf[0] = (uint8_t)((pos - 2) & 0xff);
    buf[1] = (uint8_t)(((pos - 2) >> 8) & 0xff);
    return pos;
}

int welcome_decode(struct welcome *msg, const uint8_t *buf, int len)
{
    int end, pos = 2;

    if (len < 2) return -1;
    end = (int)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8)) + 2;
    if (end > len) return -1;
    memset(msg, 0, sizeof(*msg));

    while (pos < end) {
        uint8_t tag = buf[pos++];
        switch (tag >> 3) {
        case 1:
            pos = ms_read_u16(buf, pos, end, &msg->your_player);
            break;
        case 2:
            pos = ms_read_u16(buf, pos, end, &msg->seed);
            break;
        default:
            pos = ms_skip(buf, pos, end, tag & 7);
            break;
        }
        if (pos < 0) return -1;
    }
    return end;
}

int snapshot_encode(const struct snapshot *msg, uint8_t *buf, int len)
{
    int pos = 2;

    pos = ms_write_tag_u32(buf, pos, len, 1, msg->tick);
    if (pos < 0) return -1;
    pos = ms_write_tag_bytes(buf, pos, len, 2,
        msg->state, msg->state_len);
    if (pos < 0) return -1;

    buf[0] = (uint8_t)((pos - 2) & 0xff);
    buf[1] = (uint8_t)(((pos - 2) >> 8) & 0xff);
    return pos;
}

int snapshot_decode(struct snapshot *msg, const uint8_t *buf, int len)
{
    int end, pos = 2;

    if (len < 2) return -1;
    end = (int)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8)) + 2;
    if (end > len) return -1;
    memset(msg, 0, sizeof(*msg));

    while (pos < end) {
        uint8_t tag = buf[pos++];
        switch (tag >> 3) {
        case 1:
            pos = ms_read_u32(buf, pos, end, &msg->tick);
            break;
        case 2:
            pos = ms_read_bytes(buf, pos, end,
                &msg->state, 65535, &msg->state_len);
            break;
        default:
            pos = ms_skip(buf, pos, end, tag & 7);
            break;
        }
        if (pos < 0) return -1;
    }
    return end;
}

