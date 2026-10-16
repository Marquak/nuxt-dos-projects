/* 3D Checkers 88 1.2.1. See README.TXT and CREDITS.TXT. */
#include <bios.h>
#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include "ENGINE.H"
#include "VIDEO.H"
#include "AUDIO.H"
static void idle(void);
#pragma aux idle = "sti" "hlt" modify exact []
#include "INPUT.H"
static Board game;
static Move legal[MAXMOVES];
static int nlegal,selected=-1,cursor=21,gameover,live,quit;
static int opponent=0,difficulty=1,human=1,matchopponent,matchhuman;
static int menusel,menushown=-1,inmenu=1,aiabort,helpopen;
static unsigned aistart,ailimit,turnnumber;
static U8 marks[32];
static int lastfrom=-1,lastto=-1,activefrom=-1;
typedef struct {Board board; unsigned turn; S8 from,to,cursor;} Undo;
static Undo undos[128];
static unsigned undohead,undocount;
static int musicmenushown=-1,joymenushown=-1;
static char menutext[13][40],notice[56];
static char cfgpath[160],newpath[160],bakpath[160];
static const char *soundnames[4]={"OFF","ADLIB","SPEAKER","BOTH"};
static const char *themenames[4]={"NEON","CLASSIC","AMBER","ICE"};
static const char *levelnames[3]={"EASY","NORMAL","HARD"};
#ifdef SELFTEST
static unsigned statuspasses,menupasses;
#endif
static void draw_menu(void);
static void refresh(void);
static void paths(const char *exe) {
    unsigned i,n=0;
    for(i=0;exe[i]&&i<140;++i)if(exe[i]=='\\'||exe[i]=='/'||exe[i]==':')n=i+1;
    memcpy(cfgpath,exe,n);strcpy(cfgpath+n,"CHECK88.CFG");
    memcpy(newpath,exe,n);strcpy(newpath+n,"CHECK88.NEW");
    memcpy(bakpath,exe,n);strcpy(bakpath+n,"CHECK88.BAK");
}
static unsigned crc16(const U8 *p,unsigned n) {
    unsigned c=0xffff,i;
    while(n--) {c^=*p++;for(i=0;i<8;++i)c=(c>>1)^((c&1)?0xa001:0);}
    return c;
}
static int config_read(const char *path) {
    U8 b[20];FILE *f=fopen(path,"rb");unsigned n;int old;
    if(!f)return 0;
    n=fread(b,1,sizeof(b),f);if(fgetc(f)!=EOF){fclose(f);return 0;}fclose(f);
    old=n==16&&!memcmp(b,"C881",4);
    if(!old&&(n!=20||memcmp(b,"C882",4)))return 0;
    if(crc16(b,n-2)!=(b[n-2]|((unsigned)b[n-1]<<8)))return 0;
    if(b[4]>1||b[5]>2||b[6]>1||b[7]>3||b[8]>1||b[9]>3)return 0;
    if(!old&&(b[10]>3||b[11]>1||b[12]>10||b[13]>10))return 0;
    opponent=b[4];difficulty=b[5];human=b[6]?1:-1;soundmode=b[7];music=b[8];theme=b[9];
    if(!old){song=b[10];shuffle=b[11];musicvolume=b[12];fxvolume=b[13];}
    return 1;
}
static int config_save(void) {
    U8 b[20];FILE *f;unsigned c;int ok;
    memset(b,0,20);memcpy(b,"C882",4);
    b[4]=opponent;b[5]=difficulty;b[6]=human>0;b[7]=soundmode;b[8]=music;b[9]=theme;
    b[10]=song;b[11]=shuffle;b[12]=musicvolume;b[13]=fxvolume;
    c=crc16(b,18);b[18]=c&255;b[19]=c>>8;
    f=fopen(newpath,"wb");if(!f)return 0;
    ok=fwrite(b,1,20,f)==20;if(fclose(f))ok=0;
    if(!ok){remove(newpath);return 0;}
    if(!access(cfgpath,0)) {
        if(!access(bakpath,0)&&remove(bakpath)) {remove(newpath);return 0;}
        if(rename(cfgpath,bakpath)) {remove(newpath);return 0;}
    }
    if(rename(newpath,cfgpath)) {rename(bakpath,cfgpath);remove(newpath);return 0;}
    return 1;
}
static int computer_turn(void) {return !gameover&&!matchopponent&&game.side!=matchhuman;}
static void set_notice(const char *s) {strncpy(notice,s,55);notice[55]=0;}
static void status_text(void) {
    char s[56];int i,r=0,b=0,rk=0,bk=0;
#ifdef SELFTEST
    ++statuspasses;
#endif
    for(i=0;i<32;++i) {
        if(game.cell[i]>0){++r;if(game.cell[i]==2)++rk;}
        else if(game.cell[i]<0){++b;if(game.cell[i]==-2)++bk;}
    }
    sprintf(s,"RED %02d  KINGS %02d",r,rk);delta(32,82,hudtext[0],s,3,1);
    sprintf(s,"BLUE %02d  KINGS %02d",b,bk);delta(476,82,hudtext[1],s,5,1);
    if(gameover==1)strcpy(s,"RED WINS");
    else if(gameover==2)strcpy(s,"BLUE WINS");
    else if(gameover==3)strcpy(s,"DRAW - THREEFOLD");
    else if(gameover==4)strcpy(s,"DRAW - 40 MOVES");
    else sprintf(s,"%s TO MOVE",game.side>0?"RED":"BLUE");
    delta(236,82,hudtext[2],s,15,2);
    delta(32,106,hudtext[3],notice,15,1);
}
static void update_marks(void) {
    int i;
    memset(marks,0,32);
    if(lastfrom>=0)marks[lastfrom]|=16;
    if(lastto>=0)marks[lastto]|=32;
    if(!gameover&&!computer_turn()) {
        marks[cursor]|=1;
        if(selected>=0)marks[selected]|=2;
        for(i=0;i<nlegal;++i) {
            if(selected<0)marks[legal[i].from]|=8;
            else if(legal[i].from==selected)marks[legal[i].to]|=4;
        }
    }
}
static void refresh(void) {update_marks();update_board(&game,marks);status_text();}
static void turn_status(void) {
    nlegal=moves(&game,legal);
    if(!nlegal) {gameover=game.side>0?2:1;set_notice("ENTER OR ESC FOR MENU");pendingfx=4;}
    else if(repeated(&game)) {gameover=3;set_notice("ENTER OR ESC FOR MENU");}
    else if(game.quiet>=80) {gameover=4;set_notice("ENTER OR ESC FOR MENU");}
    else if(computer_turn())set_notice("THINKING...  ESC MENU");
    else if(game.forced>=0)set_notice("CONTINUE THE JUMP WITH THIS PIECE");
    else if(legal[0].over>=0)set_notice("CAPTURE REQUIRED - SELECT A MARKED PIECE");
    else set_notice("SELECT A PIECE, THEN ITS DESTINATION");
}
static void start_game(void) {
    keys_flush();
    board_start(&game);history_reset(&game);gameover=0;selected=-1;cursor=21;
    undocount=undohead=0;lastfrom=lastto=activefrom=-1;
    matchopponent=opponent;matchhuman=human;turnnumber=1;live=1;inmenu=0;
    turn_status();draw_board(&game);refresh();
}
/* One snapshot per complete turn, never per capture leg. Rebuild exact draw
 * history on undo; 128 retained turns cover the 80-ply reversible draw window. */
static void undo_push(int from) {
    Undo *u;
    if(undocount==128){undohead=(undohead+1)&127;--undocount;}
    u=undos+((undohead+undocount++)&127);u->board=game;u->turn=turnnumber;
    u->from=lastfrom;u->to=lastto;u->cursor=(S8)from;
}
static int undo_index(void) {
    int index=(int)undocount-1;
    if(!live)return -1;
    if(!matchopponent)while(index>=0&&undos[(undohead+index)&127].board.side!=matchhuman)--index;
    return index;
}
static int undo_turn(void) {
    int i,index=undo_index();Undo *u;
    if(!live)return 0;
    if(index<0){set_notice("NO TURN TO UNDO");if(!inmenu)refresh();return 0;}
    u=undos+((undohead+index)&127);game=u->board;turnnumber=u->turn;
    lastfrom=u->from;lastto=u->to;cursor=u->cursor;selected=activefrom=-1;
    undocount=index;gameover=0;pendingfx=0;
    if(!undocount)history_reset(&game);
    else {
        history_reset(&undos[undohead].board);
        for(i=1;i<(int)undocount;++i)history_add(&undos[(undohead+i)&127].board);
        history_add(&game);
    }
    turn_status();if(!inmenu)refresh();return 1;
}
static void focus_destination(void) {
    int i,count=0,to=-1;
    if(selected<0||computer_turn()||gameover)return;
    for(i=0;i<nlegal;++i)if(legal[i].from==selected){++count;to=legal[i].to;}
    if(count==1){cursor=to;set_notice("ENTER / SPACE / BUTTON 1 TO CONFIRM MOVE");}
}
static void make_move(Move m) {
    int p=game.cell[m.from],ended,crowned;
    if(game.forced<0){undo_push(m.from);activefrom=m.from;}
    ended=apply(&game,m);crowned=abs(p)==1&&abs(game.cell[m.to])==2;
    pendingfx=crowned?3:m.over>=0?2:1;
    cursor=m.to;lastfrom=activefrom;lastto=m.to;
    if(ended) {selected=-1;++turnnumber;history_add(&game);}else selected=game.forced;
    turn_status();focus_destination();refresh();
}
static void select_square(int s) {
    int i;
    if(s<0||s>31||gameover||computer_turn())return;
    cursor=s;
    for(i=0;i<nlegal;++i) if(legal[i].from==selected&&legal[i].to==s) {make_move(legal[i]);return;}
    if(game.forced<0) {
        for(i=0;i<nlegal;++i)if(legal[i].from==s) {selected=s;set_notice("SELECT A HIGHLIGHTED DESTINATION");focus_destination();refresh();return;}
        selected=-1;
    }
    turn_status();refresh();
}
static void menu_arrow(int row) {text(98,110+row*25,">",6,2);}
static void menu_rows(void) {
    int i,track=song;char s[40];
#ifdef SELFTEST
    ++menupasses;
#endif
    for(i=0;i<(inmenu==2?8:13);++i) {
        if(inmenu==2) {
            switch(i) {
                case 0:sprintf(s,"MUSIC           %s",!(soundmode&1)||!audiofound?"NEEDS ADLIB":music?"ON":"OFF");break;
                case 1:sprintf(s,"TRACK           %s",tunenames[track]);break;
                case 2:strcpy(s,"PREVIOUS TRACK");break;
                case 3:strcpy(s,"NEXT TRACK");break;
                case 4:sprintf(s,"SHUFFLE         %s",shuffle?"ON":"OFF");break;
                case 5:sprintf(s,"MUSIC VOLUME    %u / 10",(unsigned)musicvolume);break;
                case 6:sprintf(s,"ADLIB EFFECTS   %u / 10",(unsigned)fxvolume);break;
                default:strcpy(s,"BACK");break;
            }
        } else switch(i) {
            case 0:strcpy(s,live?"RESUME":"PLAY");break;
            case 1:strcpy(s,"NEW GAME");break;
            case 2:sprintf(s,"OPPONENT        %s",opponent?"TWO PLAYERS":"COMPUTER");break;
            case 3:sprintf(s,"DIFFICULTY      %s",levelnames[difficulty]);break;
            case 4:sprintf(s,"YOUR COLOUR     %s",human>0?"RED":"BLUE");break;
            case 5:sprintf(s,"SOUND           %s",soundnames[soundmode]);break;
            case 6:strcpy(s,"MUSIC AND VOLUME");break;
            case 7:sprintf(s,"COLOURS         %s",themenames[theme]);break;
            case 8:strcpy(s,undo_index()>=0?"UNDO LAST TURN":"UNDO LAST TURN  NONE");break;
            case 9:sprintf(s,"JOYSTICK        %s",joyenabled?"CALIBRATED":"CALIBRATE");break;
            case 10:strcpy(s,"DISABLE JOYSTICK");break;
            case 11:strcpy(s,"HOW TO PLAY");break;
            default:strcpy(s,"EXIT");break;
        }
        delta(124,110+i*25,menutext[i],s,15,2);
    }
    if(menushown!=menusel) {if(menushown>=0)menu_arrow(menushown);menu_arrow(menusel);menushown=menusel;}
    musicmenushown=track;joymenushown=joyenabled;
}
static void draw_menu(void) {
    screen_clear();title();
    center(78,inmenu==2?"MUSIC AND VOLUME":"RED PLAYS FIRST",3,1);
    memset(menutext,0,sizeof(menutext));menushown=-1;menu_rows();
    text(34,452,"ENTER SELECT   ARROWS CHANGE   F1 HELP   F10 EXIT",15,1);
    text(34,467,inmenu==2?"PC SPEAKER VOLUME IS FIXED - USE SOUND ON/OFF":"OPPONENT AND COLOUR APPLY TO A NEW GAME",8,1);
}
static void show_help(void) {
    static const char *lines[]={
        "MOVE DIAGONALLY ON THE DARK SQUARES.",
        "MEN MOVE AND CAPTURE FORWARD. KINGS GO BOTH WAYS.",
        "CAPTURES ARE REQUIRED. FINISH EVERY JUMP IN A CHAIN.",
        "YOU MAY CHOOSE ANY AVAILABLE CAPTURE ROUTE.",
        "REACH THE FAR ROW TO BECOME A KING. THAT ENDS THE TURN.",
        "WIN BY TAKING EVERY PIECE OR BLOCKING ALL LEGAL MOVES.",
        "THREE REPEATS OR 40 TURNS EACH WITHOUT A MAN MOVE",
        "OR CAPTURE ARE AUTOMATIC DRAWS.",
        "ARROWS: MOVE CURSOR     ENTER/SPACE: SELECT / MOVE",
        "MOUSE: CLICK PIECE, THEN DESTINATION",
        "GAMEPAD: D-PAD MOVES    BUTTON 1 SELECTS",
        "ESC / RIGHT CLICK / BUTTON 2: CANCEL SELECTION / MENU",
        "W/S: TILT    A/D: TURN    R: DEFAULT CAMERA",
        "U: UNDO YOUR TURN AND THE COMPUTER REPLY",
        "SOLE DESTINATION IS HIGHLIGHTED; CONFIRM TO MOVE.",
        "[ / ]: PREVIOUS / NEXT TRACK    M: MUSIC ON/OFF",
        "- / +: MUSIC VOLUME    MUSIC MENU: EFFECTS VOLUME",
        "F2: SOUND    F10: EXIT WITH CONFIRMATION"
    };
    int i,k;
    keys_flush();screen_clear();title();center(83,"HOW TO PLAY",6,2);
    for(i=0;i<(int)(sizeof(lines)/sizeof(lines[0]));++i)text(44,118+i*17,lines[i],15,1);
    center(445,"ENTER / ESC / BUTTON 2 TO RETURN",6,1);
    helpopen=1;
    do {k=wait_input();}while(k!=13&&k!=27&&k!=32&&!mouseclick);
    helpopen=0;keys_flush();
}
static int confirm(const char *question) {
    int yes=0,old=-1,k;
    keys_flush();screen_clear();title();center(195,question,15,2);
    center(300,"TAB / ARROWS TO CHOOSE - ENTER CONFIRMS",15,1);
    for(;;) {
        if(old!=yes) {
            if(old>=0) {center(250,old?"> YES       NO":"  YES     > NO",6,2);}
            center(250,yes?"> YES       NO":"  YES     > NO",6,2);old=yes;
        }
        k=wait_input();
        if(k==27)return 0;
        if(k==9||k==LEFT||k==RIGHT||k==UP||k==DOWN)yes=!yes;
        if(mouseclick&&pointery>=242&&pointery<=275) {
            if(pointerx>=224&&pointerx<320)return 1;
            if(pointerx>=320&&pointerx<=416)return 0;
        }
        if(k=='y'||k=='Y')return 1;
        if(k=='n'||k=='N')return 0;
        if(k==13)return yes;
    }
}
static void resume_screen(void) {
    keys_flush();
    if(inmenu)draw_menu();else {draw_board(&game);refresh();}
}
static void enter_menu(void) {keys_flush();inmenu=1;menusel=0;draw_menu();}
static void menu_action(int direction) {
    if(inmenu==2) {
        switch(menusel) {
            case 0:music_change();break;
            case 1:music_seek(direction);break;
            case 2:music_seek(-1);break;
            case 3:music_seek(1);break;
            case 4:shuffle=!shuffle;break;
            case 5:volume_step(0,direction);break;
            case 6:volume_step(1,direction);break;
            case 7:inmenu=1;menusel=6;draw_menu();return;
        }
        menu_rows();return;
    }
    switch(menusel) {
        case 0:if(!live)start_game();else {inmenu=0;draw_board(&game);turn_status();refresh();}return;
        case 1:if(!live||gameover||confirm("START A NEW GAME?"))start_game();else draw_menu();return;
        case 2:opponent=!opponent;break;
        case 3:difficulty=(difficulty+3+direction)%3;break;
        case 4:human=-human;break;
        case 5:audio_change();break;
        case 6:inmenu=2;menusel=0;draw_menu();return;
        case 7:theme=(theme+4+direction)%4;palette();break;
        case 8:if(undo_turn()){inmenu=0;draw_board(&game);refresh();return;}break;
        case 9:calibrate();draw_menu();return;
        case 10:joyenabled=0;break;
        case 11:show_help();draw_menu();return;
        case 12:if(confirm("EXIT TO DOS?"))quit=1;else draw_menu();return;
    }
    menu_rows();
}
static void menu_input(int k) {
    int count=inmenu==2?8:13;
    if(!k&&!mouseclick)return;
    if(k==UP)menusel=(menusel+count-1)%count;
    if(k==DOWN||k==9)menusel=(menusel+1)%count;
    if(k==UP||k==DOWN||k==9){menu_rows();return;}
    if(k==27&&inmenu==2){inmenu=1;menusel=6;draw_menu();return;}
    if(k==27&&live) {menusel=0;menu_action(1);return;}
    if(k==LEFT||k==RIGHT) {if(inmenu==2||(menusel>=2&&menusel<=7))menu_action(k==LEFT?-1:1);return;}
    /* A keyboard action can open a dialog and consume a new mouse click.
     * Never dispatch that click again against the previous menu. */
    if(k==13||k==32){menu_action(1);return;}
    if(mouseclick&&pointerx>=90&&pointerx<=590&&pointery>=105&&pointery<105+count*25) {
        menusel=(pointery-105)/25;menu_action(1);
    }
}
static int camera_key(int k) {
    int a=azimuth,e=elevation;
    if(k=='a'||k=='A')azimuth=(azimuth+23)%24;
    if(k=='d'||k=='D')azimuth=(azimuth+1)%24;
    if((k=='w'||k=='W')&&elevation<4)++elevation;
    if((k=='s'||k=='S')&&elevation>0)--elevation;
    if(k=='r'||k=='R'){azimuth=0;elevation=2;}
    if(a==azimuth&&e==elevation)return 0;
    geometry();draw_board(&game);refresh();return 1;
}
static void game_input(int k) {
    int row=rowof(cursor),c=cursor&3,s,oldcursor=cursor;
    if(!k&&!mouseclick)return;
    if(k=='u'||k=='U'){undo_turn();return;}
    if(camera_key(k))return;
    if(k==27) {
        if(selected>=0&&game.forced<0&&!gameover) {selected=-1;turn_status();refresh();}
        else enter_menu();
        return;
    }
    if(gameover) {if(k==13||k==32||mouseclick)enter_menu();return;}
    if(computer_turn())return;
    if(k==LEFT&&c>0)--cursor;
    if(k==RIGHT&&c<3)++cursor;
    if(k==UP&&row>0)cursor-=4;
    if(k==DOWN&&row<7)cursor+=4;
    if(k==LEFT||k==RIGHT||k==UP||k==DOWN) {
        if(cursor!=oldcursor){update_marks();update_board(&game,marks);}
        return;
    }
    if(k==13||k==32){select_square(cursor);return;}
    if(mouseclick) {
        s=hit_square(pointerx,pointery);
        if(s>=0)select_square(s);
        else if(pointery>=448&&pointerx<90) {show_help();resume_screen();}
        return;
    }
}
static int ai_poll(void) {
    int k=input();
    if(k==27) {aiabort=1;return 1;}
    if(k==F2)audio_change();
    if(k=='m'||k=='M')music_change();
    if(k==F10) {aiabort=2;return 1;}
    if(k=='u'||k=='U') {aiabort=3;return 1;}
    if(k=='[')music_seek(-1);if(k==']')music_seek(1);
    if(k=='-')volume_step(0,-1);if(k=='+'||k=='=')volume_step(0,1);
    return (unsigned)(ticks-aistart)>=ailimit;
}
static void ai_turn(void) {
    static const unsigned waits[3]={36,109,291};
    static const unsigned long budgets[3]={700UL,7000UL,50000UL};
    Move m;
    aiabort=0;aistart=ticks;ailimit=waits[difficulty];
    m=computer(&game,difficulty,budgets[difficulty],ai_poll);
    if(aiabort) {
        if(aiabort==3){undo_turn();return;}
        if(aiabort==2 && confirm("EXIT TO DOS?")){quit=1;return;}
        enter_menu();return;
    }
    make_move(m);
}
#ifdef SELFTEST
#include "TEST.H"
#endif
int main(int argc,char **argv) {
    int i,k,oldmode,saved=1,joylaunch=0;unsigned probe[2]={0,0};
    unsigned last;
    paths(argv[0]);if(!config_read(cfgpath))config_read(bakpath);
    for(i=1;i<argc;++i) {
        if(!stricmp(argv[i],"/S"))soundmode=0;
        else if(!stricmp(argv[i],"/A"))soundmode=1;
        else if(!stricmp(argv[i],"/P"))soundmode=2;
        else if(!stricmp(argv[i],"/B"))soundmode=3;
        else if(!stricmp(argv[i],"/J"))joylaunch=1;
        else if(!stricmp(argv[i],"/NO87"))no87=1;
        else if(!stricmp(argv[i],"/NOMUSIC"))music=0;
        else {
            puts("3D Checkers 88 1.2.1\nCHECK88 [/A | /P | /B | /S] [/J] [/NOMUSIC] [/NO87]\n"
                 "A AdLib, P speaker, B both, S silent, J calibrate joystick.\n"
                 "V20/80186-compatible CPU, DOS 3.3+, VGA 640x480/16 colours.\n"
                 "Mouse driver and 8087 optional. No other files needed to play.");return 0;
        }
    }
    engine_init();
#ifdef SELFTEST
    return selftest();
#else
    if(!no87)have87=probe87(probe);
    geometry();audiofound=adlib_detect();
    if(!audiofound&&(soundmode&1))soundmode=2;
    oldmode=modequery();
    if(!video_start()) {biosmode(oldmode);puts("VGA 640x480 / 16 colours is required.");return 1;}
    audio_install();mouse_init();
    if(joylaunch)calibrate();draw_menu();last=ticks-1;
    while(!quit) {
        if(last==ticks){idle();continue;}last=ticks;
        k=input();
        if((inmenu==2&&musicmenushown!=song)||(inmenu==1&&joymenushown!=joyenabled))menu_rows();
        if(k=='['||k==']'){music_seek(k=='['?-1:1);continue;}
        if(k=='-'||k=='+'||k=='='){volume_step(0,k=='-'?-1:1);if(inmenu)menu_rows();continue;}
        if(k==F10) {if(confirm("EXIT TO DOS?"))quit=1;else resume_screen();continue;}
        if(k==F1) {show_help();resume_screen();continue;}
        if(k==F2) {audio_change();if(inmenu)menu_rows();continue;}
        if(k=='m'||k=='M') {music_change();if(inmenu)menu_rows();continue;}
        if(inmenu)menu_input(k);
        else {
            game_input(k);
            if(!inmenu&&!quit&&computer_turn())ai_turn();
        }
    }
    pointer_hide();audio_remove();biosmode(oldmode);
    if(mousefound){union REGS r;memset(&r,0,sizeof(r));int86(0x33,&r,&r);}
    saved=config_save();
    puts("3D Checkers 88 - thanks for playing.");
    if(!saved)puts("Could not save settings. Check free space and write access.");
    return 0;
#endif
}
