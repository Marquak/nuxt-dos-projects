/* Pixel equivalence across every offered mode, not a performance test. */
#define BENCH_LIBRARY
#include "BENCH.C"

int main(void) {
    static const unsigned colors[5] = {0, 2, 6, 11, 15};
    FILE *out = fopen("CHECK.TXT", "w");
    int res, color, x, y;
    uint32_t crc;
    if (!out) return 1;
    init_crc();
    for (res = 0; res < 3; res++) {
        for (color = 0; color < 2; color++) {
            if (!startmode(res, color)) {
                fprintf(out, "%dx%d/%d unavailable\n", width, height, depth);
                continue;
            }
            palette();
#ifdef BENCH_NEW
            renderer_init();
#endif
            for (y = 0; y < rows; y++) {
                for (x = 0; x < cols; x++) {
                    if (!(x % 7)) draw(x, y, 45, 0);
                    draw(x, y, 33 + ((x * 3 + y * 7) % 94), colors[(x + y) % 5]);
                    if (!((x + y) % 11)) {
                        draw(x, y, 90, 15);
                        draw(x, y, 90, 15);
                    }
                }
            }
            draw(0, -1, 65, 15);
            draw(0, rows, 65, 15);
#ifdef BENCH_NEW
            flush_draw();
#endif
            crc = video_crc();
            biosmode(3);
            fprintf(out, "%dx%d/%d CRC32=%08lx\n", width, height, depth, crc);
            fflush(out);
        }
    }
    fclose(out);
    return 0;
}
