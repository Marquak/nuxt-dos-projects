/* gcc -std=c99 -O2 -Wall -Wextra -Werror -DHOST -x c MUSIC.C tests/MUSTEST.C
 * Exact IRQ-clock and OPL-stream tests. Optional argument saves timestamped
 * register writes (tick,register,value) for independent synthesis analysis.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../SUNSET.H"
extern void sunset_test_tick(void);
extern void sunset_test_clock(unsigned long value);
extern unsigned long sunset_test_bios(void);
extern unsigned long sunset_test_phase(void);
extern unsigned sunset_test_cycle(void);
extern unsigned music_pulse(void);
int sunset_test_card=1;
static unsigned regs[256],writes,keyons[9],keyoffs[9],drum_events;
static unsigned current_writes,max_writes,failures,checks,all_drums,sweeps;
static unsigned kick_bars[192],snare_bars[192],notes_bars[192],last_pitch[9];
static unsigned lead_hash[192],loop_keyoffs;
static unsigned char changed_pitch_while_on;
static unsigned bass_macros,pad_macros,lead_bends;
static unsigned long raw_tick,total_writes;
static FILE *capture;

void sunset_test_opl(unsigned reg,unsigned value){
 unsigned ch,bar=(music_beat()>>4)+96*sunset_test_cycle();
 if(reg>=256 || value>=256){fprintf(stderr,"Invalid OPL write\n");exit(2);}
 writes++;current_writes++;total_writes++;
 if(capture)fprintf(capture,"%lu,%u,%u\n",raw_tick,reg,value);
 if(reg>=0xa0 && reg<=0xa5){
  ch=reg-0xa0;
  if((regs[0xb0+ch]&0x20) && ch!=2)changed_pitch_while_on=1;
  if(ch==2 && (regs[0xb2]&0x20))lead_bends++;
  last_pitch[ch]=value;
  if(ch==2 && bar<192)lead_hash[bar]=lead_hash[bar]*31+value;
 }
 if(reg>=0xb0 && reg<=0xb8){
  ch=reg-0xb0;
  if((value&0x20) && !(regs[reg]&0x20)){
   keyons[ch]++;if(bar<192)notes_bars[bar]++;
  }
  if(!(value&0x20) && (regs[reg]&0x20))keyoffs[ch]++;
 }
 if(reg==0xbd && (value&31)){
  drum_events++;all_drums|=value&31;
  if(bar<192 && (value&8))snare_bars[bar]++;
  if(bar<192 && (value&16))kick_bars[bar]++;
 }
 if(reg==0xa6 && value!=regs[reg])sweeps++;
 if(reg==0x41 && (regs[0xb1]&0x20) && value!=regs[reg])bass_macros++;
 if(reg==0x4b && (regs[0xb3]&0x20) && value!=regs[reg])pad_macros++;
 regs[reg]=value;
}
static void check(int value,const char *what){
 checks++;if(!value){fprintf(stderr,"FAIL: %s\n",what);failures++;}
}
static void tick(unsigned n){
 while(n--){raw_tick++;current_writes=0;sunset_test_tick();if(current_writes>max_writes)max_writes=current_writes;}
}
static int no_keys(void){
 unsigned i;for(i=0;i<9;i++)if(regs[0xb0+i]&0x20)return 0;
 return !(regs[0xbd]&31);
}
int main(int argc,char **argv){
 unsigned i,before,notes,step;
 unsigned long now,phase,bios,first_loop_tick,second_loop_tick,expected;
 memset(regs,0,sizeof(regs));
 check(music_start(1)==0,"explicit silent skips AdLib");
 check(writes==0,"silent startup performs zero OPL writes");
 tick(1);check(music_pulse()==12,"silent mode still emits actual kick pulse");
 tick(1199);check(music_ticks()==300,"public animation clock remains 18.2 Hz");
 check(sunset_test_bios()==300,"every fourth raw IRQ chains BIOS");
 check(music_beat()==115,"104 BPM has 115 sixteenths by IRQ 1200");
 check(writes==0 && music_notes()==0,"silent score performs zero I/O");
  music_stop();before=writes;tick(10);
 check(music_ticks()==300 && writes==before,"stopped timer does no work");

 sunset_test_card=0;check(!music_start(0),"missing AdLib falls back to silence");
 tick(48);check(music_ticks()==12 && writes==before,"no-card clock still works");
 music_stop();sunset_test_card=1;
 check(music_start(0),"AdLib detected");
 check((regs[0xbd]&0x20)!=0,"hardware rhythm mode enabled");
 check(regs[1]==0x20,"waveform select enabled");
 check(no_keys(),"startup leaves all notes off");
 check(writes<160,"bounded startup register programming");
 tick(1);check(music_ticks()==0 && music_beat()==1,"first beat on first raw IRQ");
 check((regs[0xbd]&16)!=0,"opening hardware kick starts immediately");
 check(music_pulse()==12,"visual pulse starts on actual kick");
 tick(3);check(music_ticks()==1 && music_beat()==1,"animation advances on fourth IRQ");
 check(sweeps>=2,"kick is swept across live pitches");
 check(music_pulse()==9,"visual pulse decays on high-rate clock");
 tick(8);check(music_beat()==2,"second sixteenth arrives at IRQ twelve");

 now=music_ticks();step=music_beat();phase=sunset_test_phase();bios=sunset_test_bios();
 music_pause(1);before=writes;
 check(no_keys(),"pause keys off melodic voices and drums");
 tick(100);check(music_ticks()==now && music_beat()==step,"pause freezes clock and score");
 check(sunset_test_phase()==phase,"pause preserves fractional tempo phase");
 check(sunset_test_bios()==bios+25,"BIOS keeps its standard time while paused");
 check(writes==before,"paused IRQ does zero sound I/O");
 music_pause(0);check(!no_keys(),"resume restores active kick envelope");
 tick(4);check(music_ticks()==now+1,"resume retains animation divider phase");

 music_mute(1);check(no_keys(),"mute clears current keys");
 before=writes;notes=music_notes();now=music_ticks();step=music_beat();
 tick(400);
 check(music_ticks()==now+100 && music_beat()>step+30,"muting preserves musical progress");
 check(writes==before && music_notes()==notes,"muted score performs no OPL writes");
 music_mute(0);
 now=music_ticks();step=music_beat();before=writes;phase=sunset_test_phase();
 for(i=0;i<5;i++)music_section(i);
 music_section(99);
 check(music_ticks()==now && music_beat()==step,"scene selection preserves both clocks");
 check(writes==before && sunset_test_phase()==phase,"scene selection does not interrupt the tune");
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
 while(!sunset_test_cycle() && raw_tick<17000)tick(1);
 first_loop_tick=raw_tick;
 check(sunset_test_cycle()==1 && music_beat()==0,"96-bar score wraps naturally");
 check(first_loop_tick==16125,"104 BPM final step of 96-bar score arrives at IRQ 16125");
 check(!no_keys(),"loop wrap sustains voices rather than resetting OPL");
 phase=sunset_test_phase();step=music_beat();notes=music_notes();
 for(i=0;i<9;i++)loop_keyoffs+=keyoffs[i];
 tick(1);
 check(sunset_test_phase()==phase+6815744UL,"wrap preserves fractional tempo phase");
 check(music_beat()==step && music_notes()==notes,"wrap does not trigger an early opening beat");
 before=0;for(i=0;i<9;i++)before+=keyoffs[i];
 check(before==loop_keyoffs,"wrap has no global key-off or silence operation");
 while(sunset_test_cycle()<2 && raw_tick<34000)tick(1);
 second_loop_tick=raw_tick;
 check(sunset_test_cycle()==2,"second full arrangement also loops");
 expected=1+((raw_tick-1)*6815744UL)/71590920UL;
 check(music_beat()==expected%1536,"two loops retain exact rational tempo");
 check(second_loop_tick-first_loop_tick==16134,
       "each complete loop remains 96 bars at 104 BPM");
 check(music_ticks()==raw_tick/4,"animation time stays monotonic through both loops");
 for(i=0;i<6;i++)check(keyons[i]>50,"all six melodic voices contribute to track");
 for(i=6;i<9;i++)check(keyons[i]==0,"rhythm voices never use melodic key bits");
 for(i=0;i<40;i++)check(kick_bars[i]==4,"first main groove has four kicks per bar");
 for(i=56;i<96;i++)check(kick_bars[i]==4,"return and turnaround retain the kick groove");
 for(i=40;i<48;i++)check(kick_bars[i]==0,"breakdown removes kick for contrast");
 check(kick_bars[55]==2,"build breath leaves only two kicks before return");
 check(snare_bars[54]==10,"build reaches eighth-note snare roll plus backbeat flams");
 check(snare_bars[55]==16,"last build snare roll includes backbeat flams and a pickup breath");
 for(i=16;i<24;i++)check(lead_hash[i]!=lead_hash[i+96],"alternate loop changes each hook bar");
 check(all_drums==31,"all five OPL hardware drum instruments are used");
 check(sweeps>2000,"kick has a sustained low-frequency pitch-sweep body");
 check(!changed_pitch_while_on,"bass/pads/echo key off before changing pitch");
 check(lead_bends>1000,"lead uses smooth live pitch scoops");
 check(bass_macros>1000,"bass has per-note FM brightness envelopes");
 check(pad_macros>1000,"chords duck and recover around each kick");
 check(max_writes<=32,"peak OPL register writes per high-rate IRQ are bounded");
 if(capture){fclose(capture);capture=0;}
 sunset_test_clock(0xfffffffeUL);tick(12);
 check(music_ticks()==1,"public clock has defined 32-bit wrap");
 music_stop();check(no_keys() && regs[0xbd]==0,"shutdown clears keys and rhythm mode");
 before=writes;music_stop();tick(3);check(writes==before,"shutdown is idempotent");
 printf("Music: %u checks, %u failures; peak %u OPL writes/IRQ.\n",checks,failures,max_writes);
 printf("Two loops: %lu raw IRQs (%.4f seconds), %lu OPL writes, %u kick bends.\n",
        second_loop_tick,second_loop_tick*16384.0/1193182.0,total_writes,sweeps);
 printf("Melodic triggers: %u %u %u %u %u %u; hardware rhythm %u.\n",
        keyons[0],keyons[1],keyons[2],keyons[3],keyons[4],keyons[5],drum_events);
 return failures?1:0;
}
