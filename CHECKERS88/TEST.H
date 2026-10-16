static FILE *report;
static unsigned tests,failures;
static void check(int ok,const char *s) {
    ++tests;if(!ok){++failures;fprintf(report,"FAIL: %s\n",s);}
}
#include "TESTCORE.H"
static unsigned long vram_crc(void) {
    U8 __far *v=(U8 __far *)MK_FP(0xa000,0);
    unsigned i;int plane;unsigned long h=2166136261UL;
    for(plane=0;plane<4;++plane) {
        outpw(0x3ce,(plane<<8)|4);
        for(i=0;i<38400;++i) {h^=v[i];h*=16777619UL;}
    }
    outpw(0x3ce,4);return h;
}
static void capture(const char *path) {
    FILE *f=fopen(path,"wb");U8 __far *v=(U8 __far *)MK_FP(0xa000,0);
    unsigned i;int p;U8 data[800];
    if(!f)return;
    for(p=0;p<4;++p) {
        outpw(0x3ce,(p<<8)|4);
        for(i=0;i<38400;i+=800) {_fmemcpy(data,v+i,800);fwrite(data,1,800,f);}
    }
    outpw(0x3ce,4);fclose(f);
}
static int safe_point(Point p) {return p.x>=10&&p.x<=629&&p.y>=120&&p.y<=443;}
static void camera_tests(void) {
    int a,e,r,c,k,s,ok,visible;Point q,face[4];
    have87=0;
    for(a=0;a<24;++a)for(e=0;e<5;++e) {
        azimuth=a;elevation=e;geometry();ok=1;
        for(r=0;r<8;++r)for(c=0;c<8;++c) {
            q=project(c*64-224,(7-r)*64+32,0);
            if(hit_square(q.x,q.y)!=square(r,c))ok=0;
        }
        for(r=0;r<9;++r)for(c=0;c<9;++c)if(!safe_point(grid[r][c]))ok=0;
        for(s=0;s<32;++s) {
            for(r=0;r<2;++r)for(k=0;k<12;++k)if(!safe_point(rings[s][r][k]))ok=0;
            for(k=0;k<7;++k)if(!safe_point(crowns[s][k]))ok=0;
            for(k=0;k<4;++k)if(!safe_point(lights[s][k])||!safe_point(corners[s][k]))ok=0;
        }
        visible=0;
        for(k=0;k<4;++k) {
            if(!safe_point(rim[k])||!safe_point(shoulder[k])||!safe_point(belt[k])||!safe_point(foot[k]))ok=0;
            face[0]=shoulder[k];face[1]=shoulder[(k+1)&3];face[2]=belt[(k+1)&3];face[3]=belt[k];
            if(face_area(face)>0)++visible;
        }
        check(ok&&visible>=1&&visible<=2,"orbit view: board/base bounds, picking and visible sides");
    }
    azimuth=0;elevation=2;geometry();
}
static void update_tests(void) {
    Board before;Move m;int i;unsigned oldhist;
    inmenu=0;opponent=1;start_game();
    /* Edge piece has only one legal move, but selection MUST NOT execute it. */
    before=game;select_square(20);
    check(samepos(&before,&game)&&selected==20&&cursor!=20,"sole move focuses landing without playing");
    i=cursor;game_input(13);
    check(game.cell[20]==0&&game.cell[i]==1&&game.side==-1,"Enter confirms sole destination");
    check(lastfrom==20&&lastto==i,"last move endpoints recorded");
    check(undo_turn()&&samepos(&before,&game)&&histories==1,"local undo restores board and draw history");
    check(lastfrom==-1&&lastto==-1,"undo restores earlier move markers");
    /* Human turn plus AI reply is one undo action. */
    opponent=0;human=1;start_game();before=game;
    make_move(legal[0]);m=computer(&game,0,100UL,0);make_move(m);
    check(undocount==2&&game.side==1,"two complete turns retained");
    check(undo_turn()&&samepos(&before,&game)&&undocount==0,"AI undo returns to human decision");
    human=-1;start_game();make_move(legal[0]);before=game;
    check(!undo_turn()&&samepos(&before,&game),"cannot undo AI opening without a human turn");
    inmenu=1;draw_menu();check(!strcmp(menutext[8],"UNDO LAST TURN  NONE"),"menu disables undo until human has played");
    opponent=1;human=1;start_game();
    empty_board(&game);put(&game,5,0,1);put(&game,4,1,-1);put(&game,2,3,-1);put(&game,0,7,-1);
    history_reset(&game);turn_status();draw_board(&game);refresh();before=game;
    select_square(square(5,0));game_input(13);
    check(game.forced==square(3,2)&&cursor==square(1,4)&&game.cell[square(2,3)]==-1,
        "sole continuation highlights but waits for another confirmation");
    check(undo_turn()&&samepos(&before,&game)&&game.cell[square(4,1)]==-1,"undo partial capture restores all pieces");
    select_square(square(5,0));game_input(13);game_input(32);
    check(game.forced==-1&&undocount==1,"complete capture chain uses one undo snapshot");
    check(undo_turn()&&samepos(&before,&game),"undo full capture chain");
    /* Exercise ring rollover and reconstruction with 80 reversible positions. */
    undocount=undohead=0;inmenu=1;board_start(&game);history_reset(&game);lastfrom=lastto=-1;
    for(i=0;i<150;++i){game.quiet=0;undo_push(cursor);game.side=-game.side;history_add(&game);}
    check(undocount==128&&undohead==22,"undo ring bounded at 128 turns");
    check(undo_turn()&&undocount==127,"undo after ring wraps");
    undocount=undohead=0;empty_board(&game);put(&game,7,0,2);put(&game,0,7,-2);history_reset(&game);
    for(i=0;i<10;++i){undo_push(cursor);game.side=-game.side;++game.quiet;history_add(&game);}
    oldhist=histories;undo_turn();
    check(histories==oldhist-1&&game.quiet==9,"undo rebuilds reversible draw history");
    /* Camera survives a new match but is intentionally absent from settings. */
    azimuth=7;elevation=4;geometry();start_game();check(azimuth==7&&elevation==4,"new game preserves session camera");
    camera_key('r');check(azimuth==0&&elevation==2,"camera reset restores default");
    camera_key('d');capture("ROTATE.VGA");camera_key('d');camera_key('d');capture("DEPTH.VGA");
    {unsigned long pixels=vram_crc();int j;
     for(j=0;j<32;++j){piece(j,2);piece(j,2);piece(j,-2);piece(j,-2);}
     check(pixels==vram_crc(),"rotated pieces erase over solid frame exactly");}
    camera_key('r');
    capture("BOARD.VGA");
    inmenu=2;menusel=5;draw_menu();capture("MUSIC.VGA");
    menu_input(LEFT);check(musicvolume==7,"music slider keyboard decreases");
    menu_input(27);check(inmenu==1&&menusel==6,"music submenu back returns to main menu");
    musicvolume=fxvolume=8;volumechange=1;
}
static void review_tests(void) {
    unsigned status,menus,p,m,b;int i;Board before;Point landing;
    opponent=1;human=1;mouseclick=0;start_game();
    status=statuspasses;p=drawpieces;m=drawmarks;b=drawbases;
    for(i=0;i<100;++i)game_input(0);
    check(status==statuspasses&&p==drawpieces&&m==drawmarks&&b==drawbases,
        "idle play skips status formatting and all board work");
    game_input(RIGHT);game_input(LEFT);
    check(status==statuspasses,"cursor navigation does not rebuild unchanged status text");
    /* Press Enter and click the sole landing in one poll: only select the
     * piece. It must still take another deliberate action to play the move. */
    before=game;cursor=20;landing=project(colof(square(4,1))*64-224,(7-4)*64+32,0);
    pointerx=landing.x;pointery=landing.y;mouseclick=1;game_input(13);mouseclick=0;
    check(samepos(&before,&game)&&selected==20&&cursor==square(4,1),
        "simultaneous Enter and click cannot select and play in one poll");
    game_input(13);check(game.cell[20]==0&&game.cell[square(4,1)]==1,"next action confirms move normally");
    inmenu=1;menusel=2;draw_menu();menus=menupasses;
    for(i=0;i<100;++i)menu_input(0);
    check(menus==menupasses,"idle menu skips all row formatting");
    opponent=1;mouseclick=1;pointerx=200;pointery=160;menu_input(13);mouseclick=0;
    check(opponent==0,"simultaneous menu Enter and click applies only once");
}
static unsigned pixel_at(int x,int y) {
    U8 __far *v=(U8 __far *)MK_FP(0xa000,0);unsigned color=0;int p;
    for(p=0;p<4;++p){outpw(0x3ce,(p<<8)|4);if(v[y*80+x/8]&(128>>(x&7)))color|=1<<p;}
    outpw(0x3ce,4);return color;
}
static void span_tests(void) {
    int a,b,x,end,ok;unsigned wanted;
    for(a=0;a<8;++a)for(b=0;b<16;++b) {
        end=b<8?32+b:48+(b&7);if(end<32+a)continue;
        solid_begin(6);solidspan(24,125,64);solid_end();
        solid_begin(9);solidspan(32+a,125,end);solid_end();ok=1;
        for(x=24;x<=64;++x){wanted=x>=32+a&&x<=end?9:6;if(pixel_at(x,125)!=wanted)ok=0;}
        check(ok,"solid span exact colour and neighbouring pixels, all alignments");
    }
    solid_begin(1);solidspan(24,125,64);solid_end();
    solid_begin(4);outpw(0x3ce,0x0305);segment(33,125,53,125);solid_end();
    ok=1;for(x=24;x<=64;++x)if(pixel_at(x,125)!=(unsigned)(x>=33&&x<53?4:1))ok=0;
    check(ok,"opaque edge mode paints exact colour without damaging neighbours");
}
static int selftest(void) {
    unsigned probe[2]={0,0},oldcw,cw,p,m,start,basecount;
    unsigned long before;
    int i,r,c,s,oldmode;Point reference[9][9];Move mv;
    report=fopen("RESULT.TXT","w");if(!report)return 1;
    coretests();camera_tests();
    have87=0;geometry();memcpy(reference,grid,sizeof(grid));
    have87=probe87(probe);fprintf(report,"8087 detected: %d\n",have87);
    if(have87) {
        control87(&oldcw);geometry();control87(&cw);
        check(cw==oldcw,"projection restores FPU control word");
        check(!memcmp(reference,grid,sizeof(grid)),"8087 and integer projection match");
    }
    for(r=0;r<8;++r)for(c=0;c<8;++c) {
        Point a=project(c*64-224,(7-r)*64+32,0);
        check(hit_square(a.x,a.y)==square(r,c),"mouse hits every playable/light square correctly");
    }
    check(hit_square(0,0)==-1&&hit_square(639,479)==-1,"mouse outside board rejected");
    oldmode=modequery();check(video_start(),"VGA mode 12h starts");span_tests();
    audiofound=adlib_detect();fprintf(report,"AdLib detected: %d\n",audiofound);
    audio_install();
    board_start(&game);history_reset(&game);live=1;gameover=0;matchopponent=1;cursor=21;selected=-1;
    turn_status();draw_board(&game);refresh();capture("BOARD.VGA");
    p=drawpieces;m=drawmarks;basecount=drawbases;before=vram_crc();refresh();
    check(drawpieces==p&&drawmarks==m&&drawbases==basecount,"unchanged board performs zero base/piece/mark redraws");
    check(vram_crc()==before,"unchanged refresh is byte-identical");
    game_input(RIGHT);game_input(LEFT);
    check(vram_crc()==before,"cursor move and return restores all four planes");
    selected=21;refresh();selected=-1;refresh();
    check(vram_crc()==before,"selection and landing marks erase perfectly");
    /* Round trip individual pieces, including overlaps with board and highlights. */
    for(i=0;i<32;++i) {piece(i,2);piece(i,2);piece(i,-2);piece(i,-2);}
    check(vram_crc()==before,"king geometry XOR round trip preserves background");
    inmenu=0;select_square(21);check(selected==21,"human can select movable piece");
    for(i=0;i<nlegal;++i)if(legal[i].from==21)break;
    s=legal[i].to;select_square(s);
    check(game.cell[21]==0&&game.cell[s]==1&&game.side==-1,"destination selection executes a legal turn");
    empty_board(&game);put(&game,5,0,1);put(&game,4,1,-1);put(&game,2,3,-1);put(&game,0,7,-1);
    history_reset(&game);selected=-1;gameover=0;turn_status();draw_board(&game);refresh();
    select_square(square(5,0));select_square(square(3,2));
    check(selected==game.forced&&game.side==1,"UI locks multi-jump to same piece");
    select_square(square(1,4));check(game.forced==-1&&game.side==-1,"UI completes multi-jump");
    empty_board(&game);put(&game,2,1,1);put(&game,1,2,-1);put(&game,5,6,-2);put(&game,6,3,2);
    history_reset(&game);selected=-1;turn_status();draw_board(&game);refresh();
    select_square(square(2,1));select_square(square(0,3));capture("KINGS.VGA");
    check(game.cell[square(0,3)]==2,"UI crowns captured move");
    update_tests();
    review_tests();
    inmenu=1;menusel=3;draw_menu();capture("MENU.VGA");
    before=vram_crc();menu_rows();check(before==vram_crc(),"unchanged menu draws no changes");
    /* Keyboard path through confirmation: NO is default and TAB can select YES. */
    {union REGS rr;rr.h.ah=5;rr.w.cx=0x1c0d;int86(0x16,&rr,&rr);check(keyread()==13,"BIOS keyboard input");}
    paths("C:\\CHECK88.EXE");check(!strcmp(cfgpath,"C:\\CHECK88.CFG"),"config uses executable directory");
    paths("C:\\FOLDER\\CHECK88.EXE");check(!strcmp(cfgpath,"C:\\FOLDER\\CHECK88.CFG"),"config subdirectory retained");
    paths("TEST88.EXE");theme=3;difficulty=2;human=-1;opponent=1;music=0;
    song=2;shuffle=1;musicvolume=3;fxvolume=9;
    check(config_save(),"settings saved");theme=0;difficulty=0;
    check(config_read(cfgpath)&&theme==3&&difficulty==2,"settings CRC round trip");
    check(song==2&&shuffle==1&&musicvolume==3&&fxvolume==9,"playlist and separate volumes saved");
    {U8 b[16];FILE *f;unsigned crc;memset(b,0,16);memcpy(b,"C881",4);b[4]=1;b[5]=2;b[6]=1;b[7]=1;b[8]=1;b[9]=2;b[10]=1;
     crc=crc16(b,14);b[14]=crc&255;b[15]=crc>>8;f=fopen(cfgpath,"wb");if(f){fwrite(b,1,16,f);fclose(f);}
     check(config_read(cfgpath)&&theme==2&&difficulty==2,"1.0 settings migrate without camera persistence");}
    {FILE *f=fopen(cfgpath,"r+b");if(f){fputc('X',f);fclose(f);}}
    check(!config_read(cfgpath),"damaged settings rejected");
    remove(cfgpath);remove(newpath);remove(bakpath);
    soundmode=3;music=1;pendingfx=3;start=ticks;
    /* Keep sound playing during a real bounded search, then at least one bar. */
    board_start(&game);history_reset(&game);mv=computer(&game,1,1500UL,0);(void)mv;
    while((unsigned)(ticks-start)<100) {}
    check(musstep>0,"timer music runs while foreground searches/waits");
    audio_remove();biosmode(oldmode);check(modequery()==oldmode,"video restored");
    fprintf(report,"%u checks, %u failures\n",tests,failures);fclose(report);return failures?1:0;
}
