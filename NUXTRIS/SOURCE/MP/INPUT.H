static volatile unsigned ticks;
static volatile unsigned char keys[128], makes[128];
static unsigned char timerhalf;
static void(__interrupt __far *oldtimer)(void), (__interrupt __far * oldkey)(void);
static void __interrupt __far timer_isr(void) {
    ++ticks;
    timerhalf ^= 1;
    if (!timerhalf)
        _chain_intr(oldtimer);
    else
        outp(0x20, 0x20);
}
static void __interrupt __far key_isr(void) {
    unsigned char k = inp(0x60), a;
    if (k != 0xe0 && k != 0xe1) {
        if (k & 128)
            keys[k & 127] = 0;
        else {
            if (!keys[k])
                makes[k] = 1;
            keys[k] = 1;
        }
    }
    /* Release the XT keyboard latch immediately before the PIC EOI, after
       storing the scan code. This matches the proven Invaders IRQ ordering. */
    a = inp(0x61);
    outp(0x61, a | 128);
    outp(0x61, a);
    outp(0x20, 0x20);
}
static void install(void) {
    oldtimer = _dos_getvect(8);
    oldkey = _dos_getvect(9);
    _dos_setvect(8, timer_isr);
    _dos_setvect(9, key_isr);
    _disable();
    outp(0x43, 0x34);
    outp(0x40, 0);
    outp(0x40, 128);
    _enable();
}
static void uninstall(void) {
    _disable();
    outp(0x43, 0x36);
    outp(0x40, 0);
    outp(0x40, 0);
    _enable();
    _dos_setvect(8, oldtimer);
    _dos_setvect(9, oldkey);
    *(unsigned __far *)MK_FP(0x40, 0x1a) = *(unsigned __far *)MK_FP(0x40, 0x1c);
}
static int edge(unsigned k) {
    int v;
    _disable();
    v = makes[k];
    makes[k] = 0;
    _enable();
    return v;
}
