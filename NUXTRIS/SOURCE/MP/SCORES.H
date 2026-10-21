/* Explicit little-endian save format. Never write compiler-padded structs.
 * A failed save keeps the previous DAT or BAK recoverable. */
#define NSCORES 10
#define SAVESIZE 348
typedef struct {
    uint32_t score;
    char name[9];
    uint8_t level, height, won;
} Record;
static Record scores[2][NSCORES];
static unsigned opttype, optlevel, optheight, theme, soundmode = 3;
static char player[9] = "PLAYER", savepath[160], backpath[160], newpath[160];
static int savefailed, recovered;
static unsigned checksum(const unsigned char *p, unsigned n) {
    unsigned crc = 0xffff, i;
    while (n--) {
        crc ^= *p++;
        for (i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
    }
    return crc;
}
static void paths(const char *exe) {
    unsigned n = 0, i;
    for (i = 0; exe[i] && i < 140; ++i)
        if (exe[i] == '\\' || exe[i] == '/' || exe[i] == ':')
            n = i + 1;
    memcpy(savepath, exe, n);
    strcpy(savepath + n, "NUXTRIS.DAT");
    memcpy(backpath, exe, n);
    strcpy(backpath + n, "NUXTRIS.BAK");
    memcpy(newpath, exe, n);
    strcpy(newpath + n, "NUXTRIS.NEW");
}
static void packsave(unsigned char *b) {
    unsigned i, j, k, p = 24, c;
    uint32_t s;
    memset(b, 0, SAVESIZE);
    memcpy(b, "NXTRIS01", 8);
    b[8] = opttype;
    b[9] = optlevel;
    b[10] = optheight;
    b[11] = theme;
    b[12] = soundmode;
    memcpy(b + 13, player, 8);
    for (i = 0; i < 2; ++i)
        for (j = 0; j < NSCORES; ++j) {
            s = scores[i][j].score;
            for (k = 0; k < 4; ++k)
                b[p++] = (unsigned char)(s >> (k * 8));
            memcpy(b + p, scores[i][j].name, 8);
            p += 8;
            b[p++] = scores[i][j].level;
            b[p++] = scores[i][j].height;
            b[p++] = scores[i][j].won;
            b[p++] = 0;
        }
    c = checksum(b, SAVESIZE - 2);
    b[SAVESIZE - 2] = c;
    b[SAVESIZE - 1] = c >> 8;
}
static int validname(const unsigned char *p) {
    unsigned i;
    for (i = 0; i < 8; ++i)
        if (p[i] && p[i] != ' ' && !(p[i] >= 'A' && p[i] <= 'Z') && !(p[i] >= '0' && p[i] <= '9'))
            return 0;
    return 1;
}
static int readsave(const char *path, unsigned char *b) {
    FILE *f = fopen(path, "rb");
    unsigned p = 24, i, k;
    uint32_t s, previous;
    int ok;
    if (!f)
        return 0;
    ok = fread(b, 1, SAVESIZE, f) == SAVESIZE && fgetc(f) == EOF;
    fclose(f);
    if (!ok || memcmp(b, "NXTRIS01", 8) ||
        checksum(b, SAVESIZE - 2) != (b[SAVESIZE - 2] | ((unsigned)b[SAVESIZE - 1] << 8)))
        return 0;
    if (b[8] > 1 || b[9] > 9 || b[10] > 5 || b[11] > 3 || b[12] > 3 || !validname(b + 13))
        return 0;
    previous = 999999999UL;
    for (i = 0; i < 20; ++i) {
        if (i == 10)
            previous = 999999999UL;
        s = 0;
        for (k = 0; k < 4; ++k)
            s |= (uint32_t)b[p + k] << (k * 8);
        if (s > previous || !validname(b + p + 4) || b[p + 12] > 19 || b[p + 13] > 5 ||
            b[p + 14] > 1)
            return 0;
        previous = s;
        p += 16;
    }
    return 1;
}
static void loadscores(void) {
    unsigned char b[SAVESIZE];
    unsigned p = 24, i, j, k;
    if (!readsave(savepath, b)) {
        if (!readsave(backpath, b))
            return;
        recovered = 1;
    }
    opttype = b[8];
    optlevel = b[9];
    optheight = b[10];
    theme = b[11];
    soundmode = b[12];
    memcpy(player, b + 13, 8);
    player[8] = 0;
    for (i = 0; i < 2; ++i)
        for (j = 0; j < NSCORES; ++j) {
            scores[i][j].score = 0;
            for (k = 0; k < 4; ++k)
                scores[i][j].score |= (uint32_t)b[p++] << (k * 8);
            memcpy(scores[i][j].name, b + p, 8);
            scores[i][j].name[8] = 0;
            p += 8;
            scores[i][j].level = b[p++];
            scores[i][j].height = b[p++];
            scores[i][j].won = b[p++];
            ++p;
        }
}
static int savescores(void) {
    unsigned char b[SAVESIZE], check[SAVESIZE];
    FILE *f;
    int ok, hadold;
    packsave(b);
    f = fopen(newpath, "wb");
    if (!f) {
        savefailed = 1;
        return 0;
    }
    ok = fwrite(b, 1, SAVESIZE, f) == SAVESIZE;
    if (fclose(f))
        ok = 0;
    if (!ok || !readsave(newpath, check) || memcmp(b, check, SAVESIZE)) {
        savefailed = 1;
        return 0;
    }
    /* Do not replace a good recovery backup with a corrupt primary. */
    hadold = readsave(savepath, check);
    if (hadold) {
        if (remove(backpath) && access(backpath, 0) == 0) {
            savefailed = 1;
            return 0;
        }
        if (rename(savepath, backpath)) {
            savefailed = 1;
            return 0;
        }
    } else if (access(savepath, 0) == 0 && remove(savepath)) {
        savefailed = 1;
        return 0;
    }
    if (rename(newpath, savepath)) {
        if (hadold)
            rename(backpath, savepath);
        savefailed = 1;
        return 0;
    }
    savefailed = recovered = 0;
    return 1;
}
static int scoreplace(unsigned type, uint32_t value) {
    int i;
    if (!value)
        return -1;
    for (i = 0; i < NSCORES; ++i)
        if (value > scores[type][i].score)
            return i;
    return -1;
}
static void recordscore(int place, int won) {
    int i;
    Record *r;
    for (i = NSCORES - 1; i > place; --i)
        scores[game.type][i] = scores[game.type][i - 1];
    r = &scores[game.type][place];
    r->score = game.score;
    strcpy(r->name, player);
    r->level = game.level;
    r->height = game.height;
    r->won = won;
}
