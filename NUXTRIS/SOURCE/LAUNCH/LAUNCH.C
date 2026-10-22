/* NuXTris game selector. 16-bit DOS, VGA 640x480x16. */
#include <conio.h>
#include <ctype.h>
#include <direct.h>
#include <dos.h>
#include <errno.h>
#include <io.h>
#include <malloc.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FONT.H"

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

static char game_dir[260], original_dir[260], game_file[260];
static unsigned original_drive, original_mode, selected;
static const char *const filenames[2] = {"SP_TRIS.EXE", "MP_TRIS.EXE"};
static const char *const choices[3] = {"SINGLE PLAYER", "MULTIPLAYER", "EXIT TO DOS"};

static unsigned glyph(char c, int row) {
    if (c == '>') {
        static const unsigned char arrow[7] = {128, 64, 32, 16, 32, 64, 128};
        return arrow[row];
    }
    if (c < 32 || c > 90) return 0;
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
static void centered(int y, const char *s, int color) {
    textxy((640 - (int)strlen(s) * 12) / 2, y, s, color, 2);
}
static void rect(int x, int y, int w, int h) {
    segment(x, y, x + w, y);
    segment(x + w, y, x + w, y + h);
    segment(x + w, y + h, x, y + h);
    segment(x, y + h, x, y);
}
static void biosmode(unsigned mode) {
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = mode;
    int86(0x10, &r, &r);
}
static unsigned modequery(void) {
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.h.ah = 15;
    int86(0x10, &r, &r);
    return r.h.al & 127;
}
static void dac(unsigned i, unsigned red, unsigned green, unsigned blue) {
    outp(0x3c8, i);
    outp(0x3c9, red);
    outp(0x3c9, green);
    outp(0x3c9, blue);
}
static int video_start(void) {
    union REGS r;
    int i;
    biosmode(0x12);
    if (modequery() != 0x12) return 0;
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
    for (i = 0; i < 16; ++i) dac(i, 0, 0, 0);
    dac(8, 12, 12, 12);
    dac(12, 63, 14, 10);
    dac(15, 58, 61, 63);
    video_init(80, 480);
    wipe();
    return 1;
}
/* Keep the title shape and menu spacing of single-player NuXTris. */
static void logo(int y) {
    int x = 202;
    ink(15);
    raster(0x10);
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
}
static void cursor(unsigned item) {
    raster(0x18);
    textxy(176, 150 + item * 58, ">", 15, 2);
}
static void menu_draw(const char *notice) {
    unsigned i;
    wipe();
    logo(26);
    ink(15);
    raster(0x10);
    rect(150, 102, 340, 244);
    for (i = 0; i < 3; ++i)
        textxy(210, 150 + i * 58, choices[i], 15, 2);
    centered(374, "UP/DOWN SELECT   ENTER CHOOSE", 15);
    centered(402, "ESC EXIT", 15);
    if (notice && *notice) centered(442, notice, 12);
    cursor(selected);
}
static int setup_paths(void) {
    char full[260], *slash;
    size_t length;
    if (!getcwd(original_dir, sizeof(original_dir))) return 0;
    original_drive = _getdrive();
    game_dir[0] = 0;
    if (_cmdname(full)) {
        slash = strrchr(full, '\\');
        if (slash) {
            length = slash - full + 1;
            if (length + 12 < sizeof(game_dir)) {
                memcpy(game_dir, full, length);
                game_dir[length] = 0;
            }
        }
    }
    return 1;
}
static int enter_game_dir(void) {
    char dir[260];
    size_t length = strlen(game_dir);
#ifdef LAUNCH_TEST
    if (getenv("LAUNCHTEST_CHDIR_FAIL")) return 0;
#endif
    strcpy(dir, game_dir);
    /* DOS CHDIR rejects a trailing backslash on some non-root paths. */
    if (length > 3 && dir[length - 1] == '\\') dir[length - 1] = 0;
    if (game_dir[1] == ':' && _chdrive(toupper(game_dir[0]) - 'A' + 1)) return 0;
    return chdir(dir) == 0;
}
static void restore_dir(void) {
    _chdrive(original_drive);
    chdir(original_dir);
}
static const char *run_game(unsigned item) {
    const char *args[3];
    const char *selected_dir = game_dir;
    int result;
    game_file[0] = 0;
    if (game_dir[0]) {
        strcpy(game_file, game_dir);
        strcat(game_file, filenames[item]);
    }
    if (!game_dir[0] || access(game_file, 0)) {
        /* DOS launchers may not supply a usable executable path. */
        if (access(filenames[item], 0))
            return item ? "MP_TRIS.EXE MISSING" : "SP_TRIS.EXE MISSING";
        selected_dir = original_dir;
        strcpy(game_file, filenames[item]);
    }
    biosmode(original_mode);
    if (selected_dir == game_dir && !enter_game_dir()) {
        restore_dir();
        if (access(filenames[item], 0)) {
            video_start();
            return "CANNOT OPEN GAME FOLDER";
        }
        /* The child is in the current folder, so no CHDIR is needed. */
        strcpy(game_file, filenames[item]);
    }
    args[0] = game_file;
#ifdef LAUNCH_TEST
    if ((item == 0 && getenv("LAUNCHTEST_SP")) ||
        (item == 1 && getenv("LAUNCHTEST_MP")))
        args[1] = NULL; /* Test builds exercise complete startup and exit. */
    else {
        args[1] = "/?"; /* Release games return after printing help. */
        args[2] = NULL;
    }
#else
    args[1] = NULL;
#endif
    _heapshrink();
    result = spawnv(P_WAIT, game_file, args);
    restore_dir();
    if (!video_start()) {
        biosmode(original_mode);
        puts("VGA 640x480x16 mode could not be restored.");
        exit(1);
    }
    while (kbhit()) (void)getch(); /* Discard the child's final keypress. */
    if (result < 0)
        return item ? "CANNOT START MP_TRIS.EXE" : "CANNOT START SP_TRIS.EXE";
    return NULL;
}
static unsigned readkey(void) {
    union REGS r;
    memset(&r, 0, sizeof(r));
    int86(0x16, &r, &r);
    return r.w.ax;
}
#ifdef LAUNCH_TEST
static void capture_screen(const char *path) {
    FILE *f = fopen(path, "wb");
    volatile unsigned char __far *v = MK_FP(0xa000, 0);
    unsigned p, y, x;
    unsigned char row[80];
    if (!f) return;
    for (p = 0; p < 4; ++p) {
        outpw(0x3ce, (p << 8) | 4);
        for (y = 0; y < 480; ++y) {
            for (x = 0; x < 80; ++x) row[x] = v[y * 80 + x];
            fwrite(row, 1, 80, f);
        }
    }
    fclose(f);
    outpw(0x3ce, 4);
}
static int selftest(void) {
    FILE *f;
    int failed = 0, mp_present;
    const char *notice;
    if (game_dir[0]) {
        strcpy(game_file, game_dir);
        strcat(game_file, filenames[1]);
    } else game_file[0] = 0;
    mp_present = (game_dir[0] && access(game_file, 0) == 0) ||
                 access(filenames[1], 0) == 0;
    capture_screen("MENU.RAW");
    cursor(selected);
    selected = 1;
    cursor(selected);
    capture_screen("CURSOR.RAW");
    selected = 0;
    menu_draw(NULL);
    notice = run_game(0);
    if (notice || modequery() != 0x12) ++failed;
    menu_draw(notice);
    capture_screen("BACKSP.RAW");
    notice = run_game(1);
    if (mp_present) {
        if (notice || modequery() != 0x12) ++failed;
    } else if (!notice || strcmp(notice, "MP_TRIS.EXE MISSING")) ++failed;
    menu_draw(notice);
    capture_screen("BACKMP.RAW");
    biosmode(original_mode);
    restore_dir();
    f = fopen("CHECK.TXT", "wt");
    if (f) {
        fprintf(f, "SP return: %s\nMP: %s\nFailed: %d\n",
                access("BACKSP.RAW", 0) ? "missing capture" : "captured",
                mp_present ? "launched" : "missing file shown", failed);
        fclose(f);
    } else ++failed;
    return failed ? 1 : 0;
}
#endif
int main(void) {
    unsigned key, scan, ascii;
    const char *notice;
    if (!setup_paths()) {
        puts("Cannot locate NUXTRIS.EXE or its folder.");
        return 1;
    }
    original_mode = modequery();
    if (!video_start()) {
        biosmode(original_mode);
        puts("VGA 640x480x16 mode could not be set.");
        return 1;
    }
    menu_draw(NULL);
#ifdef LAUNCH_TEST
    return selftest();
#endif
    for (;;) {
        key = readkey();
        scan = key >> 8;
        ascii = key & 255;
        if (scan == 72 || ascii == 'w' || ascii == 'W') {
            cursor(selected);
            selected = (selected + 2) % 3;
            cursor(selected);
        } else if (scan == 80 || ascii == 's' || ascii == 'S') {
            cursor(selected);
            selected = (selected + 1) % 3;
            cursor(selected);
        } else if (ascii == 27) {
            break;
        } else if (ascii == 13) {
            if (selected == 2) break;
            notice = run_game(selected);
            menu_draw(notice);
        }
    }
    biosmode(original_mode);
    restore_dir();
    return 0;
}
