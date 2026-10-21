/* Rules adapted from olzhasar/sdl-tetris, src/game.c (MIT).
 * Upstream commit 428d99d52a1de2448c7442d2655fae56aa6e9e96.
 * See UPSTREAM.TXT and CREDITS.TXT. DOS adaptation: byte board, 32-bit
 * row mask/score, preview, fixed ticks and separate lock/clear/spawn.
 */
#include <stdint.h>
#include <string.h>
#define BW 10
#define BH 20
typedef struct {
    uint8_t grid[BH][BW];
    int shape[8], x, y, kind, next, over;
    unsigned level, lines, startlevel, type, height;
    uint32_t score, full;
} Game;
static Game game;
static uint16_t rng = 0x7a31;
static uint8_t bag[7];
static unsigned bagpos = 7;
/* Four coordinate pairs about a pivot, as in the upstream core.
 * Spawn orientations adjusted to fit the usual two-row preview. */
static const int shapes[7][8] = {
    {0, 0, 1, 0, 0, 1, 1, 1},   /* O */
    {0, 0, -1, 0, 1, 0, 0, 1},  /* T */
    {-1, 0, 0, 0, 1, 0, 1, 1},  /* L */
    {-1, 0, 0, 0, 1, 0, -1, 1}, /* J */
    {-1, 0, 0, 0, 1, 0, 2, 0},  /* I */
    {0, 0, 1, 0, 0, 1, -1, 1},  /* S */
    {0, 0, -1, 0, 0, 1, 1, 1}   /* Z */
};
static uint16_t random16(void) {
    rng ^= (uint16_t)(rng << 7);
    rng ^= (uint16_t)(rng >> 9);
    rng ^= (uint16_t)(rng << 8);
    return rng;
}
static unsigned bounded(unsigned n) {
    unsigned v, limit = 65535U - 65535U % n;
    do {
        v = random16() - 1U;
    } while (v >= limit);
    return v % n;
}
/* Upstream Fisher-Yates seven-piece bag, with bounded random sampling. */
static int next_kind(void) {
    int i, j;
    uint8_t tmp;
    if (bagpos == 7) {
        for (i = 0; i < 7; ++i)
            bag[i] = (uint8_t)i;
        for (i = 6; i > 0; --i) {
            j = bounded(i + 1);
            tmp = bag[i];
            bag[i] = bag[j];
            bag[j] = tmp;
        }
        bagpos = 0;
    }
    return bag[bagpos++];
}
static int collision(int x, int y) {
    if (x < 0 || x >= BW || y >= BH)
        return 1;
    return y >= 0 && game.grid[y][x] != 0;
}
static int fits(const int *shape, int x, int y) {
    int i;
    for (i = 0; i < 4; ++i)
        if (collision(x + shape[i * 2], y + shape[i * 2 + 1]))
            return 0;
    return 1;
}
static void spawn(void) {
    game.kind = game.next;
    game.next = next_kind();
    memcpy(game.shape, shapes[game.kind], sizeof(game.shape));
    game.x = 4;
    game.y = 0;
    game.over = !fits(game.shape, game.x, game.y);
}
static int move_side(int direction) {
    if (!fits(game.shape, game.x + direction, game.y))
        return 0;
    game.x += direction;
    return 1;
}
static int rotate_shape(int clockwise) {
    int temp[8], i;
    if (game.kind == 0)
        return 1;
    for (i = 0; i < 4; ++i) {
        temp[i * 2] = clockwise ? -game.shape[i * 2 + 1] : game.shape[i * 2 + 1];
        temp[i * 2 + 1] = clockwise ? game.shape[i * 2] : -game.shape[i * 2];
    }
    if (!fits(temp, game.x, game.y))
        return 0;
    memcpy(game.shape, temp, sizeof(temp));
    return 1;
}
static void add_score(uint32_t n) {
    if (n > 999999999UL - game.score)
        game.score = 999999999UL;
    else
        game.score += n;
}
static int drop_one(int soft) {
    if (!fits(game.shape, game.x, game.y + 1))
        return 0;
    ++game.y;
    if (soft)
        add_score(1);
    return 1;
}
static int lock_shape(void) {
    int i, x, y, count = 0, full;
    game.full = 0;
    /* Validate all four cells before touching the grid, including lock-out
     * above the top. No negative indexing or partly committed pieces. */
    for (i = 0; i < 4; ++i) {
        x = game.x + game.shape[i * 2];
        y = game.y + game.shape[i * 2 + 1];
        if (y < 0 || collision(x, y)) {
            game.over = 1;
            return 0;
        }
    }
    for (i = 0; i < 4; ++i)
        game.grid[game.y + game.shape[i * 2 + 1]][game.x + game.shape[i * 2]] = game.kind + 1;
    for (y = 0; y < BH; ++y) {
        full = 1;
        for (x = 0; x < BW; ++x)
            if (!game.grid[y][x]) {
                full = 0;
                break;
            }
        if (full) {
            game.full |= 1UL << y;
            ++count;
        }
    }
    return count;
}
/* Row compaction from the upstream row-shift approach. Scan bottom-up so
 * adjacent lines cannot be skipped; one pass also handles separated rows. */
static int clear_rows(void) {
    static const unsigned points[5] = {0, 40, 100, 300, 1200};
    int r, to = BH - 1, count = 0;
    for (r = BH - 1; r >= 0; --r) {
        if (game.full & (1UL << r))
            ++count;
        else {
            if (to != r)
                memcpy(game.grid[to], game.grid[r], BW);
            --to;
        }
    }
    while (to >= 0)
        memset(game.grid[to--], 0, BW);
    if (count > 4)
        count = 4;
    add_score((uint32_t)points[count] * (game.level + 1));
    if (game.lines <= 65535U - count)
        game.lines += count;
    else
        game.lines = 65535U;
    if (!game.type) {
        game.level = game.startlevel + game.lines / 10;
        if (game.level > 19)
            game.level = 19;
    }
    game.full = 0;
    return count;
}
static unsigned fall_period(void) {
    static const uint8_t periods[20] = {30, 27, 24, 21, 18, 15, 12, 10, 8, 6,
                                        5,  4,  4,  3,  3,  3,  2,  2,  2, 2};
    return periods[game.level < 20 ? game.level : 19];
}
static void new_game(unsigned type, unsigned level, unsigned height) {
    int y, x, hole;
    memset(&game, 0, sizeof(game));
    game.type = type;
    game.level = game.startlevel = level;
    game.height = height;
    bagpos = 7;
    if (type) {
        for (y = BH - (int)height * 2; y < BH; ++y) {
            hole = bounded(BW);
            for (x = 0; x < BW; ++x)
                if (x != hole && bounded(4))
                    game.grid[y][x] = bounded(7) + 1;
        }
    }
    game.next = next_kind();
    spawn();
}
