/* Build with BUILD.SH --test. Run CHECK.EXE only in a disposable test folder. */
#include "RULETEST.H"
static unsigned long planehash(void) {
    volatile unsigned char __far *v = MK_FP(0xa000, 0);
    unsigned i, p;
    unsigned long h = 2166136261UL;
    for (p = 0; p < 4; ++p) {
        outpw(0x3ce, (p << 8) | 4);
        for (i = 0; i < 38400U; ++i) {
            h ^= v[i];
            h *= 16777619UL;
        }
    }
    outpw(0x3ce, 4);
    return h;
}
static void capture_screen(const char *path) {
    volatile unsigned char __far *v = MK_FP(0xa000, 0);
    FILE *f = fopen(path, "wb");
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
static void showcase(void) {
    unsigned start, last, t, elapsed;
    int stage = 0, i, y, x, typed = 0;
    static const unsigned char namekeys[5] = {30, 50, 23, 34, 24}; /* AMIGO */
    paths("CHECK.EXE");
    memset(scores, 0, sizeof(scores));
    strcpy(player, "PLAYER");
    video_start();
    theme = 3;
    settheme();
    soundmode = 3;
    audiofound = adlib();
    if (audiofound)
        instruments();
    speakerbits = inp(0x61) & 3;
    install();
    start = last = ticks;
    state = MENU;
    redraw = 1;
    render();
    while ((unsigned)(ticks - start) < 620) {
        t = ticks;
        if (t == last)
            continue;
        last = t;
        elapsed = (unsigned)(t - start);
        if (stage == 0 && elapsed >= 72) {
            opttype = BTYPE;
            optlevel = 3;
            optheight = 4;
            begin();
            stage = 1;
        }
        if (stage == 1 && elapsed >= 180) {
            opttype = optlevel = optheight = 0;
            begin();
            memset(game.grid, 0, sizeof(game.grid));
            for (y = 16; y < 20; ++y)
                for (x = 0; x < 10; ++x)
                    if (x != 4)
                        game.grid[y][x] = (x + y) % 7 + 1;
            for (i = 0; i < 4; ++i) {
                game.shape[i * 2] = 0;
                game.shape[i * 2 + 1] = i - 1;
            }
            game.kind = 4;
            game.x = 4;
            game.y = 13;
            keys[80] = 1;
            makes[80] = 1;
            stage = 2;
        }
        if (stage == 2 && elapsed >= 320) {
            keys[80] = 0;
            stage = 3;
        }
        if (stage == 3 && elapsed >= 360) {
            opttype = SPRINT;
            optlevel = 2;
            optheight = 0;
            begin();
            game.lines = 39;
            memset(game.grid[19], 3, BW);
            game.grid[19][4] = game.grid[19][5] = 0;
            memcpy(game.shape, shapes[0], sizeof(game.shape));
            game.kind = 0;
            game.x = 4;
            game.y = 18;
            keys[80] = 1;
            makes[80] = 1;
            stage = 4;
        }
        if (stage == 4 && state == NAME) {
            keys[80] = 0;
            stage = 5;
        }
        if (stage == 5 && elapsed >= 440 && typed < 5) {
            makes[namekeys[typed++]] = 1;
        } else if (stage == 5 && typed == 5 && elapsed >= 500) {
            makes[28] = 1;
            stage = 6;
        }
        audio_tick();
        tick();
        render();
        resetclock = 0;
    }
    silence();
    uninstall();
    outp(0x61, (inp(0x61) & 0xfc) | speakerbits);
    biosmode(3);
}
static int hud_pixels(int n, int x, int y, const char *s) {
    static unsigned char saved[4][14][36];
    volatile unsigned char __far *v = MK_FP(0xa000, 0);
    int len = strlen(oldhud[n]), len2 = strlen(s), bytes, row, col, p, ok = 1;
    unsigned offset;
    if (len2 > len) len = len2;
    if (!len) return 1;
    bytes = ((x & 7) + len * 12 + 7) / 8;
    if (bytes > 36) return 0;
    hudline(n, x, y, s);
    for (p = 0; p < 4; ++p) {
        outpw(0x3ce, (p << 8) | 4);
        for (row = 0; row < 14; ++row) {
            offset = (y + row) * 80 + (x >> 3);
            for (col = 0; col < bytes; ++col) saved[p][row][col] = v[offset + col];
        }
    }
    clearbox(x, y, len * 12, 14);
    textxy(x, y, s, 15, 2);
    for (p = 0; p < 4; ++p) {
        outpw(0x3ce, (p << 8) | 4);
        for (row = 0; row < 14; ++row) {
            offset = (y + row) * 80 + (x >> 3);
            for (col = 0; col < bytes; ++col)
                if (saved[p][row][col] != v[offset + col]) ok = 0;
        }
    }
    outpw(0x3ce, 4);
    return ok;
}
static void hud_checks(void) {
    static const uint32_t values[] = {0, 9, 10, 99, 100, 999, 1000, 9999,
        65535UL, 99999999UL, 100000000UL, 999999999UL};
    char s[24], expected[24];
    int i, j, ok = 1, x, y;
    unsigned gc, fc;
    unsigned long blank;
    for (i = 0; i < 12; ++i) {
        hudnumber(s, values[i], 9); sprintf(expected, "%09lu", (unsigned long)values[i]);
        if (strcmp(s, expected)) ok = 0;
        hudnumber(s, values[i], 2); sprintf(expected, "%02lu", (unsigned long)values[i]);
        if (strcmp(s, expected)) ok = 0;
        hudnumber(s, values[i], 3); sprintf(expected, "%03lu", (unsigned long)values[i]);
        if (strcmp(s, expected)) ok = 0;
    }
    check(ok, "HUD decimal formatting matches all field widths, carries and score cap");
    wipe(); memset(oldhud, 0, sizeof(oldhud)); blank = planehash(); ok = 1;
    for (i = 32; i <= 95; ++i) {
        for (j = 0; j < 9; ++j) s[j] = 32 + (i + j - 32) % 64;
        s[9] = 0;
        if (!hud_pixels(0, 36, 126, s) || !hud_pixels(1, 464, 328, s)) ok = 0;
    }
    check(ok, "Glyph-delta HUD matches all four VGA planes at both cell alignments");
    check(hud_pixels(0, 36, 126, "000000009") && hud_pixels(0, 36, 126, "000000010") &&
          hud_pixels(0, 36, 126, "999999999"), "Score digit carries render without stale pixels");
    check(hud_pixels(1, 464, 328, "SPEAKER") && hud_pixels(1, 464, 328, "BOTH"),
          "Shorter HUD values remove the old suffix");
    hudline(0, 36, 126, ""); hudline(1, 464, 328, "");
    check(planehash() == blank, "HUD updates and erasure preserve all surrounding pixels");
    opttype = optlevel = optheight = 0;
    begin(); scene(); gc = hudglyphs; fc = hudformats;
    for (i = 0; i < 6; ++i) { play_tick(0, 0, 1, i == 0); hud(); }
    check(game.score > 0 && settledscore == 0 && hudscore == 0,
          "Soft-drop points accumulate internally while displayed score waits for landing");
    check(hudglyphs == gc && hudformats == fc,
          "Soft drop does no HUD formatting or glyph drawing before landing");
    state = PAUSED; scene();
    check(hudscore == 0 && !strcmp(oldhud[0], "000000000"),
          "Pause/redraw retains the committed score while a piece is falling");
    state = PLAY;
    while (drop_one(1)) {}
    play_tick(0, 0, 1, 1); hud();
    check(settledscore == game.score && hudscore == game.score && hudscore > 0,
          "Landing commits all soft-drop points even when the next piece spawns immediately");
    gc = hudglyphs; fc = hudformats; hud();
    check(gc == hudglyphs && fc == hudformats, "Unchanged HUD does no formatting or glyph writes");
    begin(); game.lines = 9;
    memset(game.grid[19], 3, BW); game.grid[19][4] = game.grid[19][5] = 0;
    memcpy(game.shape, shapes[0], sizeof(game.shape)); game.kind = 0; game.x = 4; game.y = 18;
    play_tick(0, 0, 1, 1);
    for (i = 0; i < 8; ++i) play_tick(0, 0, 0, 0);
    hud();
    check(game.score == 40 && hudscore == 40 && hudlines == 10 && hudlevel == 1,
          "Completed line clear commits bonus, line count and new level together");
    begin();
    for (y = 16; y < 20; ++y) for (x = 0; x < 10; ++x)
        if (x != 4) game.grid[y][x] = 2;
    for (i = 0; i < 4; ++i) { game.shape[i*2] = 0; game.shape[i*2+1] = i - 1; }
    game.kind = 4; game.x = 4; game.y = 17;
    play_tick(0, 0, 1, 1);
    for (i = 0; i < 8; ++i) play_tick(0, 0, 0, 0);
    render();
    check(fxwait && game.score == 1200 && hudscore == 1200,
          "Tetris celebration shows the complete four-line bonus immediately");
    opttype = SPRINT; begin(); game.lines = 39;
    memset(game.grid[19], 3, BW); game.grid[19][4] = game.grid[19][5] = 0;
    memcpy(game.shape, shapes[0], sizeof(game.shape)); game.kind = 0; game.x = 4; game.y = 18;
    play_tick(0, 0, 1, 1);
    for (i = 0; i < 8; ++i) play_tick(0, 0, 0, 0);
    check(won && settledscore == game.score && game.score == 40 && state != PLAY,
          "Sprint result retains the full final score");
    opttype = 0; begin();
}
#include "UITEST.H"
static void mode_checks(void) {
    unsigned char old[SAVESIZE];
    unsigned c, x, i;
    FILE *f;
    char ts[8];
    memset(scores, 0, sizeof(scores));
    opttype = MARATHON; optlevel = optheight = 0;
    begin(); game.lines = 9;
    memset(game.grid[19], 1, BW); game.full = 1UL << 19;
    finish_clear();
    check(game.level == 1 && game.lines == 10,
          "Marathon raises speed on the tenth cleared line");
    opttype = SPRINT; optlevel = 4; begin(); game.lines = 39;
    memset(game.grid[19], 1, BW); game.full = 1UL << 19;
    finish_clear();
    check(game.lines == 40 && game.level == 4,
          "Sprint reaches forty lines without changing its chosen speed");
    roundticks = 100; next_piece();
    check(won && state == NAME,
          "Sprint completes at forty lines and opens results");
    name_draw(); capture_screen("SPRINT.RAW");
    opttype = SPRINT; begin();
    game.y = -3; lock_shape(); endround();
    check(game.over && !won && scoreplace(SPRINT, 100, 200, 0) == -1,
          "Sprint top-out is a loss and does not enter the best-times table");
    opttype = BTYPE; optlevel = 0; optheight = 3; begin();
    scene(); capture_screen("BTYPE.RAW");
    for (i = 0; i < 14; ++i) {
        unsigned filled = 0;
        for (x = 0; x < BW; ++x) filled += game.grid[i][x] != 0;
        if (filled) break;
    }
    check(i == 14, "B-Type height three leaves the upper fourteen rows clear");
    game.lines = 24;
    memset(game.grid[19], 1, BW); game.full = 1UL << 19;
    finish_clear();
    check(game.lines == 25 && game.level == 1,
          "B-Type advances at twenty-five lines and keeps playing");
    next_piece();
    check(state == PLAY && !won,
          "B-Type does not end at the original Game Boy target");
    game.lines = 249;
    memset(game.grid[19], 1, BW); game.full = 1UL << 19;
    finish_clear();
    check(game.lines == 250 && game.level == 9,
          "B-Type caps its twenty-five-line stages at level nine");
    time_text(ts, 182U * 13U);
    check(!strcmp(ts, "01:05"), "Active-play timer displays minutes and seconds");
    opttype = MARATHON; begin(); fxwait = 2;
    play_tick(0, 0, 0, 0);
    check(roundticks == 0 && elapsedseconds == 0,
          "The original TETRIS! hold does not advance the game timer");
    fxwait = 0; play_tick(0, 0, 0, 0);
    check(roundticks == 1 && timefraction == 5,
          "Active play advances the timer without a per-frame division");
    memset(scores, 0, sizeof(scores));
    game.type = SPRINT; game.score = 200;
    recordscore(0, 1, 4000);
    game.score = 100;
    recordscore(scoreplace(SPRINT, 100, 3000, 1), 1, 3000);
    check(scores[SPRINT][0].time == 3000 && scores[SPRINT][1].time == 4000,
          "Completed Sprint records rank by fastest time");
    tabletype = SPRINT; resultshown = 0;
    scores_draw(); capture_screen("TIMES.RAW");
    memset(old, 0, OLDSAVESIZE);
    memcpy(old, "NXTRIS01", 8);
    old[8] = 1; old[9] = 3; old[10] = 2; old[11] = 2; old[12] = 3;
    memcpy(old + 13, "OLDNAME", 7);
    puts32(old + 24, 100); memcpy(old + 28, "ALICE", 5);
    puts32(old + 184, 200); memcpy(old + 188, "BOB", 3);
    old[196] = 3; old[197] = 2; old[198] = 1;
    c = checksum(old, OLDSAVESIZE - 2);
    old[OLDSAVESIZE - 2] = (unsigned char)c;
    old[OLDSAVESIZE - 1] = (unsigned char)(c >> 8);
    strcpy(savepath, "TSTLEG.DAT"); strcpy(backpath, "TSTLEG.BAK");
    strcpy(newpath, "TSTLEG.NEW");
    remove(savepath); remove(backpath); remove(newpath);
    f = fopen(savepath, "wb");
    if (f) { fwrite(old, 1, OLDSAVESIZE, f); fclose(f); }
    memset(scores, 0, sizeof(scores));
    loadscores();
    check(imported && opttype == BTYPE && optlevel == 3 && optheight == 2 &&
          scores[MARATHON][0].score == 100 &&
          scores[LEGACY_B][0].score == 200 && scores[BTYPE][0].score == 0,
          "Old Type A and B records import into separate preserved tables");
    check(savescores() && readsave(savepath, old) == 2,
          "Imported scores save in the new format");
    memset(scores, 0, sizeof(scores)); loadscores();
    check(scores[LEGACY_B][0].score == 200 &&
          scores[MARATHON][0].score == 100 && !imported,
          "Legacy scores survive a new-format reload");
    tabletype = LEGACY_B; resultshown = 0;
    scores_draw(); capture_screen("LEGACY.RAW");
    opttype = BTYPE; state = MENU; wipe(); menu_draw();
    capture_screen("MENU.RAW");
    remove(savepath); remove(backpath); remove(newpath);
    memset(scores, 0, sizeof(scores));
    imported = recovered = savefailed = 0;
    strcpy(player, "PLAYER");
}
static int selftests(void) {
    int x, y, i;
    unsigned long hash, blank;
    unsigned t;
    void(__interrupt __far * v0)(void), (__interrupt __far * v1)(void);
    report = fopen("CHECKS.TXT", "w");
    if (!report)
        return 2;
    rule_tests();
    save_tests();
    soundmode = 0;
    theme = 0;
    opttype = optlevel = optheight = 0;
    if (!video_start()) {
        fputs("FAIL VGA unavailable\n", report);
        fclose(report);
        return 2;
    }
    ui_checks();
    hud_checks();
    mode_checks();
    wipe();
    blank = planehash();
    block(100, 100, 5);
    block(100, 100, 5);
    check(planehash() == blank, "Wireframe XOR block erases without leftover pixels");
    raster(0x10);
    ink(7);
    rect(80, 80, 80, 80);
    raster(0x18);
    clearbox(79, 79, 82, 82);
    check(planehash() == blank, "Unaligned rectangle clearing preserves a clean four-plane screen");
    ink(15);
    dot(602, 100);
    hash = planehash();
    dot(500, 100);
    clearbox(480, 100, 122, 1);
    check(planehash() == hash,
          "Masked rectangle clear loads latches and preserves neighboring pixels");
    begin();
    scene();
    hash = planehash();
    game.y += 1;
    board_render();
    game.y -= 1;
    board_render();
    check(planehash() == hash, "Moving a piece down and back leaves no ghosts");
    theme = 3;
    settheme();
    check(planehash() == hash, "Theme switch updates only DAC entries, never video pixels");
    game.y = 5;
    scene();
    hash = planehash();
    for (i = 0; i < 4; ++i) {
        rotate_shape(1);
        board_render();
    }
    check(planehash() == hash, "Rotating a piece through four states restores identical pixels");
    begin();
    y = game.y;
    play_tick(0, 0, 1, 1);
    check(game.y == y + 1 && game.score == 1,
          "Down press moves one cell immediately, never hard-drops");
    play_tick(0, 0, 1, 0);
    check(game.y == y + 1, "Soft drop observes its two-tick interval");
    play_tick(0, 0, 1, 0);
    check(game.y == y + 2, "Holding down repeats soft drop at the fixed interval");
    moveheld = moveage = 0;
    x = 0;
    for (i = 0; i < 7; ++i)
        if (repeat_dir(1, &moveheld, &moveage))
            ++x;
    check(x == 1 && repeat_dir(1, &moveheld, &moveage) == 1,
          "Horizontal repeat has an initial delay before repeating");
    begin();
    active = 0;
    game.grid[18][3] = 4;
    game.grid[19][7] = 6;
    scene();
    hash = planehash();
    fxwait = 54;
    scene();
    check(planehash() != hash, "Four-line background celebration draws into the scene");
    fxwait = 0;
    scene();
    check(planehash() == hash, "Celebration cleanup restores exact gameplay pixels");
    v0 = _dos_getvect(8);
    v1 = _dos_getvect(9);
    install();
    t = ticks;
    while ((unsigned)(ticks - t) < 4) {
    }
    check(ticks != t, "Fixed-rate game clock advances");
    uninstall();
    check(_dos_getvect(8) == v0 && _dos_getvect(9) == v1,
          "Timer and keyboard interrupt vectors restored on exit");
    biosmode(3);
    fprintf(report, "\n%d passed, %d failed\n", passed, failed);
    fclose(report);
    printf("NUXTRIS: %d passed, %d failed. See CHECKS.TXT\n", passed, failed);
    if (testshow)
        showcase();
    return failed ? 1 : 0;
}
