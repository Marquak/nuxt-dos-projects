/* Stable drawing-only workload. BENCH_NEW selects the optimized source. */
#define main matrix_program_main
#define BENCH_NEW
#include "../MATRIX.C"
#undef main

static uint32_t crc_table[256];

static void init_crc(void) {
    unsigned i, bit;
    uint32_t value;
    for (i = 0; i < 256; i++) {
        value = i;
        for (bit = 0; bit < 8; bit++)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320UL : 0);
        crc_table[i] = value;
    }
}

/* Read all visible bytes of every plane, after the timed section. */
static uint32_t video_crc(void) {
    uint32_t a, total, crc = 0xffffffffUL, left;
    unsigned plane, i, n;
    unsigned char __far *v;
    total = (uint32_t)pitch * height;
    for (plane = 0; plane < (depth == 16 ? 4 : 1); plane++) {
        if (depth == 16) outpw(0x3ce, (plane << 8) | 4);
        for (a = 0; a < total; a += n) {
            left = (uint32_t)span_k * 1024UL;
            left -= a % left;
            if (left > total - a) left = total - a;
            n = left > 16384 ? 16384 : (unsigned)left;
            v = address(a);
            for (i = 0; i < n; i++)
                crc = (crc >> 8) ^ crc_table[(crc ^ v[i]) & 255];
        }
    }
    if (depth == 16) outpw(0x3ce, 4);
    return crc ^ 0xffffffffUL;
}

#ifndef BENCH_LIBRARY
int main(void) {
    static const unsigned colors[5] = {0, 2, 6, 11, 15};
    FILE *out;
    int color, pass, x, y;
    unsigned start, elapsed;
    uint32_t crc;
    out = fopen("DRAW.TXT", "w");
    if (!out) return 1;
    init_crc();
    fprintf(out, "Drawing benchmark; 640x480; 32000 glyphs per mode; BIOS tick timing\n");
    for (color = 0; color < 2; color++) {
        if (!startmode(0, color)) {
            fprintf(out, "%u colors unavailable\n", color ? 256 : 16);
            continue;
        }
        palette();
#ifdef BENCH_NEW
        renderer_init();
#endif
        start = ticks();
        while (ticks() == start) { }
        start = ticks();
        for (pass = 0; pass < 10; pass++) {
            for (y = 0; y < 40; y++)
                for (x = 0; x < 80; x++)
                    draw(x, y, 33 + ((x + y * 3 + pass * 7) % 94),
                         colors[(x + y + pass) % 5]);
#ifdef BENCH_NEW
            flush_draw();
#endif
        }
        elapsed = ticks() - start;
        crc = video_crc();
        biosmode(3);
        fprintf(out, "%u colors: %u BIOS ticks, VRAM CRC32=%08lx\n", color ? 256 : 16, elapsed, crc);
        fflush(out);
    }
    fclose(out);
    return 0;
}
#endif
