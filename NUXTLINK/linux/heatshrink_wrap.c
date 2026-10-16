/* Independent upload blocks. Upstream encoder license is bundled alongside. */
#include "heatshrink_encoder.h"

size_t nx_heatshrink_encode(uint8_t *src, size_t size, uint8_t *out, size_t capacity) {
    heatshrink_encoder *h;
    size_t used=0, sunk=0, count;
    HSE_poll_res poll;
    if (!size || size>4096 || capacity<4608) return 0;
    h=heatshrink_encoder_alloc(12,4);
    if (!h) return 0;
    if (heatshrink_encoder_sink(h,src,size,&sunk)!=HSER_SINK_OK || sunk!=size) goto fail;
    while (heatshrink_encoder_finish(h)!=HSER_FINISH_DONE) {
        do {
            if (used==capacity) goto fail;
            poll=heatshrink_encoder_poll(h,out+used,capacity-used,&count);
            if (poll<0) goto fail;
            used+=count;
        } while (poll==HSER_POLL_MORE);
    }
    heatshrink_encoder_free(h);
    return used;
fail:
    heatshrink_encoder_free(h);
    return 0;
}
