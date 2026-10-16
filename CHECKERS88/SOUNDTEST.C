/* Host test of the actual IRQ music player, with an OPL register capture.
 * gcc -x c -std=c99 -O2 SOUNDTEST.C -o soundtest && ./soundtest [writes.csv]
 * No emulator or time delay: the complete four-piece playlist is checked. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char U8;
#define __interrupt
#define __far
#define _disable() ((void)0)
#define _enable() ((void)0)
#define _dos_getvect(n) NULL
#define _dos_setvect(n,f) ((void)0)
#define _chain_intr(f) ((void)0)
static unsigned regs[256],latch,keyons[9],writes,peak,checks,failures;
static unsigned long clock_tick;
static FILE *logfile;
static unsigned inp(unsigned p){(void)p;return 0;}
static void outp(unsigned p,unsigned v){
    unsigned ch;
    if(p==0x388){latch=v;return;}
    if(p!=0x389)return;
    if(v>255||latch>255){fprintf(stderr,"Invalid OPL write\n");exit(2);}
    if(latch>=0xb0&&latch<=0xb8){ch=latch-0xb0;if((v&32)&&!(regs[latch]&32))++keyons[ch];}
    regs[latch]=v;++writes;
    if(logfile)fprintf(logfile,"%lu,%u,%u\n",clock_tick,latch,v);
}
#include "AUDIO.H"
static void check(int ok,const char *s){++checks;if(!ok){++failures;printf("FAIL: %s\n",s);}}
static void tick(void){writes=0;audio_tick();if(writes>peak)peak=writes;++clock_tick;}
int main(int argc,char **argv){
    unsigned i,j,total,expected[4]={247,905,1142,238},guard;
    audiofound=1;soundmode=1;music=1;
    if(argc>1){logfile=fopen(argv[1],"w");if(!logfile)return 2;}
    instruments();
    for(i=0;i<4;++i){
        music_start(i);memset(keyons,0,sizeof(keyons));guard=0;
        do{tick();}while(song==i&&++guard<20000);
        total=0;for(j=1;j<9;++j)total+=keyons[j];
        check(guard<20000,"song completes and advances");
        check(total==expected[i],"all MIDI notes played exactly once");
        check(keyons[0]==0,"music never uses effect channel");
        for(j=1;j<9;++j)check(!(regs[0xb0+j]&32),"song ends without stuck notes");
    }
    if(logfile){fclose(logfile);logfile=NULL;}
    music_start(0);for(i=0;i<100;++i)tick();
    for(i=0;i<12;++i)volume_step(0,-1);
    tick();
    check(musicvolume==0,"music volume clamps at zero");
    for(i=0;i<200;++i)tick();
    for(j=1;j<9;++j)check(!(regs[0xb0+j]&32),"zero music volume really keys off");
    pendingfx=1;tick();check(regs[0xb0]&32,"music mute leaves effects audible");
    for(i=0;i<12;++i)volume_step(1,-1);
    tick();pendingfx=4;
    for(i=0;i<40;++i)tick();
    check(fxvolume==0&&!(regs[0xb0]&32),"zero effects volume keys off and prevents new notes");
    for(i=0;i<12;++i)volume_step(0,1);
    tick();
    check(musicvolume==10,"music volume clamps at ten");
    music_change();for(i=0;i<100;++i)tick();
    for(j=1;j<9;++j)check(!(regs[0xb0+j]&32),"music off has no hanging voices");
    music_seek(-1);tick();check(song==3,"previous wraps while music off");
    music_seek(1);tick();check(song==0,"next wraps while music off");
    music_seek(1);music_seek(1);tick();check(song==2,"queued next requests accumulate before IRQ");
    music_seek(-1);music_seek(1);tick();check(song==2,"opposite queued requests cancel");
    music_seek(-1);music_seek(-1);music_seek(-1);tick();check(song==3,"queued previous requests wrap");
    music=1;shuffle=1;
    for(i=0;i<4;++i){music_start(i);while(tunes[i][musstep].ch!=255)++musstep;muswait=0;tick();check(song!=i,"shuffle avoids immediate repeat");}
    silence();for(j=0;j<9;++j)check(!(regs[0xb0+j]&32),"shutdown releases every voice");
    check(peak<=40,"bounded OPL writes per tick");
    printf("Audio: %u checks, %u failures; peak %u OPL writes/tick.\n",checks,failures,peak);
    return failures?1:0;
}
