/* OPL2 effects and a short original four-line fanfare. Timer-driven, never
 * busy-wait for a note. PIT channel 2 handles the optional PC speaker. */
static int audiofound, tune, tunepos, tunewait, toneleft;
static unsigned char opllife[3], speakerbits;
static const unsigned fnums[12] = {343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647};
static const unsigned divisors[12] = {4560, 4304, 4063, 3835, 3620, 3417,
                                      3225, 3044, 2873, 2712, 2560, 2416};
static void opl(unsigned reg, unsigned val) {
    int i;
    outp(0x388, reg);
    for (i = 0; i < 6; ++i)
        inp(0x388);
    outp(0x389, val);
    for (i = 0; i < 35; ++i)
        inp(0x388);
}
static int adlib(void) {
    unsigned a, b;
    int i;
    opl(4, 0x60);
    opl(4, 0x80);
    a = inp(0x388);
    opl(2, 255);
    opl(4, 0x21);
    for (i = 0; i < 240; ++i)
        inp(0x388);
    b = inp(0x388);
    opl(4, 0x60);
    opl(4, 0x80);
    return !(a & 0xe0) && (b & 0xe0) == 0xc0;
}
static void speaker_off(void) {
    outp(0x61, inp(0x61) & 0xfc);
    toneleft = 0;
}
static void silence(void) {
    int i;
    if (audiofound)
        for (i = 0; i < 3; ++i)
            opl(0xb0 + i, 0);
    memset(opllife, 0, sizeof(opllife));
    speaker_off();
    tune = 0;
}
static void instruments(void) {
    int i;
    opl(1, 32);
    opl(0xbd, 0);
    for (i = 0; i < 3; ++i) {
        opl(0x20 + i, 0x21);
        opl(0x23 + i, 0x21);
        opl(0x40 + i, i == 0 ? 26 : 36);
        /* Raise carrier output four attenuation steps; leave modulation,
           envelopes and PC speaker timing/pitch unchanged. */
        opl(0x43 + i, i ? 10 : 1);
        opl(0x60 + i, 0xf3);
        opl(0x63 + i, 0xf3);
        opl(0x80 + i, 0x65);
        opl(0x83 + i, 0x65);
        opl(0xe0 + i, 1);
        opl(0xe3 + i, 0);
        opl(0xc0 + i, 4);
    }
}
static void note(int ch, int pitch, int duration) {
    unsigned f = fnums[pitch % 12], d;
    int oct = pitch / 12;
    if (audiofound && (soundmode & 1)) {
        opl(0xb0 + ch, 0);
        opl(0xa0 + ch, f & 255);
        opl(0xb0 + ch, 32 | ((3 + oct) << 2) | (f >> 8));
        opllife[ch] = duration;
    }
    if (ch == 0 && (soundmode & 2)) {
        d = divisors[pitch % 12] >> oct;
        outp(0x43, 0xb6);
        outp(0x42, d & 255);
        outp(0x42, d >> 8);
        outp(0x61, inp(0x61) | 3);
        toneleft = duration;
    }
}
static void sound_event(int which) {
    silence();
    if (which == 1)
        note(0, 7, 2); /* landing */
    else {
        tune = which;
        tunepos = tunewait = 0;
    }
}
static void audio_tick(void) {
    static const unsigned char fanfare[10] = {12, 16, 19, 24, 19, 21, 23, 26, 28, 24};
    static const unsigned char clear[4] = {12, 16, 19, 24};
    int i, p, d;
    for (i = 0; i < 3; ++i)
        if (opllife[i] && !--opllife[i])
            opl(0xb0 + i, 0);
    if (toneleft && !--toneleft)
        speaker_off();
    if (!tune)
        return;
    if (tunewait) {
        --tunewait;
        return;
    }
    if (tunepos >= (tune == 4 ? 10 : 4)) {
        tune = 0;
        return;
    }
    p = tune == 4 ? fanfare[tunepos] : clear[tunepos];
    d = tune == 4 ? 3 : 2;
    if (tune == 4 && tunepos == 9)
        d = 10;
    note(0, p, d);
    if (tune == 4 && (tunepos == 0 || tunepos == 3 || tunepos == 9)) {
        note(1, 0, 10);
        note(2, tunepos == 9 ? 7 : 4, 10);
    }
    tunewait = d;
    ++tunepos;
}
