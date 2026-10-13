/* NUXTRIS 1.2. V20 DOS, VGA 640x480x16. See CREDITS.TXT. */
#include <conio.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CORE.H"
#include "FONT.H"
#include "SCORES.H"
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
extern void hudpatch(unsigned, const unsigned *, unsigned);
#pragma aux hudpatch parm[di][si][cx] modify[ax bx cx dx si di]
extern unsigned joyread(unsigned *);
#pragma aux joyread parm[si] value[ax] modify[ax bx dx]
extern void clearcolumn(unsigned, unsigned);
#pragma aux clearcolumn parm[di][cx] modify[ax cx di]
#include "INPUT.H"
#include "SOUND.H"
enum { MENU, PLAY, PAUSED, NAME, SCORES };
static int state = MENU, quit, redraw = 1, resetclock, menusel, tabletype;
static int width = 640, height = 480, scale = 2;
static int joyenabled, joyleft, joyright, joyup, joydown, joyfire, joyedge, joyold, joyfail,
    joyback, joysettle;
static unsigned joyxmin, joyxmax, joyymin, joyymax;
static int clearwait, clearnum, fxwait, won, pendingplace, namepos, namefresh;
static int resultshown;
static int moveheld, moveage, uiheld, uiage, oldup, olddown, active;
static unsigned fallticks;
static unsigned char shown[BH][BW];
static char oldhud[5][24];
static uint32_t hudscore, hudbest;
static uint32_t settledscore; /* Displayed score commits on lock/line clear. */
#ifdef SELFTEST
static unsigned hudglyphs, hudformats;
#endif
static unsigned hudlevel, hudlines, hudsound;
static unsigned long frames;
/* Cache only menu/name text; any full clear invalidates both screens. */
static int uiscreen = -1, shownmenusel = -1, showncursor = -1;
static char menutext[6][40], menunotice[60], nametext[9];
#ifdef SELFTEST
static unsigned uiclears, uiglyphs;
#endif
static void screen_wipe(void) {
    wipe();
    uiscreen = -1;
#ifdef SELFTEST
    ++uiclears;
#endif
}
#define wipe screen_wipe

static const char *themenames[4] = {"WHITE", "RED", "GREEN", "AMBER"};
static const char *soundnames[4] = {"SILENT", "ADLIB", "SPEAKER", "BOTH"};
static void rect(int x, int y, int w, int h) {
    segment(x, y, x + w, y);
    segment(x + w, y, x + w, y + h);
    segment(x + w, y + h, x, y + h);
    segment(x, y + h, x, y);
}
static unsigned glyph(char c, int row) {
    if (c == '!')
        return row == 5 ? 0 : 32;
    if (c == '>') {
        static const unsigned char g[7] = {128, 64, 32, 16, 32, 64, 128};
        return g[row];
    }
    if (c == '<') {
        static const unsigned char g[7] = {8, 16, 32, 64, 32, 16, 8};
        return g[row];
    }
    if (c == '_')
        return row == 6 ? 248 : 0;
    if (c == '=')
        return row == 2 || row == 4 ? 248 : 0;
    if (c < 32 || c > 90)
        return 0;
    return font[(c - 32) * 7 + row];
}
static void textxy(int x, int y, const char *s, int color, int size) {
    int r, c, a, b;
    unsigned bits;
    ink(color);
    while (*s) {
        for (r = 0; r < 7; ++r) {
            bits = glyph(*s, r);
            for (c = 0; c < 5; ++c)
                if (bits & (128 >> c))
                    for (a = 0; a < size; ++a)
                        for (b = 0; b < size; ++b)
                            dot(x + c * size + a, y + r * size + b);
        }
        x += 6 * size;
        ++s;
    }
}
/* XOR only changed glyph pixels over the existing black UI background.
 * The fixed cell spacing keeps neighboring glyphs and borders untouched. */
static void text_delta(int x, int y, char *old, const char *s, int size) {
    int i, r, c, a, b, olen = strlen(old), nlen = strlen(s);
    unsigned bits;
    if (!strcmp(old, s)) return;
    raster(0x18);
    ink(15);
    for (i = 0; i < olen || i < nlen; ++i) {
        char before = i < olen ? old[i] : ' ', after = i < nlen ? s[i] : ' ';
        if (before == after) continue;
        for (r = 0; r < 7; ++r) {
            bits = glyph(before, r) ^ glyph(after, r);
            for (c = 0; c < 5; ++c)
                if (bits & (128 >> c))
                    for (a = 0; a < size; ++a)
                        for (b = 0; b < size; ++b)
                            dot(x + i * 6 * size + c * size + a, y + r * size + b);
        }
#ifdef SELFTEST
        ++uiglyphs;
#endif
    }
    strcpy(old, s);
}
static void centered(int y, const char *s, int color) {
    textxy((width - (int)strlen(s) * 12) / 2, y, s, color, 2);
}
static void clearbox(int x, int y, int w, int h) {
    unsigned l = x >> 3, r = (x + w - 1) >> 3, b;
    unsigned char mask, a = (unsigned char)(255 >> (x & 7)),
                        z = (unsigned char)(255 << (7 - ((x + w - 1) & 7)));
    ink(15);
    raster(0);
    for (b = l; b <= r; ++b) {
        mask = 255;
        if (b == l)
            mask &= a;
        if (b == r)
            mask &= z;
        outpw(0x3ce, ((unsigned)mask << 8) | 8);
        clearcolumn(y * 80 + b, h);
    }
    outpw(0x3ce, 0xff08);
    raster(0x18);
}
static void dac(unsigned i, unsigned r, unsigned g, unsigned b) {
    outp(0x3c8, i);
    outp(0x3c9, r);
    outp(0x3c9, g);
    outp(0x3c9, b);
}
static void settheme(void) {
    static const unsigned char rgb[4][3] = {{58, 61, 63}, {63, 8, 10}, {10, 63, 20}, {63, 39, 5}};
    dac(15, rgb[theme][0], rgb[theme][1], rgb[theme][2]);
    dac(8, rgb[theme][0] / 5, rgb[theme][1] / 5, rgb[theme][2] / 5);
}
static void fxbrightness(int n) { dac(14, n, n / 2, n); }
static void biosmode(unsigned m) {
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = m;
    int86(0x10, &r, &r);
}
static unsigned modequery(void) {
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.h.ah = 15;
    int86(0x10, &r, &r);
    return r.h.al & 127;
}
static int video_start(void) {
    static const unsigned char colors[7][3] = {{63, 52, 8}, {45, 20, 63}, {63, 32, 8}, {12, 26, 63},
                                               {8, 57, 63}, {16, 63, 27}, {63, 12, 24}};
    union REGS r;
    int i;
    biosmode(0x12);
    if (modequery() != 0x12)
        return 0;
    for (i = 0; i < 16; ++i) {
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
    outpw(0x3c4, 0x0604);
    outpw(0x3ce, 0x0506);
    outpw(0x3ce, 0x0000);
    outpw(0x3ce, 0x0001);
    outpw(0x3ce, 0x0003);
    outpw(0x3ce, 0x0005);
    outpw(0x3ce, 0xff08);
    outpw(0x3c4, 0x0f02);
    outp(0x3c6, 255);
    for (i = 0; i < 16; ++i)
        dac(i, 0, 0, 0);
    for (i = 0; i < 7; ++i)
        dac(i + 1, colors[i][0], colors[i][1], colors[i][2]);
    settheme();
    video_init(80, 480);
    wipe();
    return 1;
}
static void outline_text(int x, int y, const char *s, int unit, int color) {
    int r, c;
    unsigned bits, above, below;
    ink(color);
    raster(0x10);
    while (*s) {
        for (r = 0; r < 7; ++r) {
            bits = glyph(*s, r);
            above = r ? glyph(*s, r - 1) : 0;
            below = r < 6 ? glyph(*s, r + 1) : 0;
            for (c = 0; c < 5; ++c)
                if (bits & (128 >> c)) {
                    int xx = x + c * unit, yy = y + r * unit;
                    if (!(above & (128 >> c)))
                        segment(xx, yy, xx + unit, yy);
                    if (!(below & (128 >> c)))
                        segment(xx, yy + unit, xx + unit, yy + unit);
                    if (!c || !(bits & (256 >> c)))
                        segment(xx, yy, xx, yy + unit);
                    if (c == 4 || !(bits & (64 >> c)))
                        segment(xx + unit, yy, xx + unit, yy + unit);
                }
        }
        x += 6 * unit;
        ++s;
    }
    raster(0x18);
}
static void logo(int y) {
    int x = 202;
    ink(15);
    raster(0x10);
    /* Custom Cyrillic И and я with Latin uXT/is: no code-page assumption. */
    segment(x, y, x, y + 24);
    segment(x, y + 24, x + 24, y);
    segment(x + 24, y, x + 24, y + 24);
    x += 34;
    segment(x, y + 6, x, y + 24);
    segment(x, y + 24, x + 20, y + 24);
    segment(x + 20, y + 24, x + 20, y + 6);
    x += 30;
    segment(x, y, x + 24, y + 24);
    segment(x + 24, y, x, y + 24);
    x += 34;
    segment(x, y, x + 24, y);
    segment(x + 12, y, x + 12, y + 24);
    x += 34;
    segment(x + 24, y + 6, x + 24, y + 24);
    segment(x + 24, y + 6, x, y + 6);
    segment(x, y + 6, x, y + 14);
    segment(x, y + 14, x + 24, y + 14);
    segment(x + 24, y + 14, x, y + 24);
    x += 34;
    segment(x + 5, y + 8, x + 5, y + 24);
    rect(x + 4, y, 2, 2);
    x += 16;
    segment(x + 20, y + 6, x, y + 6);
    segment(x, y + 6, x, y + 15);
    segment(x, y + 15, x + 20, y + 15);
    segment(x + 20, y + 15, x + 20, y + 24);
    segment(x + 20, y + 24, x, y + 24);
    raster(0x18);
}
static void block(int x, int y, int color) {
    ink(color);
    rect(x + 2, y + 2, 15, 15);
    rect(x + 4, y + 4, 11, 11);
}
static void preview(void) {
    int i, k = game.next;
    for (i = 0; i < 4; ++i)
        block(506 + shapes[k][i * 2] * 20, 106 + shapes[k][i * 2 + 1] * 20, k + 1);
}
static void board_render(void) {
    unsigned char desired[BH][BW];
    int i, x, y, c;
    memcpy(desired, game.grid, sizeof(desired));
    if (active)
        for (i = 0; i < 4; ++i) {
            x = game.x + game.shape[2 * i];
            y = game.y + game.shape[2 * i + 1];
            if (x >= 0 && x < BW && y >= 0 && y < BH)
                desired[y][x] = game.kind + 1;
        }
    if (clearwait && (clearwait & 2))
        for (y = 0; y < BH; ++y)
            if (game.full & (1UL << y))
                memset(desired[y], 0, BW);
    raster(0x18);
    for (y = 0; y < BH; ++y)
        for (x = 0; x < BW; ++x)
            if (desired[y][x] != shown[y][x]) {
                if (fxwait)
                    clearbox(220 + x * 20, 44 + y * 20, 20, 20);
                else if (shown[y][x])
                    block(220 + x * 20, 44 + y * 20, shown[y][x]);
                c = desired[y][x];
                if (c)
                    block(220 + x * 20, 44 + y * 20, c);
                shown[y][x] = c;
            }
}
/* Same glyph-delta blitter as NuXTroids 1.1. All these HUD origins and
   12-pixel cells keep a ten-pixel glyph within two VGA bytes. */
static const unsigned hudwide[32] = {
    0, 3, 12, 15, 48, 51, 60, 63, 192, 195, 204, 207, 240, 243, 252, 255,
    768, 771, 780, 783, 816, 819, 828, 831, 960, 963, 972, 975, 1008, 1011, 1020, 1023};
static void hudline(int n, int x, int y, const char *s) {
    unsigned masks[7], d, shift;
    int i, r, oldlen = strlen(oldhud[n]), newlen = strlen(s);
    char a, b;
    ink(15);
    for (i = 0; i < oldlen || i < newlen; ++i, x += 12) {
        a = i < oldlen ? oldhud[n][i] : ' ';
        b = i < newlen ? s[i] : ' ';
        if (a == b) continue;
        shift = x & 7;
        for (r = 0; r < 7; ++r) {
            d = glyph(a, r) ^ glyph(b, r);
            masks[r] = (hudwide[d >> 3] << 6) >> shift;
        }
        hudpatch(y * 80 + (x >> 3), masks, 2);
#ifdef SELFTEST
        ++hudglyphs;
#endif
    }
    strcpy(oldhud[n], s);
}
static void hudnumber(char *s, uint32_t n, int digits) {
    static const uint32_t powers[9] = {100000000UL, 10000000UL, 1000000UL,
        100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL};
    int i, d, printing = 0;
#ifdef SELFTEST
    ++hudformats;
#endif
    for (i = 0; i < 9; ++i) {
        d = 0;
        while (n >= powers[i]) { n -= powers[i]; ++d; }
        if (d || printing || 9 - i <= digits) {
            *s++ = '0' + d;
            printing = 1;
        }
    }
    *s = 0;
}
static void hud(void) {
    char s[24];
    int fresh = !oldhud[0][0];
    if (fresh || hudscore != settledscore) {
        hudnumber(s, settledscore, 9);
        hudline(0, 36, 126, s);
        hudscore = settledscore;
    }
    if (fresh || hudlevel != game.level) {
        hudnumber(s, game.level, 2);
        hudline(1, 464, 328, s);
        hudlevel = game.level;
    }
    if (fresh || hudlines != game.lines) {
        hudnumber(s, game.type ? (game.lines >= 25 ? 0 : 25 - game.lines) : game.lines, 3);
        hudline(2, 464, 386, s);
        hudlines = game.lines;
    }
    if (fresh || hudbest != scores[game.type][0].score) {
        hudnumber(s, scores[game.type][0].score, 9);
        hudline(3, 36, 186, s);
        hudbest = scores[game.type][0].score;
    }
    if (fresh || hudsound != soundmode) {
        hudline(4, 36, 416, soundnames[soundmode]);
        hudsound = soundmode;
    }
}
static void scene(void) {
    wipe();
    memset(shown, 0, sizeof(shown));
    memset(oldhud, 0, sizeof(oldhud));
    if (fxwait) {
        outline_text(53, 222, "TETRIS!", 13, 14);
        fxbrightness(55);
        ink(14);
        raster(0x10);
        segment(25, 200, 190, 209);
        segment(450, 209, 615, 200);
        segment(25, 333, 190, 321);
        segment(450, 321, 615, 333);
    }
    raster(0x10);
    logo(8);
    raster(0x10);
    ink(15);
    rect(216, 40, 208, 408);
    rect(12, 76, 188, 368);
    rect(440, 76, 188, 368);
    textxy(36, 90, game.type ? "TYPE B" : "TYPE A", 15, 2);
    textxy(36, 108, "SCORE", 15, 1);
    textxy(36, 168, "BEST", 15, 1);
    textxy(464, 90, "NEXT", 15, 1);
    textxy(464, 310, "LEVEL", 15, 1);
    textxy(464, 368, game.type ? "LINES LEFT" : "LINES", 15, 1);
    textxy(32, 344, "ARROWS: MOVE", 15, 1);
    textxy(32, 358, "DOWN: SOFT DROP", 15, 1);
    textxy(32, 372, "UP/X: ROTATE", 15, 1);
    textxy(32, 386, "Z: REVERSE", 15, 1);
    textxy(32, 402, "SOUND", 15, 1);
    textxy(34, 461, "F1 THEME  F2 SOUND  P PAUSE  J JOYSTICK  K KEYBOARD  ESC MENU", 15, 1);
    preview();
    raster(0x18);
    board_render();
    hud();
}
static void menu_draw(void) {
    char s[60];
    int i;
    const char *notice = "";
    if (uiscreen != MENU) {
        wipe();
        logo(26);
        ink(15);
        rect(150, 94, 340, 238);
        centered(350, "UP/DOWN SELECT - LEFT/RIGHT CHANGE", 15);
        centered(378, "ENTER OR BUTTON 1: PLAY", 15);
        centered(410, "H SCORES  J CALIBRATE  ESC EXIT", 15);
        memset(menutext, 0, sizeof(menutext));
        menunotice[0] = 0;
        shownmenusel = -1;
        uiscreen = MENU;
    }
    raster(0x18);
    if (shownmenusel != menusel) {
        if (shownmenusel >= 0) textxy(176, 112 + shownmenusel * 36, ">", 15, 2);
        textxy(176, 112 + menusel * 36, ">", 15, 2);
        shownmenusel = menusel;
    }
    for (i = 0; i < 6; ++i) {
        switch (i) {
        case 0:
            sprintf(s, "TYPE    %s", opttype ? "B - 25 LINES" : "A - ENDLESS");
            break;
        case 1:
            sprintf(s, "LEVEL   %u", optlevel);
            break;
        case 2:
            sprintf(s, "HEIGHT  %u%s", optheight, opttype ? "" : "  - B ONLY");
            break;
        case 3:
            sprintf(s, "THEME   %s", themenames[theme]);
            break;
        case 4:
            sprintf(s, "SOUND   %s", soundnames[soundmode]);
            break;
        default:
            sprintf(s, "INPUT   %s", joyenabled ? "JOYSTICK" : "KEYBOARD");
            break;
        }
        text_delta(202, 112 + i * 36, menutext[i], s, 2);
    }
    if (savefailed)
        notice = "SAVE FAILED - CHECK DISK SPACE/WRITE ACCESS";
    else if (recovered)
        notice = "SCORES RECOVERED FROM BACKUP";
    else if (!audiofound && (soundmode & 1))
        notice = "NO ADLIB DETECTED - SPEAKER AVAILABLE";
    if (strcmp(menunotice, notice)) {
        /* Notices have different centred origins, so erase at the old origin. */
        raster(0x18);
        if (menunotice[0]) centered(450, menunotice, 15);
        if (notice[0]) centered(450, notice, 15);
        strcpy(menunotice, notice);
    }
}

static void scores_draw(void) {
    char s[80];
    int i;
    Record *r;
    wipe();
    logo(16);
    centered(64, tabletype ? "TYPE B HIGH SCORES" : "TYPE A HIGH SCORES", 15);
    if (resultshown) {
        sprintf(s, "%s - SCORE %lu", won ? "TYPE B COMPLETE" : "GAME OVER",
                (unsigned long)game.score);
        textxy((640 - (int)strlen(s) * 6) / 2, 88, s, 15, 1);
    }
    textxy(64, 106, "NO  NAME       SCORE     LV HT CLEAR", 15, 2);
    for (i = 0; i < NSCORES; ++i) {
        r = &scores[tabletype][i];
        if (r->score)
            sprintf(s, "%2d  %-8s %9lu  %2u %2u  %s", i + 1, r->name, (unsigned long)r->score,
                    r->level, r->height, r->won ? "YES" : "-");
        else
            sprintf(s, "%2d  -------- ---------   -  -   -", i + 1);
        textxy(64, 136 + i * 25, s, 15, 2);
    }
    centered(416, "LEFT/RIGHT: A/B   ENTER: MENU", 15);
    if (savefailed)
        centered(449, "SAVE FAILED - SCORES STILL IN MEMORY", 15);
}
static void name_draw(void) {
    char s[60], cell[2], old[2];
    int i, cursor = namepos < 8 ? namepos : 7;
    if (uiscreen != NAME) {
        wipe();
        logo(32);
        centered(102, won ? "TYPE B COMPLETE!" : "GAME OVER", 15);
        sprintf(s, "SCORE %lu  -  HIGH SCORE %d", (unsigned long)game.score, pendingplace + 1);
        centered(145, s, 15);
        centered(204, "ENTER YOUR NAME", 15);
        centered(321, "TYPE A NAME - ENTER SAVES", 15);
        centered(350, "PAD: LEFT/RIGHT CURSOR, UP/DOWN LETTER", 15);
        centered(379, "BUTTON 1 SAVES - ESC USES CURRENT NAME", 15);
        memset(nametext, ' ', 8);
        nametext[8] = 0;
        showncursor = -1;
        uiscreen = NAME;
    }
    for (i = 0; i < 8; ++i) {
        old[0] = nametext[i]; old[1] = 0;
        cell[0] = player[i] ? player[i] : ' '; cell[1] = 0;
        text_delta(230 + i * 24, 249, old, cell, 3);
        nametext[i] = cell[0];
    }
    if (showncursor != cursor) {
        raster(0x18); ink(15);
        if (showncursor >= 0)
            segment(228 + showncursor * 24, 278, 246 + showncursor * 24, 278);
        segment(228 + cursor * 24, 278, 246 + cursor * 24, 278);
        showncursor = cursor;
    }
}
#include "JOYSTICK.H"
static void inputs_flush(void) {
    _disable();
    memset((void *)makes, 0, sizeof(makes));
    _enable();
    moveheld = moveage = uiheld = uiage = 0;
    oldup = joyup;
    olddown = joydown;
}
static int repeat_dir(int d, int *held, int *age) {
    if (!d) {
        *held = *age = 0;
        return 0;
    }
    if (d != *held) {
        *held = d;
        *age = 0;
        return d;
    }
    ++*age;
    if (*age >= 7 && ((*age - 7) % 2) == 0)
        return d;
    if (*age > 1000)
        *age = 8;
    return 0;
}
static void begin(void) {
    new_game(opttype, optlevel, optheight);
    settledscore = game.score;
    active = 1;
    state = PLAY;
    resultshown = 0;
    clearwait = fxwait = won = 0;
    fallticks = 0;
    redraw = resetclock = 1;
    inputs_flush();
}
static void endround(void) {
    settledscore = game.score;
    silence();
    active = 0;
    pendingplace = scoreplace(game.type, game.score);
    tabletype = game.type;
    resultshown = 1;
    if (pendingplace >= 0) {
        state = NAME;
        namepos = 0;
        namefresh = 1;
    } else {
        state = SCORES;
    }
    inputs_flush();
    redraw = 1;
}
static void next_piece(void) {
    if (game.type && game.lines >= 25) {
        won = 1;
        endround();
        return;
    }
    spawn();
    fallticks = 0;
    active = 1;
    if (game.over)
        endround();
    else {
        clearbox(480, 100, 122, 58);
        preview();
    }
}
static void play_tick(int cw, int ccw, int down, int downedge) {
    int d, period;
    if (fxwait) {
        --fxwait;
        fxbrightness(fxwait > 36 ? 55 : (fxwait * 55) / 36);
        if (!fxwait) {
            next_piece();
            redraw = resetclock = 1;
            inputs_flush();
        }
        return;
    }
    if (clearwait) {
        if (!--clearwait) {
            clearnum = clear_rows();
            settledscore = game.score;
            if (clearnum == 4) {
                fxwait = 54;
                sound_event(4);
                redraw = resetclock = 1;
            } else {
                sound_event(2);
                next_piece();
            }
        }
        return;
    }
    d = (keys[77] || joyright) - (keys[75] || joyleft);
    d = repeat_dir(d, &moveheld, &moveage);
    if (d)
        move_side(d);
    if (cw)
        rotate_shape(1);
    else if (ccw)
        rotate_shape(0);
    period = fall_period();
    if (down && period > 2)
        period = 2;
    if (downedge)
        fallticks = period;
    if (++fallticks >= (unsigned)period) {
        fallticks = 0;
        if (!drop_one(down)) {
            clearnum = lock_shape();
            settledscore = game.score;
            active = 0;
            sound_event(1);
            if (game.over) {
                endround();
                return;
            }
            if (clearnum)
                clearwait = 8;
            else
                next_piece();
        }
    }
}
static void changemenu(int d) {
    switch (menusel) {
    case 0:
        opttype ^= 1;
        break;
    case 1:
        optlevel = (optlevel + 10 + d) % 10;
        break;
    case 2:
        optheight = (optheight + 6 + d) % 6;
        break;
    case 3:
        theme = (theme + 4 + d) % 4;
        settheme();
        break;
    case 4:
        soundmode = (soundmode + 4 + d) % 4;
        silence();
        break;
    case 5:
        if (joyenabled)
            joy_off();
        else
            joy_calibrate();
        break;
    }
    redraw = 1;
}
static void save_name(void) {
    int i, nonspace = 0;
    for (i = 0; i < 8 && player[i]; ++i)
        if (player[i] != ' ')
            nonspace = 1;
    if (!nonspace)
        strcpy(player, "PLAYER");
    for (i = (int)strlen(player) - 1; i >= 0 && player[i] == ' '; --i)
        player[i] = 0;
    recordscore(pendingplace, won);
    savescores();
    state = SCORES;
    redraw = 1;
    inputs_flush();
}
static void name_tick(int lr, int ud, int enter, int esc) {
    static const char scanletters[27] = "QWERTYUIOPASDFGHJKLZXCVBNM";
    static const unsigned char scans[26] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 30, 31, 32,
                                            33, 34, 35, 36, 37, 38, 44, 45, 46, 47, 48, 49, 50};
    static const char chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int i, c = 0, n;
    if (enter || esc) {
        save_name();
        return;
    }
    for (i = 0; i < 26; ++i)
        if (edge(scans[i]))
            c = scanletters[i];
    for (i = 2; i <= 11; ++i)
        if (edge(i))
            c = i == 11 ? '0' : '1' + i - 2;
    if (edge(57))
        c = ' ';
    if (c && (namefresh || namepos < 8)) {
        if (namefresh) {
            memset(player, 0, sizeof(player));
            namepos = 0;
            namefresh = 0;
        }
        for (i = 0; i < namepos; ++i)
            if (!player[i])
                player[i] = ' ';
        player[namepos] = c;
        ++namepos; /* 8 is the insertion point after a full name. */
        redraw = 1;
    }
    if (edge(14)) {
        namefresh = 0;
        if (namepos > 0)
            --namepos;
        memmove(player + namepos, player + namepos + 1, 8 - namepos);
        redraw = 1;
    }
    if (lr) {
        namepos = namepos == 8 ? (lr < 0 ? 7 : 0) : (namepos + 8 + lr) % 8;
        namefresh = 0;
        redraw = 1;
    }
    if (ud) {
        if (namepos == 8) namepos = 7;
        namefresh = 0;
        for (i = 0; i <= namepos; ++i)
            if (!player[i])
                player[i] = ' ';
        for (n = 0; chars[n] && chars[n] != player[namepos]; ++n) {
        }
        if (!chars[n])
            n = 0;
        n = (n + 37 - ud) % 37;
        player[namepos] = chars[n];
        redraw = 1;
    }
    player[8] = 0;
}
static void tick(void) {
    int esc, enter, upedge, downedge, lr, ud, cw, ccw;
    if (joyenabled)
        joy_poll();
    else
        joy_off();
    upedge = joyup && !oldup;
    downedge = joydown && !olddown;
    oldup = joyup;
    olddown = joydown;
    esc = edge(1);
    enter = edge(28) || joyedge;
    if (state == NAME) {
        lr = repeat_dir((keys[77] || joyright) - (keys[75] || joyleft), &moveheld, &moveage);
        ud = repeat_dir((keys[80] || joydown) - (keys[72] || joyup), &uiheld, &uiage);
        name_tick(lr, ud, enter, esc);
        return;
    }
    if (edge(59)) {
        theme = (theme + 1) % 4;
        settheme();
        if (state != PLAY)
            redraw = 1;
    }
    if (edge(60)) {
        soundmode = (soundmode + 1) % 4;
        silence();
        if (state != PLAY)
            redraw = 1;
    }
    if (edge(37)) {
        joy_off();
        if (state != PLAY)
            redraw = 1;
    }
    if (edge(36)) {
        joy_calibrate();
        inputs_flush();
        return;
    }
    if (state == PLAY) {
        if (esc || edge(25)) {
            state = PAUSED;
            silence();
            redraw = 1;
            inputs_flush();
            return;
        }
        cw = edge(72) | edge(45) | edge(57) | upedge | enter;
        ccw = edge(44) | joyback;
        downedge |= edge(80);
        play_tick(cw, ccw, keys[80] || joydown, downedge);
    } else if (state == PAUSED) {
        if (esc || enter || edge(25)) {
            state = PLAY;
            redraw = resetclock = 1;
            inputs_flush();
        } else if (edge(16) || joyback) {
            state = MENU;
            redraw = 1;
            active = 0;
            clearwait = fxwait = 0;
            inputs_flush();
        }
    } else if (state == MENU) {
        if (esc || joyback) {
            quit = 1;
            return;
        }
        if (enter) {
            begin();
            return;
        }
        if (edge(35)) {
            tabletype = opttype;
            state = SCORES;
            resultshown = 0;
            redraw = 1;
            inputs_flush();
            return;
        }
        ud = repeat_dir((keys[80] || joydown) - (keys[72] || joyup), &uiheld, &uiage);
        if (ud) {
            menusel = (menusel + 6 + ud) % 6;
            redraw = 1;
        }
        lr = repeat_dir((keys[77] || joyright) - (keys[75] || joyleft), &moveheld, &moveage);
        if (lr)
            changemenu(lr);
    } else if (state == SCORES) {
        if (enter || esc || joyback) {
            state = MENU;
            redraw = 1;
            inputs_flush();
            return;
        }
        lr = repeat_dir((keys[77] || joyright) - (keys[75] || joyleft), &moveheld, &moveage);
        if (lr) {
            tabletype ^= 1;
            redraw = 1;
        }
    }
}
static void render(void) {
    if (redraw) {
        if (state == MENU)
            menu_draw();
        else if (state == NAME)
            name_draw();
        else if (state == SCORES)
            scores_draw();
        else {
            scene();
            if (state == PAUSED) {
                clearbox(180, 204, 280, 100);
                ink(15);
                rect(180, 204, 280, 100);
                centered(218, "PAUSED", 15);
                centered(249, "ENTER/P/ESC RESUMES", 15);
                centered(279, "Q OR BUTTON 2: MENU", 15);
            }
        }
        redraw = 0;
    } else if (state == PLAY && !fxwait) {
        board_render();
        hud();
    }
    ++frames;
}
#ifdef SELFTEST
static int testshow;
#include "TEST.H"
#endif
int main(int argc, char **argv) {
    int i, joylaunch = 0, override = -1;
#ifndef SELFTEST
    unsigned oldmode, last, now, elapsed;
#endif
    for (i = 1; i < argc; ++i) {
        if (!stricmp(argv[i], "/J"))
            joylaunch = 1;
        else if (!stricmp(argv[i], "/S"))
            override = 0;
        else if (!stricmp(argv[i], "/A"))
            override = 1;
        else if (!stricmp(argv[i], "/P"))
            override = 2;
        else if (!stricmp(argv[i], "/B"))
            override = 3;
#ifdef SELFTEST
        else if (!stricmp(argv[i], "/SHOW"))
            testshow = 1;
#endif
        else {
            puts("NUXTRIS 1.2\nNUXTRIS [/J] [/A | /P | /B | /S]\n/J "
                 "joystick calibration  /A AdLib  /P speaker  /B both  /S silent\nRun without "
                 "switches for menu. Scores/settings saved beside EXE.");
            return 0;
        }
    }
    paths(argv[0]);
    loadscores();
    if (override >= 0)
        soundmode = override;
#ifdef SELFTEST
    return selftests();
#else
    oldmode = modequery();
    if (!video_start()) {
        biosmode(oldmode);
        puts("VGA 640x480x16 mode could not be set.");
        return 1;
    }
    speakerbits = inp(0x61) & 3;
    audiofound = adlib();
    if (audiofound)
        instruments();
    if (!audiofound && soundmode == 1)
        soundmode = 2;
    rng = (uint16_t)(*(unsigned long __far *)MK_FP(0x40, 0x6c) ^ 0x7a31 ^ inp(0x40));
    if (!rng)
        rng = 1;
    install();
    if (joylaunch)
        joy_calibrate();
    last = ticks;
    render();
    last = ticks;
    while (!quit) {
        now = ticks;
        elapsed = (unsigned)(now - last);
        if (!elapsed)
            continue;
        last = now;
        if (elapsed > 3)
            elapsed = 3;
        while (elapsed-- && !quit) {
            audio_tick();
            tick();
            if (resetclock)
                break;
        }
        if (quit)
            break;
        render();
        if (resetclock) {
            last = ticks;
            resetclock = 0;
        }
    }
    silence();
    uninstall();
    outp(0x61, (inp(0x61) & 0xfc) | speakerbits);
    biosmode(oldmode);
    savescores();
    puts("NUXTRIS 1.2 - thanks for playing!");
    if (savefailed)
        printf("Could not save %s. New scores were not saved.\n", savepath);
    else
        printf("Scores and settings saved: %s\n", savepath);
    return savefailed ? 1 : 0;
#endif
}
