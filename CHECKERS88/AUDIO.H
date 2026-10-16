/* Compact MIDI-derived classical playlist, eight FM voices plus independent FX.
 * IRQ0 owns playback, including while the computer searches or VGA is drawn.
 * UI submits effects by one byte; no interrupt-disabled music/drawing loops. */
#include "MUSIC.H"
static volatile unsigned ticks;
static volatile U8 soundmode=1,music=1,pendingfx;
static int audiofound;
static volatile U8 song,shuffle,musicvolume=8,fxvolume=8,musrequest,volumechange;
static U8 musiclevel[8],musicplaying[8];
static unsigned shuffle_state=0x4831;
static const U8 musicops[8]={1,2,8,9,10,16,17,18};
static U8 opllife[9],spkleft,fxkind,fxpos,fxwait,muswait;
static unsigned musstep;
static U8 oldspeaker,timerhalf;
static void (__interrupt __far *oldtimer)(void);
static const unsigned fnums[12]={343,363,385,408,432,458,485,514,544,577,611,647};
static const unsigned divisor[12]={9121,8609,8126,7670,7239,6833,6449,6087,5746,5424,5120,4832};
static void opl(unsigned reg,unsigned val) {
    int i;
    outp(0x388,reg); for(i=0;i<6;++i) inp(0x388);
    outp(0x389,val); for(i=0;i<35;++i) inp(0x388);
}
static int adlib_detect(void) {
    unsigned a,b;int i;
    opl(4,0x60);opl(4,0x80);a=inp(0x388);
    opl(2,255);opl(4,0x21);for(i=0;i<240;++i) inp(0x388);
    b=inp(0x388);opl(4,0x60);opl(4,0x80);
    return !(a&0xe0)&&(b&0xe0)==0xc0;
}
static void speaker_off(void) {outp(0x61,inp(0x61)&0xfc);spkleft=0;}
static void silence(void) {
    int i;
    if(audiofound) for(i=0;i<9;++i) opl(0xb0+i,0);
    memset(opllife,0,sizeof(opllife));speaker_off();fxkind=fxwait=pendingfx=0;
}
static unsigned music_level(int c) {
    return musicvolume?musiclevel[c]+(10-musicvolume)*4:63;
}
static void music_off(void) {
    int i;for(i=0;i<8;++i) {if(audiofound)opl(0xb1+i,0);musicplaying[i]=0;}
}
static void music_start(int index) {
    music_off();song=(U8)index;musstep=0;muswait=tunes[song][0].delay;
}
static void music_seek(int direction) {
    unsigned next;
    /* Keep the pending request stable while IRQ0 can consume it. Only these
     * few memory/arithmetic instructions need protection, not OPL writes. */
    _disable();
    next=musrequest?musrequest-1:song;
    musrequest=(U8)(((next+direction)&3)+1);
    _enable();
}
static void volume_step(int effects,int direction) {
    int n=(effects?fxvolume:musicvolume)+direction;
    if(n<0)n=0;
    if(n>10)n=10;
    if(effects)fxvolume=(U8)n;else musicvolume=(U8)n;
    volumechange=1;
}
static void instruments(void) {
    int i,o;
    opl(1,32);opl(0xbd,0);
    for(i=0;i<9;++i) {
        o=i?musicops[i-1]:0;
        /* Original rounded, gently percussive FM piano; FX keeps its own voice. */
        opl(0x20+o,0x21);opl(0x23+o,0x21);
        opl(0x40+o,i?28:32);opl(0x43+o,i?24:(fxvolume? (10-fxvolume)*4:63));
        opl(0x60+o,0xf2);opl(0x63+o,i?0xf3:0xf4);
        opl(0x80+o,0x76);opl(0x83+o,0x77);
        opl(0xe0+o,0);opl(0xe3+o,0);opl(0xc0+i,2);
    }
}
static void music_event(const MusicEvent *e) {
    unsigned f;int ch=e->ch,oct,pitch=e->pitch;
    opl(0xb1+ch,0);musicplaying[ch]=(U8)pitch;
    if(!pitch)return;
    oct=pitch/12-1;f=fnums[pitch%12];musiclevel[ch]=e->level;
    opl(0x43+musicops[ch],music_level(ch));
    opl(0xa1+ch,f&255);opl(0xb1+ch,(musicvolume?32:0)|(oct<<2)|(f>>8));
}
static void note(int ch,int pitch,int life) {
    unsigned f=fnums[pitch%12],d;
    int oct=pitch/12;
    if(audiofound&&(soundmode&1)&&fxvolume) {
        if(!ch)opl(0x43,fxvolume?(10-fxvolume)*4:63);
        opl(0xb0+ch,0);opl(0xa0+ch,f&255);
        opl(0xb0+ch,32|((2+oct)<<2)|(f>>8));opllife[ch]=(U8)life;
    }
    if(ch==0&&(soundmode&2)) {
        d=divisor[pitch%12]>>oct;outp(0x43,0xb6);
        outp(0x42,d&255);outp(0x42,d>>8);outp(0x61,inp(0x61)|3);spkleft=(U8)life;
    }
}
static void audio_tick(void) {
    static const U8 fan[6]={24,28,31,36,31,36};
    int i,n,pitch;unsigned f;const MusicEvent *e;
    if(musrequest) {n=musrequest-1;musrequest=0;music_start(n);}
    if(volumechange) {
        volumechange=0;
        if(audiofound) {
            opl(0x43,fxvolume?(10-fxvolume)*4:63);
            if(!fxvolume){opl(0xb0,0);opllife[0]=0;}
            for(i=0;i<8;++i)if(musicplaying[i]) {
                opl(0x43+musicops[i],music_level(i));pitch=musicplaying[i];f=fnums[pitch%12];
                opl(0xb1+i,(musicvolume?32:0)|((pitch/12-1)<<2)|(f>>8));
            }
        }
    }
    for(i=0;i<9;++i) if(opllife[i]&&! --opllife[i]) opl(0xb0+i,0);
    if(spkleft&&! --spkleft) speaker_off();
    if(pendingfx) {fxkind=pendingfx;pendingfx=0;fxpos=fxwait=0;}
    if(fxkind) {
        if(fxwait) --fxwait;
        else {
            if(fxkind==1) {note(0,24,2);fxkind=0;}
            else if(fxkind==2) {note(0,fxpos?19:12,3);fxwait=3;if(++fxpos==2)fxkind=0;}
            else {note(0,fan[fxpos],fxpos==5?12:4);fxwait=4;if(++fxpos==6)fxkind=0;}
        }
    }
    if(!music || !(soundmode&1) || !audiofound) return;
    if(muswait&&--muswait)return;
    /* Offline conversion proves <=14 events/tick. Cap at 32 as a runtime guard. */
    for(n=0;n<32;++n) {
        e=tunes[song]+musstep;
        if(e->ch==255) {
            i=(song+1)&3;
            if(shuffle) {shuffle_state=(shuffle_state>>1)^((shuffle_state&1)?0xb400:0);i=(song+1+shuffle_state%3)&3;}
            music_start(i);return;
        }
        if(e->ch<8)music_event(e);
        ++musstep;muswait=tunes[song][musstep].delay;
        if(muswait)return;
    }
}
static void __interrupt __far timer_isr(void) {
    ++ticks;audio_tick();timerhalf^=1;
    if(!timerhalf) _chain_intr(oldtimer);else outp(0x20,0x20);
}
static void audio_install(void) {
    oldspeaker=inp(0x61)&3;
    if(audiofound) {silence();instruments();}
    music_start(song);
    oldtimer=_dos_getvect(8);_dos_setvect(8,timer_isr);
    _disable();outp(0x43,0x34);outp(0x40,0);outp(0x40,128);_enable();
}
static void audio_remove(void) {
    _disable();outp(0x43,0x36);outp(0x40,0);outp(0x40,0);_enable();
    _dos_setvect(8,oldtimer);silence();outp(0x61,(inp(0x61)&0xfc)|oldspeaker);
}
static void audio_change(void) {
    /* At most key-offs; do not reprogram instruments on each toggle. */
    _disable();
    soundmode=(soundmode+1)&3;
    if(!audiofound&&(soundmode&1)) soundmode=2;
    silence();music_start(song);_enable();
}
static void music_change(void) {
    _disable();music=!music;
    if(!music)music_off();else music_start(song);
    _enable();
}
