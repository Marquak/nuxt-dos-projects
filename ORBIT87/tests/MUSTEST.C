/* gcc -std=c99 -O2 -Wall -Wextra -Werror -DHOST -x c MUSIC.C tests/MUSTEST.C
 * Exact IRQ-clock and OPL-stream tests. Optional argument saves timestamped
 * register writes (tick,register,value) for independent synthesis analysis.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ORBIT.H"
extern void orbit_test_tick(void);
extern void orbit_test_clock(unsigned long value);
extern unsigned long orbit_test_bios(void);
extern unsigned long orbit_test_phase(void);
extern int music_finished(void);
extern void music_restart(void);
extern unsigned music_pulse(void);
int orbit_test_card=1;
static unsigned regs[256],writes,keyons[9],keyoffs[9],drum_events;
static unsigned current_writes,max_writes,failures,checks,all_drums,sweeps;
static unsigned kick_bars[112],snare_bars[112],notes_bars[112],last_pitch[9];
static unsigned char changed_pitch_while_on;
static unsigned long raw_tick,total_writes;
static FILE *capture;

void orbit_test_opl(unsigned reg,unsigned value){
 unsigned ch,bar=music_beat()>>4;
 if(reg>=256 || value>=256){fprintf(stderr,"Invalid OPL write\n");exit(2);}
 writes++;current_writes++;total_writes++;
 if(capture)fprintf(capture,"%lu,%u,%u\n",raw_tick,reg,value);
 if(reg>=0xa0 && reg<=0xa5){
  ch=reg-0xa0;
  if((regs[0xb0+ch]&0x20) && ch)changed_pitch_while_on=1;
  if(!ch && (regs[0xb0]&0x20))sweeps++;
  last_pitch[ch]=value;
 }
 if(reg>=0xb0 && reg<=0xb8){
  ch=reg-0xb0;
  if((value&0x20) && !(regs[reg]&0x20)){
   keyons[ch]++;if(bar<112){notes_bars[bar]++;if(!ch)kick_bars[bar]++;}
  }
  if(!(value&0x20) && (regs[reg]&0x20))keyoffs[ch]++;
 }
 if(reg==0xbd && (value&31)){
  drum_events++;all_drums|=value&31;
  if(bar<112 && (value&8))snare_bars[bar]++;
 }
 regs[reg]=value;
}
static void check(int value,const char *what){
 checks++;if(!value){fprintf(stderr,"FAIL: %s\n",what);failures++;}
}
static void tick(unsigned n){
 while(n--){raw_tick++;current_writes=0;orbit_test_tick();if(current_writes>max_writes)max_writes=current_writes;}
}
static int no_keys(void){
 unsigned i;for(i=0;i<9;i++)if(regs[0xb0+i]&0x20)return 0;
 return !(regs[0xbd]&31);
}
int main(int argc,char **argv){
 unsigned i,before,notes,step;
 unsigned long now,phase,bios,finished_tick;
 memset(regs,0,sizeof(regs));
 check(music_start(1)==0,"explicit silent skips AdLib");
 check(writes==0,"silent startup performs zero OPL writes");
 tick(1);check(music_pulse()==12,"silent mode still emits actual kick pulse");
 tick(1199);check(music_ticks()==300,"public animation clock remains 18.2 Hz");
 check(orbit_test_bios()==300,"every fourth raw IRQ chains BIOS");
 check(music_beat()==154,"140 BPM has 154 sixteenths by IRQ 1200");
 check(writes==0 && music_notes()==0,"silent score performs zero I/O");
  music_stop();before=writes;tick(10);
 check(music_ticks()==300 && writes==before,"stopped timer does no work");

 orbit_test_card=0;check(!music_start(0),"missing AdLib falls back to silence");
 tick(48);check(music_ticks()==12 && writes==before,"no-card clock still works");
 music_stop();orbit_test_card=1;
 check(music_start(0),"AdLib detected");
 check((regs[0xbd]&0x20)!=0,"hardware rhythm mode enabled");
 check(regs[1]==0x20,"waveform select enabled");
 check(no_keys(),"startup leaves all notes off");
 check(writes<160,"bounded startup register programming");
 tick(1);check(music_ticks()==0 && music_beat()==1,"first beat on first raw IRQ");
 check(keyons[0]==1,"opening kick starts immediately");
 check(music_pulse()==12,"visual pulse starts on actual kick");
 tick(3);check(music_ticks()==1 && music_beat()==1,"animation advances on fourth IRQ");
 check(sweeps>=2,"kick is swept across live pitches");
 check(music_pulse()==9,"visual pulse decays on high-rate clock");
 tick(5);check(music_beat()==2,"second sixteenth arrives at IRQ nine");

 now=music_ticks();step=music_beat();phase=orbit_test_phase();bios=orbit_test_bios();
 music_pause(1);before=writes;
 check(no_keys(),"pause keys off melodic voices and drums");
 tick(100);check(music_ticks()==now && music_beat()==step,"pause freezes clock and score");
 check(orbit_test_phase()==phase,"pause preserves fractional tempo phase");
 check(orbit_test_bios()==bios+25,"BIOS keeps its standard time while paused");
 check(writes==before,"paused IRQ does zero sound I/O");
 music_pause(0);check(!no_keys(),"resume restores active kick envelope");
 tick(4);check(music_ticks()==now+1,"resume retains animation divider phase");

 music_mute(1);check(no_keys(),"mute clears current keys");
 before=writes;notes=music_notes();now=music_ticks();step=music_beat();
 tick(400);
 check(music_ticks()==now+100 && music_beat()>step+50,"muting preserves musical progress");
 check(writes==before && music_notes()==notes,"muted score performs no OPL writes");
 music_mute(0);
 now=music_ticks();step=music_beat();before=writes;phase=orbit_test_phase();
 for(i=0;i<5;i++)music_section(i);
 music_section(99);
 check(music_ticks()==now && music_beat()==step,"scene selection preserves both clocks");
 check(writes==before && orbit_test_phase()==phase,"scene selection does not interrupt the tune");
 music_pause(1);music_mute(1);before=writes;music_pause(0);
 check(writes==before && no_keys(),"unpause while muted remains silent");
 music_mute(0);music_pause(1);before=writes;music_mute(1);music_mute(0);
 check(writes==before && no_keys(),"unmute while paused remains silent");
 music_pause(0);music_stop();

 memset(keyons,0,sizeof(keyons));memset(keyoffs,0,sizeof(keyoffs));
 memset(kick_bars,0,sizeof(kick_bars));memset(snare_bars,0,sizeof(snare_bars));
 memset(notes_bars,0,sizeof(notes_bars));
 raw_tick=0;total_writes=0;max_writes=0;all_drums=0;sweeps=0;
 if(argc>1){capture=fopen(argv[1],"wt");if(!capture)return 2;}
 music_start(0);
 while(!music_finished() && raw_tick<14500)tick(1);
 finished_tick=raw_tick;
 check(music_finished() && music_beat()==1792,"complete track is exactly 112 bars");
 check(finished_tick>=13982 && finished_tick<=13985,"track duration is 192 seconds at 140 BPM");
 check(no_keys(),"track end releases all voices and drums");
 before=writes;now=music_ticks();tick(80);
 check(music_beat()==1792 && writes==before,"completed score holds without restarting");
 check(music_ticks()==now+20,"animation clock continues after score ends");
 for(i=0;i<6;i++)check(keyons[i]>50,"all six melodic voices contribute to track");
 for(i=6;i<9;i++)check(keyons[i]==0,"rhythm voices never use melodic key bits");
 for(i=16;i<47;i++)check(kick_bars[i]==4,"first main groove has four kicks per bar");
 for(i=64;i<103;i++)check(kick_bars[i]==4,"drop and peak have four kicks per bar");
 for(i=48;i<56;i++)check(kick_bars[i]==0,"breakdown removes kick for contrast");
 check(kick_bars[63]==0,"pre-drop gap clears the kick");
 check(snare_bars[62]==16,"build reaches sixteen snare triggers per bar");
 check(snare_bars[63]==12,"last build bar has a quarter-bar gap before drop");
 check(notes_bars[64]>notes_bars[0]*4,"drop has much denser arrangement than intro");
 check(all_drums==31,"all five OPL hardware drum instruments are used");
 check(sweeps>1000,"low-frequency kick body comes from sustained pitch sweeps");
 check(!changed_pitch_while_on,"ordinary notes key off before changing pitch");
 check(max_writes<=44,"peak OPL register writes per high-rate IRQ are bounded");
 if(capture){fclose(capture);capture=0;}
 now=music_ticks();music_restart();
 check(!music_finished() && music_beat()==0 && music_ticks()==now,"explicit loop restart preserves animation time");
 tick(1);check(music_beat()==1,"restart starts next IRQ without a stale delay");
 orbit_test_clock(0xfffffffeUL);tick(12);
 check(music_ticks()==1,"public clock has defined 32-bit wrap");
 music_stop();check(no_keys() && regs[0xbd]==0,"shutdown clears keys and rhythm mode");
 before=writes;music_stop();tick(3);check(writes==before,"shutdown is idempotent");
 printf("Music: %u checks, %u failures; peak %u OPL writes/IRQ.\n",checks,failures,max_writes);
 printf("Full track: %lu raw IRQs (%.4f seconds), %lu OPL writes, %u kick bends.\n",
        finished_tick,finished_tick*16384.0/1193182.0,total_writes,sweeps);
 printf("Melodic triggers: %u %u %u %u %u %u; hardware rhythm %u.\n",
        keyons[0],keyons[1],keyons[2],keyons[3],keyons[4],keyons[5],drum_events);
 return failures?1:0;
}
