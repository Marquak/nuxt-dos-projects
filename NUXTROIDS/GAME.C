/* NuXTroids 1.2. Original V20 DOS game. No game assets from Atari. */
#include "TABLES.H"
#include <conio.h>
#include <dos.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROCKS 28
#define SHOTS 6
#define SPARKS 16
static int width, height, scale, hud, worldw, worldh;
extern void video_init(int, int);
#pragma aux video_init parm[ax][bx] modify[ax bx cx dx]
extern void ink(int);
#pragma aux ink parm[ax] modify[ax dx]
extern void raster(int);
#pragma aux raster parm[ax] modify[ax dx]
extern void wipe(void);
#pragma aux wipe modify[ax cx dx di]
extern void dot(int, int);
#pragma aux dot parm[ax][bx] modify[ax bx cx di]
extern void segment(int, int, int, int);
#pragma aux segment parm[ax][bx][cx][dx] modify[ax bx cx dx si di]
extern void wrap_init(int);
#pragma aux wrap_init parm[ax] modify[ax bx]
extern void wrapped(int, int, int, int);
#pragma aux wrapped parm[ax][bx][cx][dx] modify[ax bx cx dx si di]
extern void outline(unsigned, const unsigned __far *, unsigned, unsigned);
#pragma aux outline parm[ax][dx si][cx][bx] modify[ax bx cx dx si di]
extern void hudpatch(unsigned, const unsigned *, unsigned);
#pragma aux hudpatch parm[di][si][cx] modify[ax bx cx dx si di]
extern unsigned probe87(unsigned *);
#pragma aux probe87 parm[si] value[ax] modify[ax]
extern void rotate87(int *);
#pragma aux rotate87 parm[si] modify exact[]
extern void done87(unsigned *);
#pragma aux done87 parm[si] modify exact[]
#include "VIDEO.H"
typedef struct {
    int x, y;
} Point;
typedef struct {
    int x, y, vx, vy;
    unsigned char on, size, variant, angle, spin, grace;
} Rock;
typedef struct {
    int x, y, vx, vy;
    unsigned char life;
} Shot;
typedef struct {
    int x, y, vx, vy;
    unsigned char life;
} Spark;
typedef struct {
    int x, y, shape, angle, color, on;
} Picture;
static Point shapes[9][32][8], shipshape[32][4], flame[32][3];
static Rock rocks[ROCKS];
static Shot shots[SHOTS];
static Spark sparks[SPARKS];
static Picture oldrocks[ROCKS], oldshots[SHOTS], oldsparks[SPARKS], oldship, oldflame;
static int shipx, shipy, shipvx, shipvy, heading, protect, dead, lives, wave, remaining;
static int started, gameover, paused, shootwait, shootqueue, levelwait, thrusting, framephase;
static int monochrome, nofpu, fpu, muted, audiofound, quit, redraw;
static unsigned long score, best, nextlife;
static unsigned rng = 0x7a31, heartbeat;
static unsigned long frames, updates, dropped;
static int joyenabled, joyleft, joyright, joyup, joydown, joyfire, joyedge, joyold, joyfail,
    joylaunch, resetclock;
static unsigned joyxmin, joyxmax, joyymin, joyymax;
extern unsigned joyread(unsigned *);
#pragma aux joyread parm[si] value[ax] modify[ax bx dx]
static char oldhud[2][80];
static const int radius[3] = {14, 8, 4};
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
static unsigned random16(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}
static int wrap(int n, int limit) {
    if (n < 0)
        n += limit;
    else if (n >= limit)
        n -= limit;
    return n;
}
static int delta(int a, int b, int limit) {
    int d = a - b;
    if (d > limit / 2)
        d -= limit;
    else if (d < -limit / 2)
        d += limit;
    return d;
}
static int nearxy(int ax, int ay, int bx, int by, int r) {
    int dx = abs(delta(ax >> 6, bx >> 6, worldw)), dy = abs(delta(ay >> 6, by >> 6, worldh));
    if (dx > r || dy > r)
        return 0;
    return dx * dx + dy * dy <= r * r;
}
/* All OPL writes keep the original AdLib bus delays. No audiofound work in IRQs. */
static unsigned char pitchhi[5], sfxleft[5];
static unsigned sfxpitch[5];
static void opl(unsigned reg, unsigned val) {
    int i;
    outp(0x388, reg);
    for (i = 0; i < 6; i++)
        inp(0x388);
    outp(0x389, val);
    for (i = 0; i < 35; i++)
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
    for (i = 0; i < 240; i++)
        inp(0x388);
    b = inp(0x388);
    opl(4, 0x60);
    opl(4, 0x80);
    return (a & 0xe0) == 0 && (b & 0xe0) == 0xc0;
}
static void silence(void) {
    int i;
    if (audiofound)
        for (i = 0; i < 9; i++)
            opl(0xb0 + i, 0);
    memset(sfxleft, 0, sizeof(sfxleft));
}
static void instruments(void) {
    static const unsigned char op[5] = {0, 1, 2, 8, 9};
    static const unsigned char mod[5] = {0x21, 0x01, 0x01, 0x01, 0x21};
    static const unsigned char mul[5] = {0x21, 0x01, 0x01, 0x01, 0x01};
    int i, m, c;
    opl(1, 32);
    opl(0xbd, 0);
    for (i = 0; i < 5; i++) {
        m = op[i];
        c = m + 3;
        opl(0x20 + m, mod[i]);
        opl(0x20 + c, mul[i]);
        opl(0x40 + m, i == 0 ? 20 : i == 4 ? 28 : 5);
        opl(0x40 + c, i == 2 ? 22 : 5);
        opl(0x60 + m, 0xf4);
        opl(0x60 + c, i == 2 ? 0xf2 : 0xf5);
        opl(0x80 + m, 0x47);
        opl(0x80 + c, i == 0 ? 0x28 : 0x56);
        opl(0xe0 + m, i == 0 ? 0 : 1);
        opl(0xe0 + c, 0);
        opl(0xc0 + i, i == 0 ? 4 : i == 4 ? 2 : 14);
    }
}
static void note(int ch, unsigned f, int block, int life) {
    if (!audiofound || muted)
        return;
    opl(0xb0 + ch, 0);
    sfxpitch[ch] = f;
    pitchhi[ch] = (unsigned char)((block << 2) | 32);
    opl(0xa0 + ch, f & 255);
    opl(0xb0 + ch, pitchhi[ch] | (f >> 8));
    sfxleft[ch] = life;
}
static void audio_tick(void) {
    int i;
    if (!audiofound || muted)
        return;
    for (i = 0; i < 5; i++)
        if (sfxleft[i]) {
            --sfxleft[i];
            if (!sfxleft[i])
                opl(0xb0 + i, 0);
            else if (i == 0 || i == 1 || i == 3) {
                unsigned p = sfxpitch[i];
                p -= p >> (i == 0 ? 3 : 4);
                sfxpitch[i] = p;
                opl(0xa0 + i, p & 255);
                opl(0xb0 + i, pitchhi[i] | (p >> 8));
            }
        }
}
static Point rotated(int x, int y, int a) {
    Point p;
    int job[6];
    int sn = sintab[a & 31], cs = sintab[(a + 8) & 31];
    if (fpu) {
        job[0] = x;
        job[1] = y;
        job[2] = cs;
        job[3] = sn;
        rotate87(job);
        p.x = job[4];
        p.y = job[5];
    } else {
        p.x = (int)(((long)x * cs - (long)y * sn + 8192L) >> 14);
        p.y = (int)(((long)x * sn + (long)y * cs + 8192L) >> 14);
    }
    return p;
}

/* A 64x64 local grid fits every asteroid at both scales. Sparse XOR byte
   masks retain crossings and shared-vertex parity exactly. Allocate each
   shape separately so no far pointer crosses a 64K segment. Failure simply
   retains the assembly line path. No allocations occur during play. */
static unsigned __far *outline_cache[9];
static unsigned outline_start[9][32], outline_count[9][32];
static unsigned char outline_bits[512];
static void outline_raster(int shape, int angle) {
    int j, k, x, y, xx, yy, dx, dy, sx, sy, major, error, i;
    Point *v = shapes[shape][angle];
    memset(outline_bits,0,sizeof(outline_bits));
    for(j=0;j<8;j++) {
        k=(j+1)&7; x=v[j].x+32; y=v[j].y+32;
        xx=v[k].x+32; yy=v[k].y+32;
        dx=abs(xx-x); dy=abs(yy-y); sx=x<xx?1:-1; sy=y<yy?1:-1;
        major=dx>dy?dx:dy; error=major/2;
        if(!major) major=1;
        for(i=0;i<major;i++) {
            outline_bits[y*8+(x>>3)] ^= 0x80U >> (x&7);
            if(dx>=dy) { x+=sx; error-=dy; if(error<0) { y+=sy; error+=dx; } }
            else { y+=sy; error-=dx; if(error<0) { x+=sx; error+=dy; } }
        }
    }
}
static void cache_outlines(void) {
    int shape, angle, i;
    unsigned words;
    unsigned __far *v;
    for(shape=0;shape<9;shape++) {
        if(outline_cache[shape]) _ffree(outline_cache[shape]);
        outline_cache[shape]=0; words=0;
        for(angle=0;angle<32;angle++) {
            outline_raster(shape,angle);
            outline_start[shape][angle]=words;
            outline_count[shape][angle]=0;
            for(i=0;i<512;i++) if(outline_bits[i]) {
                words+=2; ++outline_count[shape][angle];
            }
        }
        /* At most 512 byte cells per angle; actual outlines are much smaller. */
        if(words>32760) continue;
        v=_fmalloc(words*sizeof(unsigned));
        if(!v) continue;
        outline_cache[shape]=v;
        for(angle=0;angle<32;angle++) {
            outline_raster(shape,angle);
            for(i=0;i<512;i++) if(outline_bits[i]) {
                *v++=(i>>3)*(width>>3)+(i&7);
                *v++=(unsigned)outline_bits[i]<<8;
            }
        }
    }
}

static void geometry(void) {
    static const int outlines[3][8][2] = {
        {{0, -16}, {11, -12}, {15, 0}, {9, 11}, {-2, 15}, {-12, 9}, {-15, -2}, {-9, -11}},
        {{2, -15}, {12, -9}, {9, -2}, {15, 7}, {0, 15}, {-9, 10}, {-15, -3}, {-8, -13}},
        {{-2, -14}, {10, -12}, {15, 1}, {7, 6}, {4, 15}, {-9, 12}, {-14, 0}, {-7, -6}}};
    static const int ship[4][2] = {{0, -8}, {5, 6}, {0, 3}, {-5, 6}},
                     fire[3][2] = {{-2, 6}, {0, 11}, {2, 6}};
    unsigned cw[2];
    int s, v, a, i, x, y;
    fpu = !nofpu && probe87(cw);
    for (s = 0; s < 3; s++)
        for (v = 0; v < 3; v++)
            for (a = 0; a < 32; a++)
                for (i = 0; i < 8; i++) {
                    x = outlines[v][i][0] * radius[s] * scale / 16;
                    y = outlines[v][i][1] * radius[s] * scale / 16;
                    shapes[s * 3 + v][a][i] = rotated(x, y, a);
                }
    for (a = 0; a < 32; a++) {
        for (i = 0; i < 4; i++)
            shipshape[a][i] = rotated(ship[i][0] * scale, ship[i][1] * scale, a);
        for (i = 0; i < 3; i++)
            flame[a][i] = rotated(fire[i][0] * scale, fire[i][1] * scale, a);
    }
    if (fpu)
        done87(cw + 1);
    cache_outlines();
}
/* Interior and seam-crossing lines both stay in assembly. */
static void line(int x, int y, int xx, int yy) {
    if (x >= 0 && xx >= 0 && x < width && xx < width && y >= 0 && yy >= 0 && y < height - hud &&
        yy < height - hud)
        segment(x, y + hud, xx, yy + hud);
    else
        wrapped(x, y + hud, xx, yy + hud);
}
static void picture(const Picture *p) {
    Point *v;
    int i, j, n;
    if (!p->on)
        return;
    ink(p->color);
    if (p->shape < 0) {
        dot(p->x, p->y + hud);
        if (p->shape == -2)
            dot(wrap(p->x + 1, width), p->y + hud);
        return;
    }
    if (p->shape >= 0 && p->shape < 9 && outline_cache[p->shape] &&
        p->x >= 32 && p->x < width-32 && p->y >= 32 && p->y < height-hud-32) {
        outline((p->y-32+hud)*(width>>3)+((p->x-32)>>3),
                outline_cache[p->shape]+outline_start[p->shape][p->angle],
                (p->x-32)&7, outline_count[p->shape][p->angle]);
        return;
    }
    if (p->shape == 9) {
        v = shipshape[p->angle];
        n = 4;
    } else if (p->shape == 10) {
        v = flame[p->angle];
        n = 3;
    } else {
        v = shapes[p->shape][p->angle];
        n = 8;
    }
    for (i = 0; i < n; i++) {
        j = i + 1;
        if (j == n)
            j = 0;
        line(p->x + v[i].x, p->y + v[i].y, p->x + v[j].x, p->y + v[j].y);
    }
}
static void replace(Picture *old, const Picture *now) {
    if (!memcmp(old, now, sizeof(*old)))
        return;
    picture(old);
    picture(now);
    *old = *now;
}
static void textxy(int x, int y, const char *s, int color) {
    unsigned ch;
    int row, col, sx, sy;
    ink(color);
    while (*s) {
        ch = (unsigned char)*s++;
        if (ch < 32 || ch > 90)
            ch = 32;
        for (row = 0; row < 7; row++)
            for (col = 0; col < 5; col++)
                if (font[(ch - 32) * 7 + row] & (128 >> col))
                    for (sy = 0; sy < scale; sy++)
                        for (sx = 0; sx < scale; sx++)
                            dot(x + col * scale + sx, y + row * scale + sy);
        x += 6 * scale;
    }
}
static void centered(int y, const char *s, int c) {
    textxy((width - (int)strlen(s) * 6 * scale) / 2, y, s, c);
}
/* Expand five font bits to ten pixels. XOR before expansion so identical
   pixels, even within a changed digit, never need a video-memory access. */
static const unsigned hudwide[32] = {
    0, 3, 12, 15, 48, 51, 60, 63, 192, 195, 204, 207, 240, 243, 252, 255,
    768, 771, 780, 783, 816, 819, 828, 831, 960, 963, 972, 975, 1008, 1011, 1020, 1023};
static void hud_text(int row, const char *s) {
    unsigned masks[7], a, b, d, offset, shift;
    int i, y, x = 2, oldlen = strlen(oldhud[row]), newlen = strlen(s);
    ink(12);
    for (i = 0; i < oldlen || i < newlen; i++, x += 6 * scale) {
        a = i < oldlen ? (unsigned char)oldhud[row][i] : ' ';
        b = i < newlen ? (unsigned char)s[i] : ' ';
        if (a == b)
            continue;
        if (a < 32 || a > 90) a = 32;
        if (b < 32 || b > 90) b = 32;
        shift = x & 7;
        for (y = 0; y < 7; y++) {
            d = font[(a - 32) * 7 + y] ^ font[(b - 32) * 7 + y];
            masks[y] = (scale == 2 ? hudwide[d >> 3] << 6 : d << 8) >> shift;
        }
        /* At x=2+6*scale*i, a 5/10-pixel glyph spans at most two bytes. */
        offset = row * 8 * scale * (width / 8) + (x >> 3);
        hudpatch(offset, masks, scale);
    }
    strcpy(oldhud[row], s);
}
/* Scores are capped at 99999. Avoid general printf formatting and software
   32-bit division in the hit path; at most 45 bounded subtractions. */
static char *hud_number(char *p, unsigned long n, int padded) {
    static const unsigned powers[5] = {10000, 1000, 100, 10, 1};
    int i, digit;
    for (i = 0; i < 5; i++) {
        digit = 0;
        while (n >= powers[i]) {
            n -= powers[i];
            ++digit;
        }
        if (digit || padded || i == 4) {
            *p++ = '0' + digit;
            padded = 1;
        }
    }
    *p = 0;
    return p;
}
static void hud_update(void) {
    char b[80], *p;
    static unsigned long lastscore, lastbest;
    static int lastwave, lastlives, laststatus;
    int i, status = paused | (gameover << 1) | (started << 2) | ((dead != 0) << 3) | (muted << 4) |
                    (audiofound << 5);
    for (i = 0; i < 2; i++) {
        if (!i && oldhud[0][0] && score == lastscore && best == lastbest && wave == lastwave)
            continue;
        if (i && oldhud[1][0] && lives == lastlives && status == laststatus)
            continue;
        if (!i) {
            p = hud_number(b, score, 1);
            memcpy(p, " BEST ", 6);
            p = hud_number(p + 6, best, 1);
            memcpy(p, "  WAVE ", 7);
            hud_number(p + 7, (unsigned)wave, 0);
        } else {
            memcpy(b, "LIVES ", 6);
            p = hud_number(b + 6, (unsigned)lives, 0);
            *p++ = ' ';
            *p++ = ' ';
            strcpy(p, paused            ? "PAUSED"
                    : gameover             ? "GAME OVER - SPACE"
                    : !started             ? "SPACE TO START"
                    : dead                 ? "RESPAWNING"
                    : muted || !audiofound ? "SOUND OFF"
                                           : "ADLIB");
        }
        hud_text(i, b);
    }
    lastscore = score;
    lastbest = best;
    lastwave = wave;
    lastlives = lives;
    laststatus = status;
}
static void ring(int cx, int cy, int rx, int ry) {
    int a, x, y, xx, yy;
    for (a = 0; a < 32; a++) {
        x = cx + (int)((long)sintab[(a + 8) & 31] * rx / 16384);
        y = cy + (int)((long)sintab[a] * ry / 16384);
        xx = cx + (int)((long)sintab[(a + 9) & 31] * rx / 16384);
        yy = cy + (int)((long)sintab[(a + 1) & 31] * ry / 16384);
        line(x, y, xx, yy);
    }
}
static void background(void) {
    unsigned saved = rng;
    int i, x, y, cx, cy, r;
    wipe();
    raster(0x10);
    rng = (unsigned)(wave * 971 + 31);
    ink(2);
    for (i = 0; i < 70; i++) {
        x = random16() % width;
        y = random16() % (height - hud);
        dot(x, y + hud);
        if (!(i % 11)) {
            line(x - 2, y, x + 3, y);
            line(x, y - 2, x, y + 3);
        }
    }
    for (i = 0; i < 2; i++) {
        cx = (i ? worldw * 3 / 4 : worldw / 4) * scale;
        cy = (i ? worldh * 2 / 3 : worldh / 3) * scale;
        r = (i ? 25 : 38) * scale;
        cx += (int)(random16() % 21) * scale - 10 * scale;
        cy += (int)(random16() % 15) * scale - 7 * scale;
        ring(cx, cy, r, r);
        ring(cx, cy, r / 3, r);
        ring(cx, cy, r * 2 / 3, r);
        ring(cx, cy, r, r / 3);
        if ((wave + i) & 1)
            ring(cx, cy, r + r / 2, r / 3);
    }
    rng = saved;
    raster(0x18);
    memset(oldrocks, 0, sizeof(oldrocks));
    memset(oldshots, 0, sizeof(oldshots));
    memset(oldsparks, 0, sizeof(oldsparks));
    memset(&oldship, 0, sizeof(oldship));
    memset(&oldflame, 0, sizeof(oldflame));
    memset(oldhud, 0, sizeof(oldhud));
    ink(3);
    segment(0, hud - 2, width - 1, hud - 2);
    if (!started) {
        centered(height / 2 - 30 * scale, "NUXTROIDS", 15);
        centered(height / 2 - 12 * scale, "SPACE TO LAUNCH", 12);
        centered(height / 2 + 7 * scale, "ARROWS: TURN / THRUST", 9);
        centered(height / 2 + 17 * scale, "SPACE: FIRE  DOWN: BRAKE", 9);
        centered(height / 2 + 27 * scale, "P: PAUSE  M: MUTE  ESC: EXIT", 9);
        centered(height / 2 + 39 * scale, "J: CALIBRATE JOYSTICK", 9);
    }
    redraw = 0;
    resetclock = 1; /* Static scene construction does not consume game time. */
}
static void render(void) {
    Picture p;
    int i;
    if (redraw)
        background();
    memset(&p, 0, sizeof(p));
    for (i = 0; i < ROCKS; i++) {
        if (!rocks[i].on && !oldrocks[i].on) continue;
        p.on = rocks[i].on;
        p.x = (rocks[i].x >> 6) * scale;
        p.y = (rocks[i].y >> 6) * scale;
        p.shape = rocks[i].size * 3 + rocks[i].variant;
        p.angle = rocks[i].angle;
        p.color = monochrome ? 15 : 12;
        replace(&oldrocks[i], &p);
    }
    p.shape = -2;
    p.color = 15;
    p.angle = 0;
    for (i = 0; i < SHOTS; i++) {
        if (!shots[i].life && !oldshots[i].on) continue;
        p.on = shots[i].life != 0;
        p.x = (shots[i].x >> 6) * scale;
        p.y = (shots[i].y >> 6) * scale;
        replace(&oldshots[i], &p);
    }
    p.shape = -1;
    for (i = 0; i < SPARKS; i++) {
        if (!sparks[i].life && !oldsparks[i].on) continue;
        p.on = sparks[i].life != 0;
        p.x = (sparks[i].x >> 6) * scale;
        p.y = (sparks[i].y >> 6) * scale;
        p.color = monochrome ? 15 : 2 + sparks[i].life / 2;
        replace(&oldsparks[i], &p);
    }
    p.shape = 9;
    p.on = started && !dead && !gameover && (!protect || (framephase & 4));
    p.x = (shipx >> 6) * scale;
    p.y = (shipy >> 6) * scale;
    p.angle = heading;
    p.color = 15;
    replace(&oldship, &p);
    p.shape = 10;
    p.on = p.on && thrusting && (framephase & 1);
    p.color = monochrome ? 15 : 9;
    replace(&oldflame, &p);
    hud_update();
    ++frames;
}
static void burst(int x, int y, int count) {
    int i, a;
    for (i = 0; i < SPARKS && count; i++)
        if (!sparks[i].life) {
            a = random16() & 31;
            sparks[i].x = x;
            sparks[i].y = y;
            sparks[i].vx = sintab[a] / 128;
            sparks[i].vy = sintab[(a + 8) & 31] / 128;
            sparks[i].life = 12;
            --count;
        }
}
static void rock_add(int slot, int x, int y, int size, int angle) {
    Rock *r = &rocks[slot];
    int speed = 28 + size * 18 + (wave < 10 ? wave : 10) * 3;
    r->x = x;
    r->y = y;
    r->vx = (int)((long)sintab[angle & 31] * speed / 16384);
    r->vy = (int)((long)sintab[(angle + 8) & 31] * speed / 16384);
    r->on = 1;
    r->size = size;
    r->variant = random16() % 3;
    r->angle = random16() & 31;
    r->spin = (random16() & 1) ? 1 : 31;
    r->grace = 8;
    ++remaining;
}
static void respawn(void) {
    int i, j, safe;
    shipvx = shipvy = heading = 0;
    shipx = worldw * 32;
    shipy = worldh * 32;
    for (i = 0; i < 12; i++) {
        safe = 1;
        for (j = 0; j < ROCKS; j++)
            if (rocks[j].on && nearxy(shipx, shipy, rocks[j].x, rocks[j].y, 45)) {
                safe = 0;
                break;
            }
        if (safe)
            break;
        shipx = (random16() % worldw) * 64;
        shipy = (random16() % worldh) * 64;
    }
    protect = 109;
    dead = 0;
    shootqueue = 0;
    shootwait = 8;
}
static void new_wave(void) {
    int i, n;
    memset(rocks, 0, sizeof(rocks));
    memset(shots, 0, sizeof(shots));
    memset(sparks, 0, sizeof(sparks));
    remaining = 0;
    levelwait = 0;
    n = 3 + wave;
    if (n > 7)
        n = 7;
    for (i = 0; i < n; i++)
        rock_add(i, (i & 1) ? (worldw - 16) * 64 : 16 * 64, (random16() % worldh) * 64, 0,
                 random16() & 31);
    respawn();
    redraw = 1;
}
static void start_game(void) {
    started = 1;
    gameover = paused = 0;
    wave = 1;
    lives = 3;
    score = 0;
    nextlife = 5000;
    shootwait = shootqueue = 0;
    heartbeat = 0;
    new_wave();
}
static void hit(int index) {
    Rock r = rocks[index];
    int i, slots[2], n = 0, a;
    rocks[index].on = 0;
    --remaining;
    score += (r.size == 0 ? 20 : r.size == 1 ? 50 : 100);
    if (score > 99999UL)
        score = 99999UL;
    if (score > best)
        best = score;
    if (score >= nextlife && nextlife <= 95000UL) {
        if (lives < 5)
            ++lives;
        nextlife += 5000;
    }
    burst(r.x, r.y, 4);
    note(1, 210 + r.size * 100, 2, 7);
    if (r.size == 2)
        return;
    for (i = 0; i < ROCKS && n < 2; i++)
        if (!rocks[i].on)
            slots[n++] = i;
    a = random16() & 31;
    for (i = 0; i < n; i++)
        rock_add(slots[i], r.x, r.y, r.size + 1, (a + i * 16) & 31);
}
static void fire(void) {
    int i;
    Shot *b;
    for (i = 0; i < SHOTS; i++)
        if (!shots[i].life) {
            b = &shots[i];
            b->x = wrap(shipx + sintab[heading] / 32, worldw * 64);
            b->y = wrap(shipy - sintab[(heading + 8) & 31] / 32, worldh * 64);
            b->vx = sintab[heading] / 52 + shipvx;
            b->vy = -sintab[(heading + 8) & 31] / 52 + shipvy;
            b->life = 48;
            shootwait = 6;
            shootqueue = 0;
            note(0, 780, 4, 5);
            return;
        }
}
static void logic(void) {
    int i, j, k, a, dx, dy;
    Rock *r;
    Shot *b;
    ++framephase;
    framephase &= 255;
    ++updates;
    audio_tick();
    for (i = 0; i < ROCKS; i++) {
        r = &rocks[i];
        if (!r->on)
            continue;
        r->x = wrap(r->x + r->vx, worldw * 64);
        r->y = wrap(r->y + r->vy, worldh * 64);
        /* Same rotation period, spread across ticks instead of one burst. */
        if (!((framephase + i) & 3))
            r->angle = (r->angle + r->spin) & 31;
        if (r->grace)
            --r->grace;
    }
    for (i = 0; i < SPARKS; i++)
        if (sparks[i].life) {
            --sparks[i].life;
            sparks[i].x = wrap(sparks[i].x + sparks[i].vx, worldw * 64);
            sparks[i].y = wrap(sparks[i].y + sparks[i].vy, worldh * 64);
        }
    if (!started || gameover)
        return;
    if (!remaining) {
        if (++levelwait >= 55) {
            if (wave < 999)
                ++wave;
            new_wave();
        }
        return;
    }
    if (++heartbeat >= (unsigned)(15 + remaining * 2)) {
        note(4, (framephase & 16) ? 235 : 175, 1, 4);
        heartbeat = 0;
    }
    if (dead) {
        if (!--dead) {
            if (lives)
                respawn();
            else
                gameover = 1;
        }
        return;
    }
    if (protect)
        --protect;
    if (!(framephase & 1)) {
        if (keys[75] || keys[30] || joyleft)
            heading = (heading + 31) & 31;
        if (keys[77] || keys[32] || joyright)
            heading = (heading + 1) & 31;
    }
    thrusting = keys[72] || keys[17] || joyup;
    if (thrusting) {
        a = heading;
        shipvx += sintab[a] / 2730;
        shipvy -= sintab[(a + 8) & 31] / 2730;
        if (shipvx > 192)
            shipvx = 192;
        if (shipvx < -192)
            shipvx = -192;
        if (shipvy > 192)
            shipvy = 192;
        if (shipvy < -192)
            shipvy = -192;
        if (!(framephase & 7))
            note(2, 90 + (random16() & 31), 2, 8);
    }
    if (keys[80] || keys[31] || joydown) {
        shipvx -= shipvx / 16;
        shipvy -= shipvy / 16;
        if (abs(shipvx) < 16)
            shipvx = 0;
        if (abs(shipvy) < 16)
            shipvy = 0;
    }
    shipx = wrap(shipx + shipvx, worldw * 64);
    shipy = wrap(shipy + shipvy, worldh * 64);
    if (shootwait)
        --shootwait;
    if (shootqueue)
        --shootqueue;
    for (i = 0; i < SHOTS; i++) {
        b = &shots[i];
        if (!b->life)
            continue;
        --b->life;
        if (!b->life)
            continue;
        dx = b->vx / 2;
        dy = b->vy / 2;
        for (k = 0; k < 2 && b->life; k++) {
            b->x = wrap(b->x + (k ? b->vx - dx : dx), worldw * 64);
            b->y = wrap(b->y + (k ? b->vy - dy : dy), worldh * 64);
            for (j = 0; j < ROCKS; j++)
                if (rocks[j].on && !rocks[j].grace &&
                    nearxy(b->x, b->y, rocks[j].x, rocks[j].y, radius[rocks[j].size])) {
                    b->life = 0;
                    hit(j);
                    break;
                }
        }
    }
    if ((keys[57] || joyfire || shootqueue) && !shootwait)
        fire();
    if (!protect)
        for (i = 0; i < ROCKS; i++)
            if (rocks[i].on &&
                nearxy(shipx, shipy, rocks[i].x, rocks[i].y, radius[rocks[i].size] + 3)) {
                burst(shipx, shipy, 12);
                dead = 55;
                --lives;
                thrusting = 0;
                memset(shots, 0, sizeof(shots));
                note(3, 600, 2, 20);
                break;
            }
}
#include "JOYSTICK.H"
static void controls(void) {
    if (edge(36))
        joy_calibrate();
    if (edge(37))
        joy_off();
    if (edge(1))
        quit = 1;
    if (edge(50)) {
        muted = !muted;
        if (muted)
            silence();
    }
    if (edge(25) && started && !gameover) {
        paused = !paused;
        if (paused)
            silence();
    }
    if (edge(57) || joyedge) {
        joyedge = 0;
        if (!started || gameover)
            start_game();
        else if (!paused)
            shootqueue = 7;
    }
}
static void usage(void) {
    puts("NuXTroids 1.2\nNUXTROID [/L | /V | /8] [/S] [/N] [/M] "
         "[/J]\n/L 320x200  /V 640x480 (default)  /8 800x600\n16 greyscale shades in every mode. "
         "/M bright monochrome objects.\n/J calibrate gameport joystick. J recalibrates, K "
         "disables it.\n/S silent (no AdLib probe)  /N integer geometry (no 8087 probe)\nArrow "
         "keys turn/thrust; Down brakes; Space fires; P pauses; M mutes.\nEscape returns to DOS. "
         "V20 or 80186-compatible CPU required.");
}
#ifdef SELFTEST
#include "TEST.H"
#else
int main(int argc, char **argv) {
    union REGS r;
    unsigned oldmode, oldpage, last, now, pending;
    int mode = 1, i, n;
    for (i = 1; i < argc; i++) {
        if (!strcmpi(argv[i], "/L"))
            mode = 0;
        else if (!strcmpi(argv[i], "/V"))
            mode = 1;
        else if (!strcmpi(argv[i], "/8"))
            mode = 2;
        else if (!strcmpi(argv[i], "/S"))
            muted = 1;
        else if (!strcmpi(argv[i], "/N"))
            nofpu = 1;
        else if (!strcmpi(argv[i], "/J"))
            joylaunch = 1;
        else if (!strcmpi(argv[i], "/M"))
            monochrome = 1;
        else {
            usage();
            return strcmpi(argv[i], "/?") ? 1 : 0;
        }
    }
    width = mode == 0 ? 320 : mode == 1 ? 640 : 800;
    height = mode == 0 ? 200 : mode == 1 ? 480 : 600;
    scale = mode == 0 ? 1 : 2;
    printf("NuXTroids: preparing %ux%u...\n", width, height);
    geometry();
    printf("Geometry: %s. Escape exits.\n", fpu ? "8087" : "integer");
    memset(&r, 0, sizeof(r));
    r.h.ah = 15;
    int86(0x10, &r, &r);
    oldmode = r.h.al & 127;
    oldpage = r.h.bh;
    if (!video_start(mode)) {
        biosmode(oldmode);
        puts("This VGA BIOS cannot provide the requested planar mode. Try /V or /L.");
        return 1;
    }
    rng = (unsigned)*(unsigned long __far *)MK_FP(0x40, 0x6c);
    if (!rng)
        rng = 0x7a31;
    if (!muted) {
        audiofound = adlib();
        if (audiofound)
            instruments();
    }
    wave = 1;
    lives = 3;
    new_wave();
    install();
    if (joylaunch)
        joy_calibrate();
    last = ticks;
    while (!quit) {
        controls();
        if (resetclock) {
            last = ticks;
            resetclock = 0;
        }
        now = ticks;
        pending = (unsigned)(now - last);
        if (!pending)
            continue;
        last = now;
        if (pending > 3) {
            dropped += pending - 3;
            pending = 3;
        }
        if (joyenabled)
            joy_poll();
        controls();
        n = pending;
        while (n--)
            if (!paused)
                logic();
        render();
    }
    silence();
    uninstall();
    biosmode(oldmode);
    memset(&r, 0, sizeof(r));
    r.h.ah = 5;
    r.h.al = oldpage;
    int86(0x10, &r, &r);
    printf("NuXTroids finished. %lu frames, %lu game ticks, %lu skipped ticks.\n", frames, updates,
           dropped);
    printf("Video, keyboard and timer restored. Best score: %lu.\n", best);
    return 0;
}
#endif
