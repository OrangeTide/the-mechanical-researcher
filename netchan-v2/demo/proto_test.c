/* proto_test.c : round-trip test for the generated game wire messages */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#include "proto.h"
#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(cond) \
    do { if (!(cond)) { printf("FAIL: %s (line %d)\n", #cond, __LINE__); fails++; } } while (0)

static void
test_player_input(void)
{
    struct player_input in = { .seq = 0xdeadbeef, .buttons = 0x15, .aim = 6 };
    uint8_t buf[64];
    int n = player_input_encode(&in, buf, sizeof(buf));
    CHECK(n > 0);

    struct player_input out;
    memset(&out, 0, sizeof(out));
    int r = player_input_decode(&out, buf, n);
    CHECK(r > 0);
    CHECK(out.seq == in.seq);
    CHECK(out.buttons == in.buttons);
    CHECK(out.aim == in.aim);
}

static void
test_entity_state(void)
{
    struct entity_state in = {
        .tick = 12345, .id = 7, .kind = ENTITY_KIND_PLAYER,
        .x = -320, .y = 480, .hp = 100
    };
    uint8_t buf[64];
    int n = entity_state_encode(&in, buf, sizeof(buf));
    CHECK(n > 0);

    struct entity_state out;
    memset(&out, 0, sizeof(out));
    int r = entity_state_decode(&out, buf, n);
    CHECK(r > 0);
    CHECK(out.tick == in.tick);
    CHECK(out.id == in.id);
    CHECK(out.kind == in.kind);
    CHECK(out.x == in.x);      /* signed, negative */
    CHECK(out.y == in.y);
    CHECK(out.hp == in.hp);
}

static void
test_welcome(void)
{
    const char *level = "caves-of-thor";
    struct welcome in = {
        .your_id = 3, .max_players = 4,
        .level = level, .level_len = (uint16_t)strlen(level)
    };
    uint8_t buf[128];
    int n = welcome_encode(&in, buf, sizeof(buf));
    CHECK(n > 0);

    struct welcome out;
    memset(&out, 0, sizeof(out));
    int r = welcome_decode(&out, buf, n);
    CHECK(r > 0);
    CHECK(out.your_id == in.your_id);
    CHECK(out.max_players == in.max_players);
    CHECK(out.level_len == in.level_len);
    CHECK(memcmp(out.level, level, out.level_len) == 0);  /* zero-copy pointer */
}

int
main(void)
{
    test_player_input();
    test_entity_state();
    test_welcome();
    if (fails == 0)
        printf("proto round-trip OK (player_input, entity_state, welcome)\n");
    return fails ? 1 : 0;
}
