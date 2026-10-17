/* Multiplayer menus and local preferences. Original code, MIT.
   No transport ownership while browsing or calibrating. */
#define MCFSIZE 32
static const unsigned long mpbauds[5]={9600,19200,38400,57600,115200};
static char cfgpath[160],cfgback[160],cfgtmp[160];
static unsigned char cfgcached[MCFSIZE];
static int cfgloaded,cfgerror,menupage,mpselect=1,editing,editpos,editfresh;
static char editname[9],editip[16],menucache[10][40],markcache[10][2],menunote[48];
static int ippos,ipfresh;
static char namecursor[24];
static char uinotice[48];
static void mp_paths(const char *exe) {
    unsigned i,n=0;
    for(i=0;exe[i] && i<140;i++) if(exe[i]=='\\' || exe[i]=='/' || exe[i]==':') n=i+1;
    memcpy(resultpath,exe,n); strcpy(resultpath+n,"NXTMATCH.TXT");
    memcpy(cfgpath,exe,n); strcpy(cfgpath+n,"NXTMULT.CFG");
    memcpy(cfgback,exe,n); strcpy(cfgback+n,"NXTMULT.BAK");
    memcpy(cfgtmp,exe,n); strcpy(cfgtmp+n,"NXTMULT.NEW");
}
static int mp_cfgread(const char *path,unsigned char *b) {
    FILE *f=fopen(path,"rb"); int ok;
    if(!f) return 0;
    ok=fread(b,1,MCFSIZE,f)==MCFSIZE;
    if(fgetc(f)!=EOF || ferror(f)) ok=0;
    fclose(f);
    return ok && !memcmp(b,"NXTM",4) && (b[4]==1 || b[4]==2) && b[5]<2 &&
        b[6]>=1 && b[6]<=2 && b[7]<5 && b[8]<2 && b[9]<=10 &&
        b[10]<=9 && b[11]<4 && b[12]<4 && b[13]<2 && b[14] &&
        lname(b+14) && (b[4]==1 || (b[22]<2 &&
        b[23]>0 && b[23]<224)) &&
        lget16(b+30)==lcrc(b,30);
}
static void mp_cfgload(void) {
    unsigned char b[MCFSIZE];
    if(!mp_cfgread(cfgpath,b) && !mp_cfgread(cfgback,b)) return;
    role=b[5]; comnum=b[6]; baudrate=mpbauds[b[7]]; optflow=b[8];
    minutes=b[9]; startlevel=b[10]; theme=b[11]; soundmode=b[12]; showlink=b[13];
    memcpy(pname,b+14,8); pname[8]=0;
    if(b[4]==2) { transport=b[22]; memcpy(joinip,b+23,4); }
    memcpy(cfgcached,b,MCFSIZE); cfgloaded=1;
}
static void mp_cfgsave(void) {
    unsigned char b[MCFSIZE],old[MCFSIZE]; unsigned i; FILE *f; int ok;
    memset(b,0,sizeof(b)); memcpy(b,"NXTM",4); b[4]=2;
    b[5]=role; b[6]=comnum;
    for(i=0;i<5;i++) if(baudrate==mpbauds[i]) b[7]=i;
    b[8]=optflow; b[9]=minutes; b[10]=startlevel;
    b[11]=theme; b[12]=soundmode; b[13]=showlink; memcpy(b+14,pname,8);
    b[22]=transport; memcpy(b+23,joinip,4);
    lput16(b+30,lcrc(b,30));
    if(cfgloaded && !memcmp(b,cfgcached,MCFSIZE)) { cfgerror=0; return; }
    cfgerror=1; f=fopen(cfgtmp,"wb"); if(!f) return;
    ok=fwrite(b,1,MCFSIZE,f)==MCFSIZE;
    if(fflush(f) || ferror(f)) ok=0;
    if(fclose(f)) ok=0;
    if(!ok) { remove(cfgtmp); return; }
    if(mp_cfgread(cfgpath,old)) {
        if(access(cfgback,0)==0 && remove(cfgback)) { remove(cfgtmp); return; }
        if(rename(cfgpath,cfgback)) { remove(cfgtmp); return; }
    } else if(access(cfgpath,0)==0 && remove(cfgpath)) { remove(cfgtmp); return; }
    if(rename(cfgtmp,cfgpath)) { remove(cfgtmp); return; }
    memcpy(cfgcached,b,MCFSIZE); cfgloaded=1; cfgerror=0;
}
static void mp_menu_scene(void) {
    memset(menucache,0,sizeof(menucache)); memset(markcache,0,sizeof(markcache));
    menunote[0]=namecursor[0]=0; wipe(); raster(0x10); logo(22); ink(15);
    centered(60,menupage?"CONNECTION":"MULTIPLAYER",15);
    rect(44,92,552,284);
    textxy(80,398,"UP/DOWN SELECT   LEFT/RIGHT CHANGE   ENTER/BUTTON 1 CHOOSE",15,1);
    textxy(80,414,"J CALIBRATE   K KEYBOARD   ESC/BUTTON 2 BACK",15,1);
    raster(0x18);
}
static void mp_menu_draw(void) {
    char s[40]; int i;
    for(i=0;i<(menupage?7:10);i++) {
        if(menupage) switch(i) {
        case 0: sprintf(s,"ROLE       %s",role?"HOST":"JOIN"); break;
        case 1: sprintf(s,"LINK       %s",transport?"LAN":"SERIAL"); break;
        case 2: sprintf(s,"PORT       %s",transport?"SERIAL ONLY":comnum==1?"COM1":"COM2"); break;
        case 3: if(transport) strcpy(s,"BAUD       SERIAL ONLY");
            else sprintf(s,"BAUD       %lu",baudrate); break;
        case 4: sprintf(s,"FLOW       %s",transport?"SERIAL ONLY":optflow?"RTS/CTS":"3-WIRE"); break;
        case 5: if(editing==2) sprintf(s,"PEER IP    %s",editip);
            else if(transport && role) strcpy(s,"PEER IP    AUTO");
            else if(transport) sprintf(s,"PEER IP    %u.%u.%u.%u",joinip[0],joinip[1],joinip[2],joinip[3]);
            else strcpy(s,"PEER IP    LAN ONLY"); break;
        default: strcpy(s,"BACK"); break;
        } else switch(i) {
        case 0: sprintf(s,"NAME       %s",editing?editname:pname); break;
        case 1: strcpy(s,role?"HOST GAME":"JOIN GAME"); break;
        case 2: if(transport) sprintf(s,"CONNECTION %s LAN",role?"HOST":"JOIN");
            else sprintf(s,"CONNECTION %s COM%u",role?"HOST":"JOIN",comnum); break;
        case 3: if(!role) strcpy(s,"MODE       CHOSEN BY HOST");
            else if(minutes) sprintf(s,"MODE       %u MIN SCORE RACE",minutes);
            else strcpy(s,"MODE       SUDDEN DEATH"); break;
        case 4: if(role) sprintf(s,"LEVEL      %u",startlevel);
            else strcpy(s,"LEVEL      CHOSEN BY HOST"); break;
        case 5: sprintf(s,"INPUT      %s",joyenabled?"JOYSTICK":"KEYBOARD"); break;
        case 6: sprintf(s,"THEME      %s",themenames[theme]); break;
        case 7: sprintf(s,"SOUND      %s",soundnames[soundmode]); break;
        case 8: sprintf(s,"LINK INFO  %s",showlink?"ON":"OFF"); break;
        default: strcpy(s,"EXIT TO DOS"); break;
        }
        mp_text(markcache[i],62,108+i*26,i==mpselect?">":"");
        mp_text(menucache[i],88,108+i*26,s);
    }
    if(editing==1) {
        strcpy(s,"           "); memset(s+11,' ',8); s[19]=0; s[11+editpos]='_';
        mp_text(namecursor,88,112,s);
        mp_text(menunote,56,446,"TYPE NAME - ENTER OK - ESC CANCEL");
    } else if(editing==2) {
        mp_text(menunote,56,446,uinotice[0]?uinotice:"TYPE IP ADDRESS - ENTER OK - ESC CANCEL");
    } else {
        const char *note=uinotice[0]?uinotice:(cfgerror?"SETTINGS NOT SAVED - CHECK DISK":
            (menupage && bridge?"PORT/SPEED/FLOW: USE DOSBOXX.SH":
            (!audiofound && (soundmode&1)?"NO ADLIB - PC SPEAKER AVAILABLE":"")));
        mp_text(menunote,56,446,note);
    }
}
static void mp_name_begin(void) {
    unsigned n=strlen(pname);
    memset(editname,' ',8); memcpy(editname,pname,n); editname[8]=0;
    editpos=0; editfresh=1; editing=1; inputs_flush();
    mp_menu_scene();
}
static void mp_ip_begin(void) {
    sprintf(editip,"%u.%u.%u.%u",joinip[0],joinip[1],joinip[2],joinip[3]);
    ippos=0; ipfresh=1; editing=2; uinotice[0]=0; inputs_flush(); mp_menu_scene();
}
static void mp_ip_tick(int lr,int ud,int enter,int esc) {
    static const char choices[]="0123456789.";
    int i,c=0,n;
    if(esc) { editing=0; inputs_flush(); mp_menu_scene(); return; }
    if(enter) {
        unsigned char proposed[4];
        if(lan_parseip(editip,proposed)) {
            memcpy(joinip,proposed,4); editing=0; inputs_flush(); mp_menu_scene();
        } else strcpy(uinotice,"ENTER AN IP LIKE 192.168.50.203");
        return;
    }
    for(i=2;i<=11;i++) if(edge(i)) c=i==11?'0':'1'+i-2;
    if(edge(52) || edge(83)) c='.';
    if(c) {
        if(ipfresh) { editip[0]=0; ippos=0; ipfresh=0; }
        n=strlen(editip); if(n<15) {
            editip[n]=c; editip[n+1]=0; ippos=n; uinotice[0]=0;
        }
    }
    if(edge(14)) { n=strlen(editip); if(n) editip[n-1]=0; ippos=n?n-1:0; ipfresh=0; uinotice[0]=0; }
    if(lr) { n=strlen(editip); if(n) ippos=(ippos+n+lr)%n; ipfresh=0; }
    if(ud) {
        const char *p=strchr(choices,editip[ippos]);
        n=p?(int)(p-choices):0;
        if(editip[0]) editip[ippos]=choices[(n+11-ud)%11];
        ipfresh=0;
    }
}
static void mp_name_tick(int lr,int ud,int enter,int esc) {
    static const unsigned char scan[26]={16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50};
    static const char letters[]="QWERTYUIOPASDFGHJKLZXCVBNM";
    static const char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int i,c=0,n;
    if(enter || esc) {
        if(enter) {
            for(n=8;n>0 && editname[n-1]==' ';--n) {}
            if(n) { memset(pname,0,9); memcpy(pname,editname,n); }
        }
        editing=0; inputs_flush(); mp_menu_scene(); return;
    }
    for(i=0;i<26;i++) if(edge(scan[i])) c=letters[i];
    for(i=2;i<=11;i++) if(edge(i)) c=i==11?'0':'1'+i-2;
    if(edge(57)) c=' ';
    if(c) {
        if(editfresh) { memset(editname,' ',8); editpos=0; editfresh=0; }
        editname[editpos]=c; if(editpos<7) ++editpos;
    }
    if(edge(14)) { if(editpos) --editpos; editname[editpos]=' '; editfresh=0; }
    if(lr) { editpos=(editpos+8+lr)%8; editfresh=0; }
    if(ud) {
        const char *p=strchr(alphabet,editname[editpos]); n=p?(int)(p-alphabet):0;
        editname[editpos]=alphabet[(n+37-ud)%37]; editfresh=0;
    }
}
#ifdef MENUTEST
/* Drive real menu controls in the disposable emulator harness. */
static void mp_menu_test(void) {
    static const unsigned char script[]={
        72,28,48,24,48,28, /* NAME -> BOB */
        80,80,28,77,80,77, /* CONNECTION: join LAN */
        80,80,80,80, /* peer IP row */
        28,1, /* open then cancel IP edit */
        80,28, /* BACK */
        80,80,80,80,80,80,80,28 /* EXIT AND SAVE */
    };
    static unsigned step,last;
    memset((void *)keys,0,sizeof(keys));
    if((unsigned)(ticks-last)<8) return;
    if(cfgloaded) {
        if(!step++) { mp_capture("RESTORE.RAW"); makes[1]=1; }
        return;
    }
    if(step==5) mp_capture("NAME.RAW");
    if(step==16) mp_capture("CONNECT.RAW");
    if(step==20) mp_capture("MENU.RAW");
    last=ticks;
    if(step<sizeof(script)) { keys[script[step]]=1; makes[script[step++]]=1; }
}
#endif
static int mp_menu(void) {
    unsigned last=ticks; int lr,ud,enter,esc,i,pressed;
    menupage=editing=0; mpselect=1; inputs_flush(); mp_menu_scene(); mp_menu_draw();
    for(;;) {
        if(ticks==last) continue; last=ticks;
#ifdef MENUTEST
        mp_menu_test();
#endif
        if(joyenabled) joy_poll();
        enter=edge(28)|joyedge; esc=edge(1)|joyback;
        lr=repeat_dir((keys[77]||joyright)-(keys[75]||joyleft),&moveheld,&moveage);
        ud=repeat_dir((keys[80]||joydown)-(keys[72]||joyup),&uiheld,&uiage);
        pressed=edge(77)-edge(75); if(pressed && !lr) lr=pressed;
        pressed=edge(80)-edge(72); if(pressed && !ud) ud=pressed;
        if(editing) {
            if(editing==1) mp_name_tick(lr,ud,enter,esc);
            else mp_ip_tick(lr,ud,enter,esc);
            mp_menu_draw(); continue;
        }
        if(esc) {
            if(menupage) { menupage=0; mpselect=2; inputs_flush(); mp_menu_scene(); }
            else { mp_cfgsave(); return 0; }
        }
        if(edge(36) || (!menupage && mpselect==5 && enter)) {
            joy_calibrate(); inputs_flush(); last=ticks; mp_menu_scene();
        }
        if(edge(37)) { joy_off(); inputs_flush(); }
        if(ud) { mpselect=(mpselect+(menupage?7:10)+ud)%(menupage?7:10); uinotice[0]=0; }
        if(enter || lr) {
            int d=lr?lr:1;
            if(menupage) {
                if(mpselect==0) role=!role;
                else if(mpselect==1) transport=!transport;
                else if(mpselect==6) { menupage=0; mpselect=2; inputs_flush(); mp_menu_scene(); }
                else if(mpselect==5 && transport && !role && enter) mp_ip_begin();
                else if(transport) strcpy(uinotice,"LAN USES MTCPCFG AND PACKET DRIVER");
                else if(bridge) strcpy(uinotice,"PORT/SPEED/FLOW: USE DOSBOXX.SH");
                else if(mpselect==2) comnum=3-comnum;
                else if(mpselect==3) {
                    for(i=0;i<5;i++) if(mpbauds[i]==baudrate) break;
                    baudrate=mpbauds[(i+5+d)%5];
                } else if(mpselect==4) optflow=!optflow;
            } else switch(mpselect) {
            case 0: if(enter) mp_name_begin(); break;
            case 1: if(enter) { mp_cfgsave(); return 1; } break;
            case 2: menupage=1; mpselect=0; inputs_flush(); mp_menu_scene(); break;
            case 3: if(role) minutes=(minutes+11+d)%11; break;
            case 4: if(role) startlevel=(startlevel+10+d)%10; break;
            case 5: break;
            case 6: theme=(theme+4+d)%4; settheme(); break;
            case 7: soundmode=(soundmode+4+d)%4; silence(); break;
            case 8: showlink=!showlink; break;
            case 9: if(enter) { mp_cfgsave(); return 0; } break;
            }
        }
        mp_menu_draw();
    }
}
