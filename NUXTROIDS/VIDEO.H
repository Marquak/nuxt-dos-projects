/* 16-colour VGA plus the NuXT's native Trident mode 5Bh. */
static unsigned char info[256];
static unsigned wordat(int i) {
    return info[i] | ((unsigned)info[i + 1] << 8);
}
static void biosmode(unsigned m) {
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = m;
    int86(0x10, &r, &r);
}
static int trident(void) {
    unsigned i;
    const char __far *p = MK_FP(0xc000, 0);
    for (i = 0; i < 32760; i++)
        if (!_fmemcmp(p + i, "Trident", 7) || !_fmemcmp(p + i, "TRIDENT", 7))
            return 1;
    return 0;
}
static int video_start(int mode) {
    union REGS r;
    struct SREGS sr;
    int i, win;
    unsigned flags;
    width = mode == 0 ? 320 : mode == 1 ? 640 : 800;
    height = mode == 0 ? 200 : mode == 1 ? 480 : 600;
    scale = mode == 0 ? 1 : 2;
    hud = 16 * scale;
    worldw = width / scale;
    worldh = (height - hud) / scale;
    if (mode < 2)
        biosmode(mode ? 0x12 : 0x0d);
    else {
        biosmode(0x12);
        memset(&r, 0, sizeof(r));
        segread(&sr);
        r.w.ax = 0x4f01;
        r.w.cx = 0x102;
        r.w.di = FP_OFF(info);
        sr.es = FP_SEG(info);
        int86x(0x10, &r, &r, &sr);
        win = (info[2] & 7) == 7 ? 0 : (info[3] & 7) == 7 ? 1 : -1;
        if (r.w.ax == 0x004f && (wordat(0) & 0x79) == 0x19 && wordat(18) == 800 &&
            wordat(20) == 600 && info[24] == 4 && info[25] == 4 && info[27] == 3 &&
            wordat(16) == 100 && win >= 0 && wordat(win ? 10 : 8) == 0xa000 && wordat(6) >= 64 &&
            wordat(4) > 0) {
            memset(&r, 0, sizeof(r));
            r.w.ax = 0x4f02;
            r.w.bx = 0x102;
            int86(0x10, &r, &r);
            if (r.w.ax != 0x004f)
                return 0;
            r.w.ax = 0x4f05;
            r.w.bx = win;
            r.w.dx = 0;
            int86(0x10, &r, &r);
            if (r.w.ax != 0x004f)
                return 0;
        } else {
            if (!trident())
                return 0;
            biosmode(0x5b);
            memset(&r, 0, sizeof(r));
            r.h.ah = 15;
            int86(0x10, &r, &r);
            if ((r.h.al & 127) != 0x5b)
                return 0;
            outp(0x3c4, 0x0b);
            inp(0x3c5);
            outp(0x3c4, 0x0e);
            flags = inp(0x3c5) & 0xf0;
            outpw(0x3c4, ((flags | 2) << 8) | 0x0e);
        }
    }
    for (i = 0; i < 16; i++) {
        memset(&r, 0, sizeof(r));
        r.w.ax = 0x1000;
        r.h.bl = i;
        r.h.bh = i;
        int86(0x10, &r, &r);
    }
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x1013;
    int86(0x10, &r, &r);
    r.w.ax = 0x1013;
    r.h.bl = 1;
    r.h.bh = 0;
    int86(0x10, &r, &r);
    outp(0x3d4, 0x14);
    outp(0x3d5, inp(0x3d5) & ~0x40);
    outp(0x3d4, 0x17);
    outp(0x3d5, inp(0x3d5) | 0x40);
    outpw(0x3c4, 0x0604);
    outpw(0x3ce, 0x0506);
    outpw(0x3ce, 0x0000);
    outpw(0x3ce, 0x0001);
    outpw(0x3ce, 0x0003);
    outpw(0x3ce, 0x0005);
    outpw(0x3ce, 0xff08);
    outpw(0x3c4, 0x0f02);
    {
        volatile unsigned char __far *v = MK_FP(0xa000, 256);
        unsigned c, p, want;
        for (c = 0; c < 16; c++) {
            outpw(0x3ce, (((~c) & 15) << 8) | 1);
            v[0] = 0xa5;
            v[1] = 0x5a;
            for (p = 0; p < 4; p++) {
                outpw(0x3ce, (p << 8) | 4);
                want = (c & (1U << p)) ? 0xa5 : 0;
                if (v[0] != want || v[1] != (want ? 0x5a : 0))
                    return 0;
            }
        }
        outpw(0x3ce, 0x0004);
        outpw(0x3ce, 0x0001);
    }
    outp(0x3c6, 255);
    outp(0x3c8, 0);
    for (i = 0; i < 256; i++) {
        unsigned char c = i < 16 ? i * 63 / 15 : 0;
        outp(0x3c9, c);
        outp(0x3c9, c);
        outp(0x3c9, c);
    }
    video_init(width / 8, height);
    wrap_init(hud);
    wipe();
    return 1;
}
