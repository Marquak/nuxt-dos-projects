/* Compile GAME.C with -dSELFTEST to run these checks inside DOS. */
static int passed, failed;
static FILE *report;
static void check(int condition, const char *label) {
    fprintf(report, "%s %s\n", condition ? "PASS" : "FAIL", label);
    if (condition)
        ++passed;
    else
        ++failed;
}
static unsigned long planehash(void) {
    volatile unsigned char __far *v = MK_FP(0xa000, 0);
    unsigned i, p;
    unsigned long h = 2166136261UL;
    for (p = 0; p < 4; p++) {
        outpw(0x3ce, (p << 8) | 4);
        for (i = hud * (width / 8); i < (unsigned)(height * (width / 8)); i++) {
            h ^= v[i];
            h *= 16777619UL;
        }
    }
    outpw(0x3ce, 4);
    return h;
}
static void reference_line(int x, int y, int xx, int yy) {
    int dx = abs(xx - x), dy = abs(yy - y), sx = x < xx ? 1 : -1, sy = y < yy ? 1 : -1,
        n = dx > dy ? dx : dy, e = n / 2, i;
    if (!n)
        n = 1;
    for (i = 0; i < n; i++) {
        dot(x, y);
        if (dx >= dy) {
            x += sx;
            e -= dy;
            if (e < 0) {
                y += sy;
                e += dx;
            }
        } else {
            y += sy;
            e -= dx;
            if (e < 0) {
                x += sx;
                e += dy;
            }
        }
    }
}
static void reference_wrapped(int x, int y, int xx, int yy) {
    int dx = abs(xx - x), dy = abs(yy - y), sx = x < xx ? 1 : -1, sy = y < yy ? 1 : -1,
        n = dx > dy ? dx : dy, e = n / 2, i;
    if (!n)
        n = 1;
    for (i = 0; i < n; i++) {
        dot(wrap(x,width), wrap(y,height-hud)+hud);
        if (dx >= dy) {
            x += sx;
            e -= dy;
            if (e < 0) {
                y += sy;
                e += dx;
            }
        } else {
            y += sy;
            e -= dx;
            if (e < 0) {
                x += sx;
                e += dy;
            }
        }
    }
}

/* Compare every HUD byte in every plane with the old independent pixel
   renderer. Also covers the gap pixels and unused remainder of each row. */
static int hud_matches_pixels(const char *top, const char *bottom) {
    volatile unsigned char __far *v = MK_FP(0xa000, 0);
    unsigned bytes = hud * (width / 8), p, i;
    unsigned char *saved = malloc(bytes * 4);
    int same = 1;
    if (!saved) return 0;
    for (p = 0; p < 4; p++) {
        outpw(0x3ce, (p << 8) | 4);
        for (i = 0; i < bytes; i++) saved[p * bytes + i] = v[i];
    }
    ink(15);
    raster(0);
    for (i = 0; i < bytes; i++) v[i] = 0;
    raster(0x18);
    textxy(2, 0, top, 12);
    textxy(2, 8 * scale, bottom, 12);
    for (p = 0; p < 4; p++) {
        outpw(0x3ce, (p << 8) | 4);
        for (i = 0; i < bytes; i++)
            if (saved[p * bytes + i] != v[i]) same = 0;
    }
    outpw(0x3ce, 4);
    free(saved);
    return same;
}
static void hud_checks(void) {
    static const unsigned long numbers[] = {0, 9, 10, 99, 100, 999, 1000,
        9999, 10000, 65535, 99990, 99999};
    char top[80], bottom[80], got[80], want[80];
    unsigned i, j;
    unsigned long bg;
    int ok = 1;
    for (i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
        hud_number(got, numbers[i], 1);
        sprintf(want, "%05lu", numbers[i]);
        if (strcmp(got, want)) ok = 0;
        hud_number(got, numbers[i], 0);
        sprintf(want, "%lu", numbers[i]);
        if (strcmp(got, want)) ok = 0;
    }
    check(ok, "Bounded number formatting matches decimal carries and limits");
    wipe();
    memset(oldhud, 0, sizeof(oldhud));
    bg = planehash();
    ok = 1;
    for (i = 0; i < 24; i++) {
        score = best = numbers[i % 12];
        wave = i + 1;
        lives = 3;
        started = 1; paused = gameover = dead = muted = 0; audiofound = 1;
        hud_update();
        sprintf(top, "%05lu BEST %05lu  WAVE %u", score, best, wave);
        strcpy(bottom, "LIVES 3  ADLIB");
        if (!hud_matches_pixels(top, bottom)) ok = 0;
    }
    check(ok, "Score, best and wave updates match all four reference HUD planes");
    hud_update();
    check(hud_matches_pixels(top, bottom), "Unchanged HUD remains pixel identical");
    ok = 1;
    for (i = 0; i < 7; i++) {
        started = i != 0; paused = i == 1; gameover = i == 2;
        dead = i == 3; muted = i == 4; audiofound = i != 5;
        lives = 5 - (i % 6);
        hud_update();
        sprintf(bottom, "LIVES %u  %s", lives, i == 0 ? "SPACE TO START" :
            i == 1 ? "PAUSED" : i == 2 ? "GAME OVER - SPACE" :
            i == 3 ? "RESPAWNING" : i < 6 ? "SOUND OFF" : "ADLIB");
        if (!hud_matches_pixels(top, bottom)) ok = 0;
    }
    check(ok, "Shorter status messages erase their old tail without disturbing neighbours");
    ok = 1;
    /* Exercise every font glyph at each HUD alignment, including blanks. */
    for (i = 32; i <= 90; i++) {
        for (j = 0; j < 8; j++) top[j] = (char)(32 + (i + j - 32) % 59);
        top[8] = 0;
        hud_text(0, top);
        if (!hud_matches_pixels(top, bottom)) ok = 0;
    }
    check(ok, "Every HUD glyph matches pixel renderer at all byte alignments");
    hud_text(0, ""); hud_text(1, "");
    check(hud_matches_pixels("", ""), "HUD can erase to empty without leftover pixels");
    check(planehash() == bg, "HUD updates leave every playfield plane untouched");
    audiofound = 0; muted = 0;
}

/* Optional controlled old/new hit-HUD timing. Same sequence, fixed DOSBox
   cycles, BIOS ticks. This measures only HUD work, not overall game FPS. */
static void hud_benchmark(void) {
    volatile unsigned long __far *clock = MK_FP(0x40, 0x6c);
    unsigned long begin, elapsed[2];
    unsigned pass, i;
    char b[80];
    for (pass = 0; pass < 2; pass++) {
        wipe(); memset(oldhud, 0, sizeof(oldhud));
        lives = 3; wave = 1; started = audiofound = 1;
        paused = gameover = dead = muted = 0;
        score = best = 0;
        hud_update();
        begin = *clock;
        while (*clock == begin) {}
        begin = *clock;
        for (i = 1; i <= 300; i++) {
            score = best = i * 20UL;
            if (pass) hud_update();
            else {
                sprintf(b, "%05lu BEST %05lu  WAVE %u", score, best, wave);
                textxy(2, 0, oldhud[0], 12);
                textxy(2, 0, b, 12);
                strcpy(oldhud[0], b);
            }
        }
        elapsed[pass] = *clock - begin;
    }
    fprintf(report, "HUD_BENCH 300 score+best updates: old=%lu new=%lu BIOS ticks\n",
            elapsed[0], elapsed[1]);
    audiofound = 0;
}

/* Fixed-work benchmark. BIOS ticks at fixed emulator cycles, not game FPS. */
static void play_benchmark(void) {
    volatile unsigned long __far *clock = MK_FP(0x40, 0x6c);
    unsigned long begin, elapsed;
    unsigned i, pass, j;
    Picture p;
    nofpu=1; geometry(); audiofound=0;
    memset(&p,0,sizeof(p)); p.on=1; p.color=12; p.shape=6;
    for(pass=0;pass<2;pass++) {
        wipe(); p.x=pass ? 0 : width/2; p.y=pass ? 0 : (height-hud)/2;
        begin=*clock; while(*clock==begin) {} begin=*clock;
        for(i=0;i<400;i++) { p.angle=i&31; picture(&p); picture(&p); }
        elapsed=*clock-begin;
        fprintf(report,"PICTURE %s: 400 draw/erase pairs %lu BIOS ticks\n",pass?"corner":"interior",elapsed);
    }
    for(pass=0;pass<2;pass++) {
        start_game(); audiofound=pass; muted=0;
        if(audiofound) instruments();
        remaining=16; protect=32760;
        for(j=0;j<16;j++) {
            rocks[j].on=1; rocks[j].size=j%3; rocks[j].variant=j%3;
            rocks[j].x=(j*23%worldw)*64; rocks[j].y=(j*37%worldh)*64;
            rocks[j].vx=43; rocks[j].vy=-31; rocks[j].spin=1;
            rocks[j].angle=j; rocks[j].grace=255;
        }
        for(j=0;j<SPARKS;j++) { sparks[j].life=255; sparks[j].x=j*11*64; sparks[j].y=j*9*64; sparks[j].vx=50; sparks[j].vy=30; }
        keys[57]=1; keys[72]=1; keys[77]=1;
        render();
        begin=*clock; while(*clock==begin) {} begin=*clock;
        for(i=0;i<160;i++) { logic(); render(); }
        elapsed=*clock-begin;
        fprintf(report,"PLAY %s: 160 logic+render, 16 rocks/fire/thrust/sparks %lu BIOS ticks\n",pass?"AdLib":"silent",elapsed);
        silence(); keys[57]=keys[72]=keys[77]=0;
    }
    begin=0;
    for(i=0;i<9;i++) for(j=0;j<32;j++) if(outline_cache[i]) begin+=outline_count[i][j]*4UL;
    fprintf(report,"Outline cache payload: %lu bytes\n",begin);
    audiofound=0;
}


static int screen_empty(void) {
    volatile unsigned char __far *v=MK_FP(0xa000,0);
    unsigned p,i;
    int good=1;
    for(p=0;p<4;p++) {
        outpw(0x3ce,(p<<8)|4);
        for(i=0;i<(unsigned)(height*(width>>3));i++) if(v[i]) good=0;
    }
    outpw(0x3ce,4);
    return good;
}
static void renderer_checks(void) {
    int edge, i, x, y, xx, yy, shape, a, shift, j;
    Picture p;
    unsigned __far *saved;
    char label[80];
    wipe(); ink(15);
    for(edge=0;edge<9;edge++) {
        for(i=0;i<160;i++) {
            x=(edge%3)*(width/2)+(int)(random16()%81)-40;
            y=(edge/3)*((height-hud)/2)+(int)(random16()%81)-40;
            xx=x+(int)(random16()%81)-40;
            yy=y+(int)(random16()%81)-40;
            if(!i) xx=x, yy=y;
            line(x,y,xx,yy); reference_wrapped(x,y,xx,yy);
        }
        check(screen_empty(),"Seam/interior lines match reference including HUD and all four planes");
        wipe(); ink(15);
    }
    memset(&p,0,sizeof(p)); p.on=1;
    for(shape=0;shape<9;shape++) {
        check(outline_cache[shape]!=0,"Asteroid mask cache allocated");
        p.shape=shape;
        for(a=0;a<32;a++) for(shift=0;shift<8;shift++) {
            p.x=width/2+shift; p.y=(height-hud)/2;
            p.angle=a; p.color=1+(a%15);
            picture(&p);
            saved=outline_cache[shape]; outline_cache[shape]=0;
            picture(&p); outline_cache[shape]=saved;
        }
        sprintf(label,"Cached shape %d: every rotation/alignment matches assembly outlines",shape);
        check(screen_empty(),label); wipe();
    }
    /* Exercise the exact fast-path boundaries and nearby wrapping objects. */
    p.color=15;
    for(i=0;i<8;i++) for(j=0;j<8;j++) {
        static const int border[8]={-1,0,1,31,32,33,63,64};
        p.x=(i&1)?width-border[i]:border[i];
        p.y=(j&1)?height-hud-border[j]:border[j];
        p.x=wrap(p.x,width); p.y=wrap(p.y,height-hud);
        for(shape=0;shape<9;shape++) {
            p.shape=shape; p.angle=(i*8+j)&31;
            picture(&p);
            for(a=0;a<8;a++) {
                Point *v=shapes[shape][p.angle]; int b=(a+1)&7;
                reference_wrapped(p.x+v[a].x,p.y+v[a].y,p.x+v[b].x,p.y+v[b].y);
            }
        }
    }
    check(screen_empty(),"Cached/fallback boundary positions match reference and never touch HUD");
    wipe();
}

int main(int argc, char **argv) {
    int mode = argc > 1 ? atoi(argv[1]) : 1, i, j, a, b, c, d, valid, prior;
    unsigned long blank, bg;
    Point __far *copy;
    unsigned t;
    void(__interrupt __far * v0)(void), (__interrupt __far * v1)(void);
    report = fopen(argc > 2 && !strcmp(argv[2], "play") ? "PERFRUN.TXT" : "CHECKS.TXT", "w");
    if (!report)
        return 2;
    fprintf(report, "NuXTroids DOS self-test mode %d\n", mode);
    if (!video_start(mode)) {
        fprintf(report, "Requested mode unavailable\n");
        fclose(report);
        biosmode(3);
        return 2;
    }
    if (argc > 2) {
        if (!strcmp(argv[2], "play")) play_benchmark();
        else hud_benchmark();
        biosmode(3); fclose(report); return 0;
    }
    hud_checks();
    check(worldw * 64 < 32767 && worldh * 64 < 32767, "Q10.6 world dimensions fit signed words");
    nofpu = 1;
    geometry();
    copy = _fmalloc(sizeof(shapes));
    if (!copy) {
        biosmode(3);
        fclose(report);
        return 2;
    }
    _fmemcpy(copy, shapes, sizeof(shapes));
    nofpu = 0;
    geometry();
    valid = 1;
    for (i = 0; i < (int)(sizeof(shapes) / sizeof(Point)); i++)
        if (abs(copy[i].x - ((Point *)shapes)[i].x) > 1 ||
            abs(copy[i].y - ((Point *)shapes)[i].y) > 1)
            valid = 0;
    check(valid, "8087 and integer geometry agree within one native pixel");
    check(fpu, "Emulated FPU detected");
    _ffree(copy);
    renderer_checks();
    wipe();
    blank = planehash();
    ink(15);
    for (i = 0; i < 100; i++) {
        a = random16() % width;
        b = hud + random16() % (height - hud);
        c = random16() % width;
        d = hud + random16() % (height - hud);
        segment(a, b, c, d);
        reference_line(a, b, c, d);
    }
    segment(10, 40, 10, 40);
    dot(10, 40);
    check(planehash() == blank, "Assembly lines match independent Bresenham pixels across octants");
    ink(15);
    line(-8, 10, 8, 10);
    check((*(unsigned char __far *)MK_FP(0xa000, (hud + 10) * (width / 8))) == 255,
          "Left edge wraps into right edge");
    line(-8, 10, 8, 10);
    check(planehash() == blank, "Wrapped XOR line erases exactly");
    memset(rocks,0,sizeof(rocks));
    started=0; audiofound=0; framephase=0; valid=1;
    for(i=0;i<16;i++) { rocks[i].on=1; rocks[i].spin=1; }
    for(i=0;i<8;i++) {
        prior=0;
        for(j=0;j<16;j++) prior+=rocks[j].angle;
        logic();
        for(j=0;j<16;j++) prior-=rocks[j].angle;
        if(prior!=-4) valid=0;
    }
    check(valid,"Rotation updates spread evenly across four ticks");
    valid=1;
    for(i=0;i<16;i++) if(rocks[i].angle!=2) valid=0;
    check(valid,"Staggered rotations retain the original angular speed");
    start_game();
    check(remaining == 4 && lives == 3 && wave == 1, "Initial four rocks, three lives");
    check(nearxy(0, 0, (worldw - 1) * 64, 0, 2), "Collision wraps across horizontal seam");
    check(nearxy(0, 0, 0, (worldh - 1) * 64, 2), "Collision wraps across vertical seam");
    check(!nearxy(0, 0, worldw * 32, worldh * 32, 20), "Far collision rejected");
    hit(0);
    check(remaining == 5 && score == 20, "Large asteroid splits into two medium rocks");
    hit(0);
    check(remaining == 6 && score == 70, "Medium asteroid splits into two small rocks");
    hit(0);
    check(remaining == 5 && score == 170, "Small asteroid removed and scored");
    start_game();
    valid = 1;
    for (j = 0; j < 3; j++) {
        for (i = 0; i < ROCKS; i++)
            if (rocks[i].on && rocks[i].size < 2)
                hit(i);
    }
    for (i = 0; i < ROCKS; i++)
        if (rocks[i].on && rocks[i].size != 2)
            valid = 0;
    check(valid && remaining == 16, "All four large asteroids can split into sixteen small rocks");
    start_game();
    background();
    bg = planehash();
    render();
    for (i = 0; i < 100; i++) {
        logic();
        render();
    }
    memset(rocks, 0, sizeof(rocks));
    memset(shots, 0, sizeof(shots));
    memset(sparks, 0, sizeof(sparks));
    gameover = 1;
    render();
    check(planehash() == bg, "Moving objects restore every background plane without trails");
    start_game();
    shipx = 0;
    shipy = 0;
    shipvx = -192;
    shipvy = -192;
    logic();
    check(shipx == (worldw * 64 - 192) && shipy == (worldh * 64 - 192),
          "Ship wraps both axes without overflow");
    start_game();
    shootwait = 2;
    shootqueue = 7;
    logic();
    check(!shots[0].life, "Fire buffer respects cooldown");
    logic();
    check(shots[0].life == 48 && shootqueue == 0, "Buffered short tap fires when cooldown ends");
    memset(shots, 0, sizeof(shots));
    shootwait = 0;
    shipx = worldw * 32;
    shipy = worldh * 32;
    rocks[0].x = shipx;
    rocks[0].y = shipy - 18 * 64;
    rocks[0].vx = rocks[0].vy = 0;
    rocks[0].grace = 0;
    prior = remaining;
    fire();
    logic();
    check(remaining == prior + 1 && score == 20, "Fired bullet hits target and splits it once");
    start_game();
    protect = 0;
    rocks[0].x = shipx;
    rocks[0].y = shipy;
    rocks[0].vx = rocks[0].vy = 0;
    logic();
    check(lives == 2 && dead == 55, "Collision costs exactly one life");
    for (i = 0; i < 55; i++)
        logic();
    check(lives == 2 && !dead && protect > 0, "Respawn supplies temporary protection");
    lives = 1;
    protect = 0;
    rocks[0].x = shipx;
    rocks[0].y = shipy;
    logic();
    for (i = 0; i < 55; i++)
        logic();
    check(gameover && !lives, "Final death enters game over");
    start_game();
    memset(rocks, 0, sizeof(rocks));
    remaining = 0;
    for (i = 0; i < 55; i++)
        logic();
    check(wave == 2 && remaining == 5, "Cleared wave advances and increases rock count");
    score = 4990;
    nextlife = 5000;
    lives = 3;
    hit(0);
    check(lives == 4 && nextlife == 10000UL, "Bonus ship awarded at 5000 points");
    start_game();
    shipvx = 7;
    shipvy = -9;
    keys[80] = 1;
    logic();
    keys[80] = 0;
    check(!shipvx && !shipvy, "Brake brings small residual drift to a complete stop");
    score = 99990UL;
    hit(0);
    check(score == 99999UL && best == 99999UL, "Score saturates without wrapping");
    v0 = _dos_getvect(8);
    v1 = _dos_getvect(9);
    install();
    t = ticks;
    while ((unsigned)(ticks - t) < 10) {
    }
    {
        unsigned raw[3], begin = ticks;
        int ok = joyread(raw);
        check((unsigned)(ticks - begin) < 3, "Gameport read has a bounded timeout");
        if (!ok) {
            joyenabled = 1;
            for (i = 0; i < 8; i++)
                joy_poll();
            check(!joyenabled && !joyfire && !joyleft && !joyright,
                  "Missing controller falls back to keyboard");
        }
    }
    uninstall();
    check(_dos_getvect(8) == v0 && _dos_getvect(9) == v1,
          "Timer and keyboard interrupt vectors restored");
    biosmode(3);
    fprintf(report, "RESULT %d passed, %d failed\n", passed, failed);
    fclose(report);
    printf("CHECKS: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
