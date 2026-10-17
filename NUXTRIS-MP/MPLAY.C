/* NuXTris multiplayer. Original game code, MIT; LAN packet-driver adapter
 * uses GPL-3.0-or-later code adapted from mTCP, so this combined build is
 * distributed under GPL-3.0-or-later.
 * Reuses the finished single-player renderer, controls, sound and rules.
 * The single-player executable and its save file are never overwritten. */
#define main single_player_main
#include "GAME.C"
#undef main
#include "LINK.H"
#include "SERIAL.H"
#include <errno.h>
static unsigned role=1, comnum=1, minutes=5, startlevel, transport;
static unsigned char joinip[4]={192,168,50,203};
static unsigned long baudrate=38400;
static int optflow=1, bridge, showlink, quickstart;
static uint16_t roundnum=1;
static int rematchwant, resceneneeded;
static LDecoder decoder;
static uint32_t nonce, peernonce;
static uint16_t seed, sequence, received, peerack, request, handled, finalseq;
static unsigned phase, peerphase, remaining, lastreceive, lastsend, peerflags;
static unsigned lastcheck;
static unsigned char lastpacket[LSIZE];
static int packetvalid;
static int havepeer, paused, desiredpause, requestpending, finalsent, saved, peersaved;
static int aborting, loss, began, remstarted, started, ended, mpquit;
static int quitconfirm, exitrequested;
static unsigned flash, cleardelay, deadline, badframes;
static int urgent=1;
static char pname[9]="PLAYER", peername[9]="WAITING";
static char resultpath[160], failure[80];
#include "LAN.H"
static Game opponent;
static uint32_t peerscore;
static unsigned char picturecache[2][BH][BW];
static unsigned char boarddirty[2]={1,1};
static char mphud[16][24];
static int nextcache[2]={-1,-1};
static uint32_t scorecache[2]={0xffffffffUL,0xffffffffUL};
static unsigned linecache[2]={65535,65535}, levelcache[2]={65535,65535};
static int secondcache=-1;
static unsigned clockremain=65535, clockboundary;
static unsigned short lastpause;
static unsigned diaglast;
static void mp_round_reset(void);
#ifdef LINKTEST
static int testpause, testresume, testrematch, testinfo;
static unsigned trace[16][4], traced;
static unsigned boardchecks, packets_sent, packets_skipped, clockdivisions;
static unsigned char stale_snapshot[LSIZE];
static int stale_ready,stale_checked;
#endif
#if defined(LINKTEST) || defined(MENUTEST)
static void mp_capture(const char *path) {
    FILE *f=fopen(path,"wb");
    volatile unsigned char __far *v=MK_FP(0xa000,0);
    unsigned p,y,x; unsigned char row[80];
    if(!f) return;
    for(p=0;p<4;p++) {
        outpw(0x3ce,(p<<8)|4);
        for(y=0;y<480;y++) {
            for(x=0;x<80;x++) row[x]=v[y*80+x];
            fwrite(row,1,80,f);
        }
    }
    fclose(f); outpw(0x3ce,4);
}
#endif
static void mp_text(char *cache,int x,int y,const char *s) {
    unsigned masks[7],d,shift;
    int i,r,oldlen,newlen;
    char a,b;
    if(!strcmp(cache,s)) return;
    oldlen=strlen(cache); newlen=strlen(s);
    ink(15);
    for(i=0;i<oldlen || i<newlen;i++,x+=12) {
        a=i<oldlen?cache[i]:' '; b=i<newlen?s[i]:' ';
        if(a==b) continue;
        shift=x&7;
        for(r=0;r<7;r++) { d=glyph(a,r)^glyph(b,r); masks[r]=(hudwide[d>>3]<<6)>>shift; }
        hudpatch(y*80+(x>>3),masks,2);
    }
    strcpy(cache,s);
}
static void mp_hud(int n,int x,int y,const char *s) {
    mp_text(mphud[n],x,y,s);
}
static void mp_small(int n,int x,int y,const char *s) {
    char a[2]={0,0},b[2]={0,0}; unsigned i,oldlen,newlen;
    if(!strcmp(mphud[n],s)) return;
    oldlen=strlen(mphud[n]); newlen=strlen(s); raster(0x18);
    for(i=0;i<oldlen || i<newlen;i++,x+=6) {
        a[0]=i<oldlen?mphud[n][i]:' '; b[0]=i<newlen?s[i]:' ';
        if(a[0]!=b[0]) { textxy(x,y,a,15,1); textxy(x,y,b,15,1); }
    }
    strcpy(mphud[n],s);
}
static void mp_fail(const char *why) {
    if(aborting) return;
    strcpy(failure,why); aborting=1; phase=LABORT; urgent=1; deadline=ticks;
    silence();
}
static void mp_pack(unsigned char *b) {
    unsigned i;
    memset(b,0,LSIZE);
    b[0]='N'; b[1]='T'; b[2]=LVERSION; b[3]=role;
    lput32(b+4,nonce); lput32(b+8,peernonce);
    lput16(b+12,sequence+1); lput16(b+14,received);
    b[16]=phase;
    b[17]=(paused?1:0)|(active?2:0)|(game.over?4:0)|(saved>0?8:0)|(saved<0?16:0)|(rematchwant?32:0);
    lput16(b+18,role?handled:request); b[20]=desiredpause;
    b[21]=startlevel; b[22]=minutes; lput16(b+23,seed);
    lput16(b+25,remaining); lput32(b+27,game.score); lput32(b+31,settledscore);
    lput16(b+35,game.lines); b[37]=game.level; b[38]=game.kind; b[39]=game.next;
    b[40]=game.x; b[41]=game.y;
    for(i=0;i<8;i++) b[42+i]=game.shape[i];
    memcpy(b+50,pname,8);
    for(i=0;i<100;i++) b[58+i]=((unsigned char *)game.grid)[i*2]|(((unsigned char *)game.grid)[i*2+1]<<4);
    lput16(b+158,roundnum);
}
static void mp_finish(void) {
    if(phase!=LRUN) return;
    if(game.full) { clear_rows(); cleardelay=0; }
    settledscore=game.score;
    active=0; phase=LFINISH; urgent=1; ended=ticks;
    boarddirty[role?0:1]=1;
    silence();
}
static void mp_round_reset(void) {
    phase=LCONNECT; peerphase=LCONNECT; peerflags=0;
    paused=desiredpause=requestpending=finalsent=saved=peersaved=0;
    request=handled=finalseq=0;
    aborting=loss=began=remstarted=quitconfirm=exitrequested=rematchwant=0;
    flash=cleardelay=fallticks=0; packetvalid=0; urgent=1;
    failure[0]=0; remaining=(minutes*60UL*1193182UL)/32768UL;
#ifdef LINKTEST
    if(minutes) remaining=110;
    testpause=testresume=0;
#endif
    rng=seed; new_game(0,startlevel,0); settledscore=peerscore=0;
    active=1; opponent=game; resceneneeded=1;
    silence(); inputs_flush();
}
static void mp_accept(const unsigned char *b) {
    unsigned i, incomingphase=b[16], otherboard=role?1:0;
    unsigned char cell, cell2, *grid=(unsigned char *)opponent.grid;
    uint32_t n=lget32(b+4), echo=lget32(b+8);
    uint16_t seq=lget16(b+12), r=lget16(b+18), incominground=lget16(b+158);
#ifdef LINKTEST
    if(traced<16) { trace[traced][0]=phase; trace[traced][1]=incomingphase;
        trace[traced][2]=seq; trace[traced++][3]=echo==nonce; }
#endif
    if(b[3]==role || (echo && echo!=nonce)) return;
    if(peernonce && n!=peernonce) return;
    if(phase>=LREADY && echo!=nonce) return;
    if(havepeer && !lnewer(seq,received)) return;
#ifdef LINKTEST
    if(incominground==1 && incomingphase==LFINISH) {
        memcpy(stale_snapshot,b,LSIZE); stale_ready=1;
    }
#endif
    if(incominground!=roundnum) {
        /* A next-round proposal is accepted only after our own rematch vote
           and completed saves. Older rounds cannot mutate the new board. */
        if(role || phase!=LFINISH || !rematchwant || saved<=0 || !peersaved ||
            roundnum==65535 || incominground!=roundnum+1 || incomingphase!=LREADY) return;
        roundnum=incominground; seed=lget16(b+23);
        mp_round_reset(); phase=LREADY;
    }
    /* Once committed, a different set of rules/seed is never accepted. */
    if(phase>=LREADY && (b[21]!=startlevel || b[22]!=minutes || lget16(b+23)!=seed)) {
        if(role && incomingphase==LCONNECT) return;
        mp_fail("RULES CHANGED - MATCH CANCELLED"); return;
    }
    if(role && phase==LCONNECT && incomingphase!=LCONNECT && echo!=nonce) return;
    if(!role && phase==LCONNECT) {
        startlevel=b[21]; minutes=b[22]; seed=lget16(b+23);
    }
    peernonce=n; received=seq; peerack=lget16(b+14); havepeer=1;
    if((peerflags^b[17])&2 || opponent.kind!=b[38] ||
        opponent.x!=(int8_t)b[40] || opponent.y!=(int8_t)b[41]) boarddirty[otherboard]=1;
    lastreceive=ticks; loss=0; peerphase=incomingphase; peerflags=b[17]; peersaved=(peerflags&8)!=0;
    memcpy(peername,b+50,8); peername[8]=0;
    peerscore=lget32(b+31); opponent.score=lget32(b+27); opponent.lines=lget16(b+35);
    opponent.level=b[37]; opponent.kind=b[38]; opponent.next=b[39];
    opponent.x=(int8_t)b[40]; opponent.y=(int8_t)b[41]; opponent.over=(peerflags&4)!=0;
    for(i=0;i<8;i++) {
        if(opponent.shape[i]!=(int8_t)b[42+i]) boarddirty[otherboard]=1;
        opponent.shape[i]=(int8_t)b[42+i];
    }
    for(i=0;i<100;i++,grid+=2) {
        cell=b[58+i]&15; cell2=b[58+i]>>4;
        if(grid[0]!=cell || grid[1]!=cell2) boarddirty[otherboard]=1;
        grid[0]=cell; grid[1]=cell2;
    }
    if(incomingphase==LABORT) {
        mp_fail(saved>0?"OTHER PLAYER LEFT - LOCAL RESULT SAVED":"OTHER PLAYER LEFT - NOT SAVED"); return;
    }
    if(phase==LCONNECT && (!role || echo==nonce)) {
        rng=seed; new_game(0,startlevel,0); settledscore=0; active=1; phase=LREADY; urgent=1;
        boarddirty[role?0:1]=1;
#ifdef LINKTEST
        if(!minutes) { unsigned y,x; for(y=10;y<20;y++) for(x=0;x<10;x++)
            game.grid[y][x]=x==((y&1)?1:8)?0:3; }
#endif
    }
    if(role) {
        if(phase==LREADY && peerphase==LREADY) { phase=LRUN; began=1; urgent=1; }
        if(lnewer(r,handled)) { handled=r; paused=b[20]; urgent=1; lastpause=ticks; inputs_flush(); }
    } else {
        if(phase==LREADY && peerphase==LRUN) { phase=LRUN; began=1; urgent=1; }
        if(paused!=!!(peerflags&1)) { inputs_flush(); lastpause=ticks; }
        paused=(peerflags&1)!=0;
        if(requestpending && r==request) requestpending=0;
        remaining=lget16(b+25);
        if(phase==LRUN && incomingphase==LFINISH) mp_finish();
    }
    if(phase==LRUN && peerphase>=LRUN) {
        if(!remstarted) { remstarted=1; started=ticks; inputs_flush(); }
    }
}
static void mp_service(void) {
    unsigned budget=1024;
    int c, result, sent;
    unsigned char b[LSIZE];
    if(transport) {
        while(budget-- && (result=lan_read())!=0) {
            if(result>0) {
                uint16_t old=received; int was=havepeer;
                mp_accept(lan_payload);
                if(havepeer && (!was || old!=received)) lan_accept_source();
            } else ++badframes;
        }
    } else {
        while(budget-- && (c=serial_get())>=0) {
            result=ldecode(&decoder,(unsigned)c);
            if(result>0) mp_accept(decoder.data);
            else if(result<0) ++badframes;
        }
        serial_pump();
    }
    if((transport || txpos==txlen) &&
       (urgent || (unsigned)(ticks-lastcheck)>=(transport?4:(baudrate<=9600?18:4))) &&
       (!transport || ticks!=lastcheck)) {
        lastcheck=ticks;
        mp_pack(b);
        if(urgent || !packetvalid || lchanged(b,lastpacket) ||
            (unsigned)(ticks-lastsend)>=(transport?12:(baudrate<=9600?18:12))) {
            lput16(b+LCRC,lcrc(b,LCRC));
            sent=transport?lan_send(b):serial_send(b);
            if(sent) {
                urgent=0; ++sequence; lastsend=ticks;
                memcpy(lastpacket,b,LSIZE); packetvalid=1;
                if(phase==LFINISH && !finalsent) { finalsent=1; finalseq=sequence; }
#ifdef LINKTEST
                ++packets_sent;
            }
            else ++packets_skipped;
#else
            }
#endif
#ifdef LINKTEST
        } else {
            ++packets_skipped;
#endif
        }
    }
    if(havepeer && (unsigned)(ticks-lastreceive)>36) loss=1;
    /* Wait until the user cancels, both before connecting and after a loss.
       Once declared lost, latch it until an accepted snapshot arrives; this
       avoids a 16-bit tick wrap temporarily restarting disconnected play. */
}
static void mp_save(void) {
    FILE *f;
    char key[24],line[160];
    const char *a=role?pname:peername, *b=role?peername:pname;
    uint32_t sa=role?game.score:opponent.score,sb=role?opponent.score:game.score;
    unsigned crc;
    if(saved || phase!=LFINISH || peerphase!=LFINISH || !finalsent ||
        !(peerack==finalseq || lnewer(peerack,finalseq))) return;
    sprintf(key,"%08lX%08lX%04X ",role?nonce:peernonce,role?peernonce:nonce,roundnum);
    /* Text journal with per-record CRC. Match identity makes retry idempotent. */
    f=fopen(resultpath,"rb");
    if(!f && errno!=ENOENT) { saved=-1; return; }
    if(f) {
        while(fgets(line,sizeof(line),f)) {
            char *star=strchr(line,'*');
            if(!strncmp(line,key,21) && star &&
                sscanf(star+1,"%x",&crc)==1 && crc==lcrc((unsigned char *)line,star-line)) {
                fclose(f); saved=1; return;
            }
        }
        if(ferror(f)) { fclose(f); saved=-1; return; }
        fclose(f);
    }
    if(minutes) sprintf(line,"%s%s: %09lu / %s: %09lu | %u MIN LEVEL %u ",key,a,sa,b,sb,minutes,startlevel);
    else sprintf(line,"%s%s: %09lu / %s: %09lu | SUDDEN DEATH LEVEL %u ",key,a,sa,b,sb,startlevel);
    crc=lcrc((unsigned char *)line,strlen(line));
    f=fopen(resultpath,"ab");
    if(!f) { saved=-1; return; }
    /* Leading newline isolates an interrupted previous append. */
    if(fprintf(f,"\r\n%s*%04X\r\n",line,crc)<0) saved=-1;
    if(fflush(f) || ferror(f)) saved=-1;
    if(fclose(f)) saved=-1;
    if(!saved) saved=1;
}
static void mp_next(void) {
    spawn(); active=!game.over; fallticks=0; if(game.over) urgent=1;
    boarddirty[role?0:1]=1;
}
static void mp_play_tick(void) {
    int d,cw,ccw,down,downedge,period,upedge;
    if(game.over) return;
    if(joyenabled) joy_poll();
    upedge=joyup && !oldup; oldup=joyup;
    cw=edge(72)|edge(45)|edge(57)|edge(28)|joyedge|upedge;
    ccw=edge(44)|joyback;
    down=keys[80]||joydown;
    downedge=edge(80)|(joydown&&!olddown); olddown=joydown;
#ifdef LINKTEST
    down=1; /* Exercise real soft-drop, lock, score and snapshot updates. */
    if((ticks&15)==0 && move_side(role?1:-1)) boarddirty[role?0:1]=1;
    if((ticks&31)==0) cw=1;
#endif
    if(cleardelay) {
        if(!--cleardelay) {
            clearnum=clear_rows(); settledscore=game.score;
            sound_event(clearnum==4?4:2);
            if(clearnum==4) flash=54;
            mp_next();
        }
        return;
    }
    d=repeat_dir((keys[77]||joyright)-(keys[75]||joyleft),&moveheld,&moveage);
    if(d && move_side(d)) boarddirty[role?0:1]=1;
    if(cw || ccw) if(rotate_shape(cw?1:0)) boarddirty[role?0:1]=1;
    period=fall_period(); if(down && period>2) period=2;
    if(downedge) fallticks=period;
    if(++fallticks>=(unsigned)period) {
        fallticks=0;
        boarddirty[role?0:1]=1;
        if(!drop_one(down)) {
            clearnum=lock_shape(); settledscore=game.score; active=0; if(game.over) urgent=1;
            sound_event(1);
            if(!game.over) { if(clearnum) cleardelay=8; else mp_next(); }
        }
    }
}
static void mp_pause(int value) {
    if(role) paused=value;
    else if(!requestpending) { ++request; desiredpause=value; requestpending=1; }
    urgent=1; inputs_flush(); silence(); lastpause=ticks;
}
static void mp_tick(void) {
    int escape=edge(1), p=edge(25), enter=(paused || phase==LFINISH)?edge(28):0;
    if(joyenabled && (phase!=LRUN || paused || loss || quitconfirm)) {
        joy_poll(); enter|=joyedge; escape|=joyback;
    }
#ifdef LINKTEST
    /* Exercise Esc confirmation, N cancellation, and the shared pause. */
    if(phase==LRUN && !testinfo) { makes[61]=1; testinfo=1; }
    if(!role && remstarted && !testpause && (unsigned)(ticks-started)>45) {
        testpause=1; escape=1;
    }
    if(!role && quitconfirm && (unsigned)(ticks-started)>63) makes[49]=1;
#endif
    if(exitrequested) { if((unsigned)(ticks-deadline)>36) mpquit=1; return; }
    if(quitconfirm) {
        if(edge(21) || enter) {
            mp_fail(saved>0?"LEFT MATCH - LOCAL RESULT SAVED":"MATCH CANCELLED - NOT SAVED");
            exitrequested=1; deadline=ticks; urgent=1;
        } else if(escape || edge(49)) { quitconfirm=0; inputs_flush(); }
        if(quitconfirm && phase==LRUN && !paused && !requestpending) mp_pause(1);
        return;
    }
    if(escape) {
        quitconfirm=1;
        if(phase==LRUN && !paused && !requestpending) mp_pause(1);
        inputs_flush();
        return;
    }
    if(edge(59)) { theme=(theme+1)%4; settheme(); }
    if(edge(60)) { soundmode=(soundmode+1)%4; silence(); }
    if(edge(61)) { showlink=!showlink; diaglast=ticks-18; }
    if(edge(37)) joy_off();
    if(aborting) return;
    if(phase==LFINISH) {
        mp_save();
        if((enter || edge(19)) && saved>0 && peersaved && !loss && roundnum<65535) {
            rematchwant=1; urgent=1;
        }
        if(role && rematchwant && (peerflags&32) && saved>0 && peersaved && !loss) {
            ++roundnum; seed=(uint16_t)(seed*25173U+13849U); if(!seed) seed=1;
            mp_round_reset(); phase=LREADY;
        }
#ifdef LINKTEST
        if(saved && peersaved && (unsigned)(ticks-ended)>36) {
            if(testrematch && roundnum==1) { rematchwant=1; urgent=1; }
            else mpquit=1;
        }
#endif
        return;
    }
    if(phase!=LRUN) return;
    if(p || (enter && paused)) mp_pause(!paused);
#ifdef LINKTEST
    if(role && paused && !testresume && (unsigned)(ticks-lastpause)>36) {
        testresume=1; mp_pause(0);
    }
#endif
    if(paused || loss || requestpending || !remstarted || peerphase<LREADY) return;
    if(!role && !minutes && (game.over || opponent.over)) return;
    if(role) {
        if(minutes && remaining) --remaining;
        if((minutes && !remaining) || (minutes?(game.over && opponent.over):(game.over || opponent.over))) { mp_finish(); return; }
    }
    if(flash) --flash;
    mp_play_tick();
}
static void mp_board(int n,Game *g,int moving) {
    unsigned char cells[BH][BW];
    int i,x,y,ox=n?428:12;
    if(!boarddirty[n]) return;
    boarddirty[n]=0;
#ifdef LINKTEST
    ++boardchecks;
#endif
    memcpy(cells,g->grid,sizeof(cells));
    if(moving) for(i=0;i<4;i++) {
        x=g->x+g->shape[i*2]; y=g->y+g->shape[i*2+1];
        if(x>=0 && x<BW && y>=0 && y<BH) cells[y][x]=g->kind+1;
    }
    raster(0x18);
    for(y=0;y<BH;y++) for(x=0;x<BW;x++) if(cells[y][x]!=picturecache[n][y][x]) {
        if(picturecache[n][y][x]) block(ox+x*20,52+y*20,picturecache[n][y][x]);
        if(cells[y][x]) block(ox+x*20,52+y*20,cells[y][x]);
        picturecache[n][y][x]=cells[y][x];
    }
}
static void mp_preview(int n,int k) {
    int i,old=nextcache[n],y=n?302:128;
    if(old==k) return;
    raster(0x18);
    if(old>=0) for(i=0;i<4;i++) block(288+shapes[old][2*i]*20,y+shapes[old][2*i+1]*20,old+1);
    for(i=0;i<4;i++) block(288+shapes[k][2*i]*20,y+shapes[k][2*i+1]*20,k+1);
    nextcache[n]=k;
}
static void mp_render(void) {
    Game *g[2]; uint32_t score[2]; int i,seconds; char s[24]; const char *status;
    g[role?0:1]=&game; g[role?1:0]=&opponent;
    score[role?0:1]=settledscore; score[role?1:0]=peerscore;
    mp_board(role?0:1,&game,active);
    if(havepeer) mp_board(role?1:0,&opponent,(peerflags&2)!=0);
    for(i=0;i<2;i++) {
        mp_hud(i,i?432:16,10,i==(role?0:1)?pname:peername);
        if(scorecache[i]!=score[i]) {
            hudnumber(s,score[i],9); mp_hud(2+i,i?432:16,30,s); scorecache[i]=score[i];
        }
        if(linecache[i]!=g[i]->lines || levelcache[i]!=g[i]->level) {
            sprintf(s,"%03u / %02u",g[i]->lines,g[i]->level);
            mp_hud(4+i,244,i?362:188,s); linecache[i]=g[i]->lines; levelcache[i]=g[i]->level;
        }
        mp_preview(i,g[i]->next);
    }
    if(secondcache<0 || (remaining!=clockremain &&
        (remaining>clockremain || remaining<=clockboundary))) {
        seconds=(remaining*32768UL+1193181UL)/1193182UL;
        clockboundary=seconds?((unsigned long)(seconds-1)*1193182UL)/32768UL:0;
        if(seconds!=secondcache) {
            if(minutes) sprintf(s,"%u:%02u",seconds/60,seconds%60); else strcpy(s,"--:--");
            mp_hud(6,292,62,s); secondcache=seconds;
        }
#ifdef LINKTEST
        ++clockdivisions;
#endif
    }
    clockremain=remaining;
    if(quitconfirm) status="LEAVE MATCH?";
    else if(aborting) status="MATCH CLOSED";
    else if(phase<LREADY) status="CONNECTING";
    else if(phase==LREADY || !remstarted) status="GET READY";
    else if(phase==LFINISH) {
        if(saved<0) status="SAVE FAILED";
        else if(peerflags&16) status="PEER DISK ERR";
        else if(saved && (peersaved || loss)) status=game.score==opponent.score?"DRAW":
            ((role?game.score:opponent.score)>(role?opponent.score:game.score)?"LEFT WINS":"RIGHT WINS");
        else status="SAVING";
    } else if(loss) status="LINK WAIT";
    else if(paused || requestpending) status="PAUSED";
    else status=minutes?"SCORE RACE":"SUDDEN DEATH";
    mp_hud(7,244,234,status);
    mp_hud(8,244,262,quitconfirm?"Y:YES N:NO":(flash?"TETRIS!":""));
    mp_hud(9,232,390,phase==LFINISH && saved>0 && peersaved && !loss && roundnum<65535?
        (rematchwant?"WAITING REMATCH":((peerflags&32)?"REMATCH? ENTER":"ENTER: REMATCH")):"");
    mp_hud(11,244,410,phase==LFINISH || paused || loss || aborting?"ESC: MENU":"");
    mp_hud(10,244,430,soundnames[soundmode]);
    if((unsigned)(ticks-diaglast)>=18) {
        diaglast=ticks;
        if(showlink) {
            sprintf(s,"RX AGE %u MS",havepeer?(unsigned)((unsigned long)(unsigned)(ticks-lastreceive)*32768UL/1193UL):0);
            if(!havepeer) strcpy(s,"RX WAITING");
            else if(loss) strcpy(s,"RX LOST");
            mp_hud(12,232,78,s);
            sprintf(s,"REJECT %u",badframes); mp_hud(13,244,207,s);
            if(transport) {
                sprintf(s,"NET DROP %u",lan_dropped); mp_small(14,244,448,s);
                sprintf(s,"NET SEND %u",lan_senterror); mp_small(15,244,458,s);
            } else {
                sprintf(s,"UART %u",uart_errors); mp_small(14,244,448,s);
                sprintf(s,"OVERFLOW %u",rxoverflow); mp_small(15,244,458,s);
            }
        } else {
            mp_hud(12,232,78,""); mp_hud(13,244,207,"");
            mp_small(14,244,448,""); mp_small(15,244,458,"");
        }
    }
}
static void mp_scene(void) {
    memset(picturecache,0,sizeof(picturecache)); memset(mphud,0,sizeof(mphud));
    boarddirty[0]=boarddirty[1]=1;
    nextcache[0]=nextcache[1]=-1; scorecache[0]=scorecache[1]=0xffffffffUL;
    linecache[0]=linecache[1]=levelcache[0]=levelcache[1]=65535;
    secondcache=-1; clockremain=65535; diaglast=ticks-18;
    wipe(); raster(0x10); ink(15);
    rect(8,48,208,408); rect(424,48,208,408);
    textxy(286,42,"TIME",15,1);
    textxy(274,108,"LEFT NEXT",15,1); textxy(274,282,"RIGHT NEXT",15,1);
    textxy(258,174,"LINES / LEVEL",15,1); textxy(258,348,"LINES / LEVEL",15,1);
    textxy(14,458,"ARROWS MOVE  DOWN DROP  X/Z ROTATE",15,1);
    textxy(14,470,"P PAUSE  ESC MENU  F1 THEME  F2 SOUND  F3 LINK",15,1);
    raster(0x18);
}
#include "MPMENU.H"
static void mp_help(void) {
    puts("NUXTRIS MULTIPLAYER - run NXTNET for the menu\n"
         "NXTNET /HOST /NAME:BOB [/COM:1] [/BAUD:38400] [/RTS] [/MIN:5] [/LEVEL:0]\n"
         "NXTNET /JOIN /NAME:JON [/COM:1] [/BAUD:38400] [/RTS]\n"
         "Serial: both ends need the same baud and cable setting. /3WIRE or /RTS.\n"
         "LAN: /LAN /IP:host-address on JOIN; packet driver and MTCPCFG needed.\n"
         "COM1 or COM2, baud 9600/19200/38400/57600/115200. /MIN:1..10, /LEVEL:0..9\n"
         "/J calibrate joystick before connecting. /A AdLib /P speaker /B both /S silent.\n"
         "Five-minute score race by default. Host chooses time and starting level.\n"
         "/SUDDEN: no time limit, first top-out ends match, highest score wins.\n"
         "Both players get the same pieces. Either can pause/resume both.\n"
         "/QUICK skips the menu. /BRIDGE locks physical settings for DOSBOXX.SH.\n"
         "Paired results: NXTMATCH.TXT. Settings: NXTMULT.CFG. F3 toggles link info.");
}
int main(int argc,char **argv) {
    unsigned i,oldmode,last,now,elapsed; int joylaunch=0,namegiven=0;
    uint32_t option;
    struct dostime_t dt; struct dosdate_t dd;
    mp_paths(argv[0]); mp_cfgload();
    for(i=1;i<(unsigned)argc;i++) {
        const char *v=strchr(argv[i],':');
        if(!stricmp(argv[i],"/HOST")) { role=1; }
        else if(!stricmp(argv[i],"/JOIN")) { role=0; }
        else if(!strnicmp(argv[i],"/NAME:",6)) {
            if(!v[1] || strlen(v+1)>8) { mp_help(); return 1; }
            memset(pname,0,sizeof(pname)); strcpy(pname,v+1); strupr(pname); namegiven=1;
        }
        else if(!strnicmp(argv[i],"/COM:",5)) {
            if(!lnumber(v+1,2,&option) || !option) { mp_help(); return 1; } comnum=option;
        }
        else if(!strnicmp(argv[i],"/BAUD:",6)) {
            if(!lnumber(v+1,115200UL,&option)) { mp_help(); return 1; } baudrate=option;
        }
        else if(!stricmp(argv[i],"/LAN")) transport=1;
        else if(!stricmp(argv[i],"/SERIAL")) transport=0;
        else if(!strnicmp(argv[i],"/IP:",4)) {
            if(!lan_parseip(v+1,joinip)) { mp_help(); return 1; }
        }
        else if(!strnicmp(argv[i],"/MIN:",5)) {
            if(!lnumber(v+1,10,&option) || !option) { mp_help(); return 1; } minutes=option;
        }
        else if(!strnicmp(argv[i],"/LEVEL:",7)) {
            if(!lnumber(v+1,9,&option)) { mp_help(); return 1; } startlevel=option;
        }
        else if(!stricmp(argv[i],"/SUDDEN")) minutes=0;
        else if(!stricmp(argv[i],"/RTS")) optflow=1;
        else if(!stricmp(argv[i],"/3WIRE")) optflow=0;
        else if(!stricmp(argv[i],"/QUICK")) quickstart=1;
        else if(!stricmp(argv[i],"/BRIDGE")) bridge=1;
#ifdef LINKTEST
        else if(!stricmp(argv[i],"/TESTREMATCH")) testrematch=1;
#endif
        else if(!stricmp(argv[i],"/J")) joylaunch=1;
        else if(!stricmp(argv[i],"/A")) soundmode=1;
        else if(!stricmp(argv[i],"/P")) soundmode=2;
        else if(!stricmp(argv[i],"/B")) soundmode=3;
        else if(!stricmp(argv[i],"/S")) soundmode=0;
        else { mp_help(); return stricmp(argv[i],"/?")?1:0; }
    }
    if(!cfgloaded && !namegiven) strcpy(pname,role?"AMIGO":"PLAYER2");
    if(bridge) comnum=1;
#ifdef LINKTEST
    quickstart=1;
#endif
    if((comnum!=1 && comnum!=2) || minutes>10 || startlevel>9 ||
        !lname((unsigned char *)pname) || (baudrate!=9600 && baudrate!=19200 &&
        baudrate!=38400 && baudrate!=57600 && baudrate!=115200)) { mp_help(); return 1; }
    oldmode=modequery();
    if(!video_start()) { biosmode(oldmode); puts("Cannot set VGA 640x480."); return 1; }
    speakerbits=inp(0x61)&3; audiofound=adlib();
    if(audiofound) instruments(); else if(soundmode==1) soundmode=2;
    install();
    if(joylaunch) joy_calibrate();
    for(;;) {
    if(!quickstart && !mp_menu()) break;
    quickstart=0; uinotice[0]=0;
    _dos_gettime(&dt); _dos_getdate(&dd);
    nonce=*(uint32_t __far *)MK_FP(0x40,0x6c) ^ ((uint32_t)dd.year<<20) ^
        ((uint32_t)dd.month<<16) ^ ((uint32_t)dd.day<<11) ^ ((uint32_t)dt.hsecond<<24) ^
        ((uint32_t)inp(0x40)<<8) ^ ((uint32_t)ticks<<3) ^ (role?0x71234a51UL:0x42bc9231UL);
    if(!nonce) nonce=1;
    seed=(uint16_t)(nonce^(nonce>>16)); if(!seed) seed=1;
    peernonce=0; havepeer=0; sequence=received=peerack=0; roundnum=1;
    badframes=0; mpquit=0; strcpy(peername,"WAITING");
    memset(&decoder,0,sizeof(decoder)); mp_round_reset();
    flowcontrol=bridge?0:optflow;
    if(transport) {
        if(!lan_open(joinip)) { strcpy(uinotice,lan_reason); continue; }
    } else if(!serial_open(comnum,baudrate)) {
        strcpy(uinotice,"COM PORT NOT FOUND - CHECK CONNECTION"); continue;
    }
    lastreceive=lastsend=lastcheck=ticks; mp_scene(); resceneneeded=0; last=ticks;
    while(!mpquit) {
        mp_service();
        now=ticks; elapsed=(unsigned)(now-last);
        if(!elapsed) continue;
        last=now;
        if(elapsed>18) { if(phase==LRUN) mp_pause(1); elapsed=1; }
        while(elapsed-- && !mpquit) { audio_tick(); mp_tick(); mp_service(); }
        if(resceneneeded) { mp_scene(); resceneneeded=0; }
        mp_render();
#ifdef LINKTEST
        if(roundnum==2 && phase==LRUN && stale_ready && !stale_checked) {
            unsigned oldseq=received; uint32_t oldscore=opponent.score;
            lput16(stale_snapshot+12,received+1);
            lput16(stale_snapshot+LCRC,lcrc(stale_snapshot,LCRC));
            mp_accept(stale_snapshot); stale_checked=1;
            if(received!=oldseq || opponent.score!=oldscore || phase!=LRUN) {
                mp_fail("STALE ROUND ACCEPTED"); mpquit=1;
            }
        }
        if((unsigned)(ticks-lastreceive)>600 || ticks>1200) { mp_fail("AUTOTEST TIMEOUT"); mpquit=1; }
#endif
    }
#ifdef LINKTEST
    mp_capture("SCREEN.RAW");
#endif
#ifdef LINKTEST
    { FILE *diag=fopen("UART.TXT","w"); if(diag) {
        fprintf(diag,"IRQcount %u head %u tail %u seq %u accepted %u phase %u IER %02X IIR %02X LSR %02X PIC %02X FIFO %d\n",
            irqcount,rxhead,rxtail,sequence,received,phase,inp(portbase+1),inp(portbase+2),inp(portbase+5),inp(0x21),hasfifo);
        for(i=0;i<traced;i++) fprintf(diag,"phase %u incoming %u seq %u echo %u\n",trace[i][0],trace[i][1],trace[i][2],trace[i][3]);
        fclose(diag); }
    }
#endif
    if(transport) lan_close(); else serial_close();
    silence();
#ifdef LINKTEST
    break;
#else
    mp_cfgsave(); inputs_flush();
#endif
    }
    uninstall();
    outp(0x61,(inp(0x61)&0xfc)|speakerbits); biosmode(oldmode);
    printf("%s\n",failure[0]?failure:(saved>0?(peersaved?"Match complete. Both scores saved.":"Local result saved; other player save unconfirmed."):"Match ended."));
    if(saved>0) printf("Results: %s\n",resultpath);
    if(saved<0) printf("Could not save %s\n",resultpath);
    if(transport) printf("Rejected frames: %u  network drops: %u  send errors: %u\n",
        badframes,lan_dropped,lan_senterror);
    else {
        printf("Rejected frames: %u  UART errors: %u  receive overflows: %u\n",badframes,uart_errors,rxoverflow);
        printf("UART: overrun %u  parity %u  framing %u  break %u\n",
            uart_overrun,uart_parity,uart_framing,uart_break);
    }
#ifdef LINKTEST
    printf("TEST role=%u phase=%u peer=%u saved=%d peersaved=%d score=%lu other=%lu pause=%d resume=%d remaining=%u dead=%d otherdead=%d round=%u\n",
        role,phase,peerphase,saved,peersaved,game.score,opponent.score,testpause,testresume,remaining,game.over,opponent.over,roundnum);
    printf("COST ticks=%u boardchecks=%u packets=%u skipped=%u clockdiv=%u\n",ticks,boardchecks,packets_sent,packets_skipped,clockdivisions);
    printf("REMATCH stale_checked=%d info=%d\n",stale_checked,showlink);
#endif
    return aborting || saved<0 ? 1:0;
}
