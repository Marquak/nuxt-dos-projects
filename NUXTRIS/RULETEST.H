/* Shared host/DOS regression checks for rules and the portable save format. */
static int passed, failed;
static FILE *report;
static void check(int ok, const char *label) {
    fprintf(report, "%s %s\n", ok ? "PASS" : "FAIL", label);
    if (ok)
        ++passed;
    else
        ++failed;
}
static void rule_tests(void) {
    static unsigned char seen[8192];
    unsigned i, j, mask, v;
    int ok, k, x, y, copy[8];
    Game before;
    rng = 1;
    ok = 1;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < 65535U; ++i) {
        v = random16();
        if (!v || (seen[v >> 3] & (1 << (v & 7))))
            ok = 0;
        seen[v >> 3] |= 1 << (v & 7);
    }
    check(ok && rng == 1, "RNG visits all 65535 nonzero word states before repeating");
    bagpos = 7;
    ok = 1;
    for (i = 0; i < 1000; ++i) {
        mask = 0;
        for (j = 0; j < 7; ++j)
            mask |= 1 << next_kind();
        if (mask != 127)
            ok = 0;
    }
    check(ok, "Every seven-piece bag contains each tetromino exactly once");
    new_game(0, 0, 0);
    check(game.level == 0 && game.lines == 0 && game.score == 0 && !game.over,
          "Type A starts with an empty playable board");
    check(collision(-1, 0) && collision(10, 0) && collision(0, 20),
          "Walls and floor reject out-of-range coordinates");
    check(!collision(0, -1) && collision(-1, -1),
          "Above-top rotation allowed but side walls still apply");
    for (k = 0; k < 7; ++k) {
        new_game(0, 0, 0);
        game.next = k;
        spawn();
        check(game.kind == k && !game.over && fits(game.shape, game.x, game.y),
              "All seven pieces spawn inside the playable field");
        game.y = 5;
        memcpy(copy, game.shape, sizeof(copy));
        for (i = 0; i < 4; ++i)
            rotate_shape(1);
        check(!memcmp(copy, game.shape, sizeof(copy)),
              "Four clockwise rotations restore the original piece");
        rotate_shape(0);
        rotate_shape(1);
        check(!memcmp(copy, game.shape, sizeof(copy)),
              "Opposite rotations cancel without shifting the pivot");
    }
    game.next = 4;
    spawn();
    game.y = 5;
    while (move_side(-1)) {
    }
    x = game.x;
    check(!move_side(-1) && game.x == x, "Rejected left movement preserves piece coordinates");
    while (move_side(1)) {
    }
    check(game.x == 7 && !move_side(1), "Horizontal I piece stops at the right boundary");
    rotate_shape(1);
    game.x = 9;
    memcpy(copy, game.shape, sizeof(copy));
    check(!rotate_shape(1) && !memcmp(copy, game.shape, sizeof(copy)),
          "Rotation into a wall is rejected atomically");
    new_game(0, 0, 0);
    y = game.y;
    check(drop_one(1) && game.y == y + 1 && game.score == 1,
          "Soft drop advances exactly one row and awards one point");
    y = game.y;
    drop_one(0);
    check(game.y == y + 1 && game.score == 1,
          "Gravity advances one row without awarding soft-drop points");
    while (drop_one(0)) {
    }
    y = game.y;
    check(!drop_one(1) && game.y == y && game.score == 1,
          "Blocked soft drop neither moves nor awards points");
    lock_shape();
    mask = 0;
    for (y = 0; y < BH; ++y)
        for (x = 0; x < BW; ++x)
            if (game.grid[y][x])
                ++mask;
    check(mask == 4 && !game.over, "Landing commits exactly four cells");
    new_game(0, 0, 0);
    game.y = -3;
    before = game;
    lock_shape();
    check(game.over && !memcmp(game.grid, before.grid, sizeof(game.grid)),
          "Lock-out above top causes game over without a partial write");
    new_game(0, 0, 0);
    memset(game.grid[0], 1, BW);
    memset(game.grid[1], 1, BW);
    game.next = 0;
    spawn();
    check(game.over, "Obstructed spawn causes game over");
    new_game(0, 2, 0);
    for (y = 16; y < 20; ++y)
        memset(game.grid[y], 1, BW);
    game.full = 0xf0000UL;
    game.grid[15][3] = 6;
    check(clear_rows() == 4 && game.lines == 4 && game.score == 3600,
          "Four-line clear uses high row-mask bits and level-scaled score");
    check(game.grid[19][3] == 6 && !game.grid[15][3] && !game.full,
          "Four cleared rows compact correctly and reset the mask");
    new_game(0, 0, 0);
    game.grid[17][2] = 3;
    game.grid[15][4] = 7;
    game.full = (1UL << 16) | (1UL << 19);
    clear_rows();
    check(game.grid[18][2] == 3 && game.grid[17][4] == 7,
          "Separated cleared rows compact in the correct order");
    new_game(0, 3, 0);
    game.lines = 9;
    game.full = 1UL << 19;
    clear_rows();
    check(game.level == 4 && game.score == 160,
          "Type A levels up every ten lines; scoring uses pre-clear level");
    game.lines = 6550;
    game.full = 0;
    clear_rows();
    check(game.level == 19, "Gravity level caps safely at nineteen");
    game.score = 999999998UL;
    add_score(100);
    check(game.score == 999999999UL, "Score saturates without wraparound");
    for (i = 0; i < 6; ++i) {
        new_game(1, 9, i);
        ok = 1;
        mask = 0;
        for (y = 0; y < 20; ++y) {
            v = 0;
            for (x = 0; x < 10; ++x)
                if (game.grid[y][x])
                    ++v;
            if (y < 20 - (int)i * 2 && v)
                ok = 0;
            if (v == 10)
                ok = 0;
            mask += v;
        }
        check(ok && (i == 0 ? !mask : mask > 0),
              "Type B height fills only its starting rows and leaves holes");
    }
    game.lines = 24;
    game.full = 1UL << 19;
    clear_rows();
    check(game.lines == 25 && game.level == 9,
          "Type B reaches 25-line target without changing starting speed");
    for (i = 0; i < 20; ++i) {
        game.level = i;
        check(fall_period() >= 2 && fall_period() <= 30,
              "Gravity period remains bounded at every supported level");
    }
}
static void save_tests(void) {
    unsigned char b[SAVESIZE];
    unsigned c;
    FILE *f;
    memset(scores, 0, sizeof(scores));
    strcpy(player, "NUXT20");
    strcpy(savepath, "TSTSAVE.DAT");
    strcpy(backpath, "TSTSAVE.BAK");
    strcpy(newpath, "TSTSAVE.NEW");
    remove(savepath);
    remove(backpath);
    remove(newpath);
    game.type = 0;
    game.score = 70000UL;
    game.level = 9;
    game.height = 0;
    check(scoreplace(0, game.score) == 0 && scoreplace(0, 0) == -1,
          "Empty score table accepts a score and rejects zero");
    recordscore(0, 0);
    game.score = 123456UL;
    recordscore(0, 0);
    check(scores[0][0].score == 123456UL && scores[0][1].score == 70000UL,
          "Ranked insertion preserves scores above the 16-bit range");
    game.type = 1;
    game.score = 5000;
    game.height = 5;
    recordscore(0, 1);
    theme = 3;
    soundmode = 2;
    opttype = 1;
    optlevel = 9;
    optheight = 5;
    check(savescores(), "Scores and preferences save and verify successfully");
    memset(scores, 0, sizeof(scores));
    strcpy(player, "OTHER");
    theme = 0;
    loadscores();
    check(scores[0][0].score == 123456UL && !strcmp(scores[0][0].name, "NUXT20") &&
              scores[1][0].won == 1 && theme == 3 && !strcmp(player, "NUXT20"),
          "Both tables, player name, win flag and preferences survive reload");
    game.type = 0;
    game.score = 200000UL;
    recordscore(0, 0);
    check(savescores(), "Second save creates a recoverable previous version");
    f = fopen(savepath, "wb");
    fputs("truncated", f);
    fclose(f);
    memset(scores, 0, sizeof(scores));
    loadscores();
    check(recovered && scores[0][0].score == 123456UL,
          "Truncated primary recovers previous high scores from BAK");
    check(savescores() && readsave(backpath, b), "Saving recovered data retains the valid backup");
    packsave(b);
    b[33] ^= 0x40;
    f = fopen(newpath, "wb");
    fwrite(b, 1, sizeof(b), f);
    fclose(f);
    check(!readsave(newpath, b), "CRC rejects a corrupted save");
    packsave(b);
    b[8] = 5;
    c = checksum(b, SAVESIZE - 2);
    b[SAVESIZE - 2] = c;
    b[SAVESIZE - 1] = c >> 8;
    f = fopen(newpath, "wb");
    fwrite(b, 1, sizeof(b), f);
    fclose(f);
    check(!readsave(newpath, b),
          "Semantic validation rejects invalid settings even with a correct CRC");
    strcpy(newpath, "NOEXIST/TST.NEW");
    check(!savescores() && readsave(savepath, b),
          "Write failure leaves the last valid score file readable");
    remove(savepath);
    remove(backpath);
    remove("TSTSAVE.NEW");
    savefailed = recovered = 0;
}
