/* Explicit little-endian format. Old A/B scores remain in a legacy table. */
#define NSCORES 10
#define SCORE_MODES 4
#define MARATHON 0
#define SPRINT 1
#define BTYPE 2
#define LEGACY_B 3
#define OLDSAVESIZE 348
#define SAVESIZE 828
typedef struct {
    uint32_t score, time;
    char name[9];
    uint8_t level, height, won;
} Record;
static Record scores[SCORE_MODES][NSCORES];
static unsigned opttype, optlevel, optheight, theme, soundmode = 3;
static char player[9] = "PLAYER", savepath[160], backpath[160], newpath[160];
static int savefailed, recovered, imported;
static unsigned checksum(const unsigned char *p, unsigned n) {
    unsigned crc = 0xffff, i;
    while (n--) {
        crc ^= *p++;
        for (i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
    }
    return crc;
}
static uint32_t gets32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void puts32(unsigned char *p, uint32_t n) {
    unsigned k;
    for (k = 0; k < 4; ++k) p[k] = (unsigned char)(n >> (k * 8));
}
static void paths(const char *exe) {
    unsigned n = 0, i;
    for (i = 0; exe[i] && i < 140; ++i)
        if (exe[i] == '\\' || exe[i] == '/' || exe[i] == ':') n = i + 1;
    memcpy(savepath, exe, n); strcpy(savepath + n, "NUXTRIS.DAT");
    memcpy(backpath, exe, n); strcpy(backpath + n, "NUXTRIS.BAK");
    memcpy(newpath, exe, n); strcpy(newpath + n, "NUXTRIS.NEW");
}
static void packsave(unsigned char *b) {
    unsigned i, j, p = 24, c;
    Record *r;
    memset(b, 0, SAVESIZE);
    memcpy(b, "NXTRIS02", 8);
    b[8] = opttype; b[9] = optlevel; b[10] = optheight;
    b[11] = theme; b[12] = soundmode;
    memcpy(b + 13, player, 8);
    for (i = 0; i < SCORE_MODES; ++i)
        for (j = 0; j < NSCORES; ++j) {
            r = &scores[i][j];
            puts32(b + p, r->score); p += 4;
            puts32(b + p, r->time); p += 4;
            memcpy(b + p, r->name, 8); p += 8;
            b[p++] = r->level; b[p++] = r->height;
            b[p++] = r->won; b[p++] = 0;
        }
    c = checksum(b, SAVESIZE - 2);
    b[SAVESIZE - 2] = (unsigned char)c;
    b[SAVESIZE - 1] = (unsigned char)(c >> 8);
}
static int validname(const unsigned char *p) {
    unsigned i;
    for (i = 0; i < 8; ++i)
        if (p[i] && p[i] != ' ' && !(p[i] >= 'A' && p[i] <= 'Z') &&
            !(p[i] >= '0' && p[i] <= '9')) return 0;
    return 1;
}
/* Return 1 for old A/B, 2 for current four-table format, 0 on error. */
static int readsave(const char *path, unsigned char *b) {
    FILE *f = fopen(path, "rb");
    unsigned n, p, i, size, stride, namepos;
    uint32_t score, time, previous;
    int version, ok = 1, emptyseen = 0;
    if (!f) return 0;
    n = fread(b, 1, SAVESIZE, f);
    if (fgetc(f) != EOF) ok = 0;
    fclose(f);
    if (!ok) return 0;
    version = n == OLDSAVESIZE && !memcmp(b, "NXTRIS01", 8) ? 1 :
              n == SAVESIZE && !memcmp(b, "NXTRIS02", 8) ? 2 : 0;
    if (!version) return 0;
    size = version == 1 ? OLDSAVESIZE : SAVESIZE;
    stride = version == 1 ? 16 : 20;
    namepos = version == 1 ? 4 : 8;
    if (checksum(b, size - 2) !=
        (b[size - 2] | ((unsigned)b[size - 1] << 8)) ||
        b[8] > (version == 1 ? 1 : 2) || b[9] > 9 || b[10] > 5 ||
        b[11] > 3 || b[12] > 3 || !validname(b + 13)) return 0;
    previous = 999999999UL;
    for (i = 0, p = 24; i < (version == 1 ? 20 : 40); ++i, p += stride) {
        unsigned table = i / NSCORES;
        score = gets32(b + p);
        time = version == 1 ? 0 : gets32(b + p + 4);
        if (i % NSCORES == 0) {
            previous = table == SPRINT && version == 2 ? 0 : 999999999UL;
            emptyseen = 0;
        }
        if (score > 999999999UL || !validname(b + p + namepos) ||
            b[p + namepos + 8] > 19 || b[p + namepos + 9] > 5 ||
            b[p + namepos + 10] > 1) return 0;
        if (table == SPRINT && version == 2) {
            if ((time && !b[p + namepos + 10]) || (!time && score) ||
                (!time && b[p + namepos + 10]) || (time && emptyseen) ||
                (previous && time && time < previous)) return 0;
            if (time) previous = time;
            else emptyseen = 1;
        } else {
            if (score > previous) return 0;
            previous = score;
        }
    }
    return version;
}
static void loadscores(void) {
    unsigned char b[SAVESIZE];
    unsigned i, j, p, step, mode, namepos;
    int version = readsave(savepath, b);
    if (!version) {
        version = readsave(backpath, b);
        if (!version) return;
        recovered = 1;
    }
    imported = version == 1;
    opttype = version == 1 && b[8] ? BTYPE : b[8];
    optlevel = b[9]; optheight = b[10]; theme = b[11]; soundmode = b[12];
    memcpy(player, b + 13, 8); player[8] = 0;
    step = version == 1 ? 16 : 20;
    namepos = version == 1 ? 4 : 8;
    for (i = 0, p = 24; i < (version == 1 ? 2 : SCORE_MODES); ++i)
        for (j = 0; j < NSCORES; ++j, p += step) {
            Record *r;
            mode = version == 1 && i == 1 ? LEGACY_B : i;
            r = &scores[mode][j];
            r->score = gets32(b + p);
            r->time = version == 1 ? 0 : gets32(b + p + 4);
            memcpy(r->name, b + p + namepos, 8); r->name[8] = 0;
            r->level = b[p + namepos + 8];
            r->height = b[p + namepos + 9];
            r->won = b[p + namepos + 10];
        }
}
static int savescores(void) {
    unsigned char b[SAVESIZE], check[SAVESIZE];
    FILE *f;
    int ok, hadold;
    packsave(b);
    f = fopen(newpath, "wb");
    if (!f) { savefailed = 1; return 0; }
    ok = fwrite(b, 1, SAVESIZE, f) == SAVESIZE;
    if (fclose(f)) ok = 0;
    if (!ok || readsave(newpath, check) != 2 || memcmp(b, check, SAVESIZE)) {
        savefailed = 1; return 0;
    }
    hadold = readsave(savepath, check) != 0;
    if (hadold) {
        if (remove(backpath) && access(backpath, 0) == 0) {
            savefailed = 1; return 0;
        }
        if (rename(savepath, backpath)) { savefailed = 1; return 0; }
    } else if (access(savepath, 0) == 0 && remove(savepath)) {
        savefailed = 1; return 0;
    }
    if (rename(newpath, savepath)) {
        if (hadold) rename(backpath, savepath);
        savefailed = 1; return 0;
    }
    savefailed = recovered = imported = 0;
    return 1;
}
static int scoreplace(unsigned mode, uint32_t value, uint32_t time, int completed) {
    int i;
    if (mode == SPRINT) {
        if (!completed || !time) return -1;
        for (i = 0; i < NSCORES; ++i)
            if (!scores[mode][i].time || time < scores[mode][i].time ||
                (time == scores[mode][i].time && value > scores[mode][i].score)) return i;
        return -1;
    }
    if (!value) return -1;
    for (i = 0; i < NSCORES; ++i)
        if (value > scores[mode][i].score) return i;
    return -1;
}
static void recordscore(int place, int completed, uint32_t time) {
    int i;
    Record *r;
    for (i = NSCORES - 1; i > place; --i)
        scores[game.type][i] = scores[game.type][i - 1];
    r = &scores[game.type][place];
    r->score = game.score; r->time = time;
    strcpy(r->name, player);
    r->level = game.level; r->height = game.height; r->won = completed;
}
