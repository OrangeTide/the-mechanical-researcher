/* Generated from proto.idl - do not edit */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#include "proto.h"
#include <string.h>

int player_input_encode(const struct player_input *msg, uint8_t *buf, int len)
{
    int pos = 2;

    pos = ms_write_tag_u32(buf, pos, len, 1, msg->seq);
    if (pos < 0) return -1;
    pos = ms_write_tag_u8(buf, pos, len, 2, msg->buttons);
    if (pos < 0) return -1;
    pos = ms_write_tag_u8(buf, pos, len, 3, msg->aim);
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
            pos = ms_read_u8(buf, pos, end, &msg->buttons);
            break;
        case 3:
            pos = ms_read_u8(buf, pos, end, &msg->aim);
            break;
        default:
            pos = ms_skip(buf, pos, end, tag & 7);
            break;
        }
        if (pos < 0) return -1;
    }
    return end;
}

int entity_state_encode(const struct entity_state *msg, uint8_t *buf, int len)
{
    int pos = 2;

    pos = ms_write_tag_u32(buf, pos, len, 1, msg->tick);
    if (pos < 0) return -1;
    pos = ms_write_tag_u16(buf, pos, len, 2, msg->id);
    if (pos < 0) return -1;
    pos = ms_write_tag_u8(buf, pos, len, 3, msg->kind);
    if (pos < 0) return -1;
    pos = ms_write_tag_i16(buf, pos, len, 4, msg->x);
    if (pos < 0) return -1;
    pos = ms_write_tag_i16(buf, pos, len, 5, msg->y);
    if (pos < 0) return -1;
    pos = ms_write_tag_u8(buf, pos, len, 6, msg->hp);
    if (pos < 0) return -1;

    buf[0] = (uint8_t)((pos - 2) & 0xff);
    buf[1] = (uint8_t)(((pos - 2) >> 8) & 0xff);
    return pos;
}

int entity_state_decode(struct entity_state *msg, const uint8_t *buf, int len)
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
            pos = ms_read_u16(buf, pos, end, &msg->id);
            break;
        case 3:
            pos = ms_read_u8(buf, pos, end, &msg->kind);
            break;
        case 4:
            pos = ms_read_i16(buf, pos, end, &msg->x);
            break;
        case 5:
            pos = ms_read_i16(buf, pos, end, &msg->y);
            break;
        case 6:
            pos = ms_read_u8(buf, pos, end, &msg->hp);
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

    pos = ms_write_tag_u16(buf, pos, len, 1, msg->your_id);
    if (pos < 0) return -1;
    pos = ms_write_tag_u8(buf, pos, len, 2, msg->max_players);
    if (pos < 0) return -1;
    pos = ms_write_tag_bytes(buf, pos, len, 3,
        msg->level, msg->level_len);
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
            pos = ms_read_u16(buf, pos, end, &msg->your_id);
            break;
        case 2:
            pos = ms_read_u8(buf, pos, end, &msg->max_players);
            break;
        case 3:
            {
                const uint8_t *_tmp = 0;
                pos = ms_read_bytes(buf, pos, end,
                    &_tmp, 65535, &msg->level_len);
                msg->level = (const char *)_tmp;
            }
            break;
        default:
            pos = ms_skip(buf, pos, end, tag & 7);
            break;
        }
        if (pos < 0) return -1;
    }
    return end;
}

