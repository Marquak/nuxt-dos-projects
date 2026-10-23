/* Mateusz Viste's bsum 1.1 BSD-checksum recurrence for Slink-XT.
 * Source: bsum11.zip, Copyright (C) 2017 Mateusz Viste, MIT license.
 */
#include <stddef.h>
#include <stdint.h>

uint16_t slx_bsum(const unsigned char *data, size_t length, uint16_t sum)
{
    size_t i;
    for (i = 0; i < length; ++i) {
        sum = (uint16_t)((sum >> 1) | (sum << 15));
        sum = (uint16_t)(sum + data[i]);
    }
    return sum;
}
