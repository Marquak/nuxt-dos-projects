/* Independent full redraw references retained from version 1.1. */
static void reference_menu(void) {
    char s[60];
    int i;
    wipe();
    logo(26);
    ink(15);
    rect(150, 94, 340, 238);
    for (i = 0; i < 6; ++i) {
        switch (i) {
        case 0:
            sprintf(s, "MODE    %s", opttype == MARATHON ? "MARATHON" :
                opttype == SPRINT ? "SPRINT 40" : "B-TYPE");
            break;
        case 1:
            sprintf(s, "LEVEL   %u", optlevel);
            break;
        case 2:
            sprintf(s, "HEIGHT  %u%s", optheight, opttype == BTYPE ? "" : "  - B ONLY");
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
        textxy(176, 112 + i * 36, i == menusel ? ">" : " ", 15, 2);
        textxy(202, 112 + i * 36, s, 15, 2);
    }
    centered(350, "UP/DOWN SELECT - LEFT/RIGHT CHANGE", 15);
    centered(378, "ENTER OR BUTTON 1: PLAY", 15);
    centered(410, "H SCORES  J CALIBRATE  ESC EXIT", 15);
    if (savefailed)
        centered(450, "SAVE FAILED - CHECK DISK SPACE/WRITE ACCESS", 15);
    else if (recovered)
        centered(450, "SCORES RECOVERED FROM BACKUP", 15);
    else if (imported)
        centered(450, "OLD SCORES READY - SEE LEGACY B TABLE", 15);
    else if (!audiofound && (soundmode & 1))
        centered(450, "NO ADLIB DETECTED - SPEAKER AVAILABLE", 15);
}
static void reference_name(void) {
    char s[60];
    int i;
    wipe();
    logo(32);
    centered(102, won ? "SPRINT COMPLETE!" : "GAME OVER", 15);
    sprintf(s, "SCORE %lu  -  HIGH SCORE %d", (unsigned long)game.score, pendingplace + 1);
    centered(145, s, 15);
    centered(204, "ENTER YOUR NAME", 15);
    for (i = 0; i < 8; ++i) {
        char a[2];
        a[0] = player[i] ? player[i] : ' ';
        a[1] = 0;
        textxy(230 + i * 24, 249, a, 15, 3);
    }
    ink(15);
    segment(228 + (namepos < 8 ? namepos : 7) * 24, 278, 246 + (namepos < 8 ? namepos : 7) * 24, 278);
    centered(321, "TYPE NAME - ENTER SAVES", 15);
    centered(350, "PAD: LEFT/RIGHT CURSOR, UP/DOWN LETTER", 15);
    centered(379, "BUTTON 1 SAVES - ESC USES CURRENT NAME", 15);
}

static void ui_checks(void) {
    static const unsigned scans8[8] = {30,48,46,32,18,33,34,35};
    unsigned n, g; unsigned long h; int i, ok;
    memset((void *)makes,0,sizeof(makes));
    namefresh=1;namepos=0;strcpy(player,"PLAYER");
    for(i=0;i<8;i++){makes[scans8[i]]=1;name_tick(0,0,0,0);}
    check(!strcmp(player,"ABCDEFGH")&&namepos==8,"Full name retains an end insertion position");
    makes[30]=1;name_tick(0,0,0,0);
    check(!strcmp(player,"ABCDEFGH"),"Ninth typed letter cannot overwrite a full name");
    makes[14]=1;name_tick(0,0,0,0);
    check(!strcmp(player,"ABCDEFG")&&namepos==7,"Backspace after eight letters deletes last letter");
    makes[35]=1;name_tick(0,0,0,0);
    check(!strcmp(player,"ABCDEFGH")&&namepos==8,"Typing after deletion refills last position");
    name_tick(-1,0,0,0);check(namepos==7,"Left from end selects last letter");
    namepos=8;name_tick(1,0,0,0);check(namepos==0,"Right from end wraps to first letter");
    namepos=8;name_tick(0,-1,0,0);check(namepos==7&&!strcmp(player,"ABCDEFGI"),"Pad edits last letter safely after full keyboard entry");
    for(i=0;i<10;i++){makes[14]=1;name_tick(0,0,0,0);}
    check(namepos==0&&player[8]==0,"Repeated backspace stays in name buffer");
    state=MENU;menusel=0;theme=0;settheme();soundmode=3;audiofound=1;
    opttype=optheight=optlevel=savefailed=recovered=joyenabled=0;
    wipe();menu_draw();h=planehash();reference_menu();
    check(planehash()==h,"Initial cached menu matches full renderer");
    menu_draw();n=uiclears;g=uiglyphs;menusel=1;menu_draw();
    check(uiclears==n&&uiglyphs==g,"Menu selection changes only arrow pixels without clearing");
    h=planehash();reference_menu();check(planehash()==h,"Menu selection erases previous arrow exactly");
    ok=1;
    for(i=0;i<5;i++){
        menu_draw();n=uiclears;menusel=i;changemenu(1);menu_draw();
        /* Avoid hardware calibration in INPUT option. */
        if(uiclears!=n)ok=0;
        h=planehash();reference_menu();if(planehash()!=h)ok=0;
    }
    check(ok,"Changed menu fields match full redraw without screen clears");
    menu_draw();n=uiclears;g=uiglyphs;menu_draw();
    check(n==uiclears&&g==uiglyphs,"Unchanged menu performs no clears or glyph updates");
    savefailed=1;menu_draw();h=planehash();reference_menu();check(h==planehash(),"Save failure notice matches full draw");
    menu_draw();savefailed=0;recovered=1;menu_draw();h=planehash();reference_menu();check(h==planehash(),"Shorter centred notice removes previous text");
    menu_draw();recovered=0;menu_draw();h=planehash();reference_menu();check(h==planehash(),"Notice removal leaves no pixels");
    wipe();n=uiclears;menu_draw();check(uiclears==n+1,"External screen clear invalidates UI cache for calibration return");
    state=NAME;pendingplace=0;won=0;game.score=1234;strcpy(player,"ABCDEFGH");namepos=8;
    wipe();name_draw();h=planehash();reference_name();check(h==planehash(),"Full name and end cursor match full renderer");
    name_draw();n=uiclears;g=uiglyphs;makes[14]=1;name_tick(0,0,0,0);name_draw();
    check(n==uiclears&&uiglyphs==g+1,"Backspace redraws just one name glyph");
    h=planehash();reference_name();check(h==planehash(),"Deleted last letter leaves exact reference pixels");
    name_draw();n=uiclears;g=uiglyphs;name_tick(-1,0,0,0);name_draw();
    check(n==uiclears&&g==uiglyphs,"Name cursor movement does not redraw text or screen");
    h=planehash();reference_name();check(h==planehash(),"Moving cursor erases old underline exactly");
    name_draw();n=uiclears;g=uiglyphs;name_draw();check(n==uiclears&&g==uiglyphs,"Unchanged name screen does no drawing");
    namefresh=1;makes[30]=1;name_tick(0,0,0,0);name_draw();h=planehash();reference_name();
    check(h==planehash(),"Replacing previous name removes all old trailing letters");
    joyenabled=0;savefailed=recovered=0;opttype=optheight=optlevel=0;soundmode=0;
}
