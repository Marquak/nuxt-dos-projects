/* ORBIT 87 v2: PULSE DRIVE, an original 140 BPM, 112-bar FM techno track.
 * Six melodic voices and OPL2 rhythm mode: swept kick, acid bass, lead,
 * three-note stabs, snare, hats, toms and crashes. The arrangement is one
 * continuous performance; selecting a visual scene never restarts it.
 *
 * IRQ0 runs at PIT / 16384 (72.8 Hz). Every fourth interrupt chains the
 * original INT 08h, preserving the normal BIOS clock and INT 1Ch users.
 * The public animation clock is still PIT / 65536 (18.2 Hz). The handler
 * performs no DOS/BIOS service calls, allocation, division or floating point.
 */
#include "ORBIT.H"
#ifdef HOST
#include <stdint.h>
typedef uint32_t Clock32;
extern void orbit_test_opl(unsigned reg,unsigned value);
extern int orbit_test_card;
#else
#include <dos.h>
#include <conio.h>
typedef unsigned long Clock32;
#endif

#define REST 255
#define BD 0x10
#define SD 0x08
#define TOM 0x04
#define CYM 0x02
#define HH 0x01
#define SCORE_STEPS 1792U
#define OUTPUT_HEADROOM 6U /* 4.5 dB: kick + snare + bass stay below clipping. */
/* Exact rational tempo for a 1,193,182 Hz PIT: no per-tick division. */
#define STEP_ADD 9175040UL       /* 140 BPM * 4 steps/beat * 16384 */
#define STEP_CLOCK 71590920UL    /* 60 seconds * 1193182 */

static volatile Clock32 clock_count;
static volatile unsigned sequencer_step,note_count;
static volatile int paused,muted,available;
static int hooked;
static unsigned char bios_divider,clock_divider;
static volatile unsigned char finished,kick_pulse;
static Clock32 step_phase;
static unsigned char gates[6],pitches[6],levels[6],keys[6];
static unsigned char rhythm,kick_age,kick_active,reg_cache[256];
static unsigned char bass_filter,hat_level,fade_level;
static unsigned short pitch_word[96];
static const unsigned short fnum[12]={345,365,387,410,435,460,488,517,548,580,615,651};
static const unsigned char op_slot[9]={0,1,2,8,9,10,16,17,18};
#ifdef HOST
static unsigned long bios_chains;
#endif

/* m20 c20 m40 c40 m60 c60 m80 c80 mE0 cE0 feedback/connection.
 * Kick is mostly sine, bass gets modulator-level sweeps, lead and short
 * three-voice stabs are bright. No samples, copied tunes or assets used.
 */
static const unsigned char patches[6][11]={
 {0x01,0x01,0x31,0x00,0xf9,0xf6,0xf8,0x98,0,0,0x00},
 {0x21,0x21,0x24,0x06,0xf7,0xf6,0x58,0x58,0,0,0x0c},
 {0x21,0x21,0x16,0x0e,0xf7,0xf6,0x78,0x68,1,0,0x08},
 {0x21,0x21,0x1b,0x14,0xf8,0xf7,0x89,0x89,1,0,0x08},
 {0x21,0x21,0x1e,0x17,0xf8,0xf7,0x89,0x89,1,0,0x08},
 {0x21,0x21,0x22,0x19,0xf8,0xf7,0x89,0x89,1,0,0x08}
};
/* Eight-bar harmonic cycle: E minor, C, D, E minor, G, C, D. Roots are
 * MIDI pitches in the bass register; D resolves down to the next E.
 */
static const unsigned char roots[8]={40,40,36,38,40,43,36,38};
static const unsigned char thirds[8]={3,3,4,4,3,4,4,4};
static const unsigned char bass_a[16]={REST,0,12,0,REST,7,0,12,REST,0,12,7,0,12,7,0};
static const unsigned char bass_b[16]={REST,12,0,7,REST,0,12,0,REST,7,12,0,REST,12,7,12};
/* Original syncopated motif. Values are offsets from each chord root.
 * REST matters: the spaces leave the kick and snare enough room to hit.
 */
static const unsigned char hook[8][8]={
 {0,REST,12,7,REST,10,7,3}, {0,REST,7,REST,12,10,7,REST},
 {7,REST,12,7,REST,4,7,12},{7,REST,9,7,4,REST,2,REST},
 {12,REST,7,3,REST,0,7,10},{12,REST,7,4,REST,7,14,12},
 {7,REST,4,0,REST,7,12,7}, {9,7,REST,4,2,REST,7,REST}
};
/* Second-half answer: longer ascending gestures, then a clipped response.
 * It arrives at bar 80 so the long drop develops instead of looping.
 */
static const unsigned char hook_b[8][8]={
 {12,REST,15,14,12,REST,7,10},{7,10,12,REST,15,14,12,REST},
 {12,REST,16,14,12,7,REST,4},{14,REST,12,9,7,REST,9,12},
 {15,14,12,REST,7,10,12,REST},{14,REST,12,7,4,REST,7,12},
 {16,14,12,REST,7,4,REST,7},{14,12,9,7,REST,9,7,REST}
};
static const unsigned char arp_order[8]={0,7,12,3,7,15,12,7};
static const unsigned char kick_sweep[5]={55,43,36,32,31};

#ifndef HOST
static void (__interrupt __far *old_tick)(void);
static unsigned lock_irq(void);
#pragma aux lock_irq = "pushf" "pop ax" "cli" value [ax];
static void unlock_irq(unsigned flags);
#pragma aux unlock_irq = "push ax" "popf" parm [ax];
/* ISA-safe YM3812 timing, 8086 instructions only. */
static void opl_raw(unsigned reg,unsigned value);
#pragma aux opl_raw = \
 "mov dx,388h" "out dx,al" "mov cx,6" \
 "addr_wait: in al,dx" "loop addr_wait" \
 "inc dx" "mov al,bl" "out dx,al" "dec dx" "mov cx,35" \
 "data_wait: in al,dx" "loop data_wait" \
 parm [ax] [bx] modify exact [ax cx dx];
static void timer_fast(void);
#pragma aux timer_fast = "mov al,36h" "out 43h,al" \
 "xor al,al" "out 40h,al" "mov al,40h" "out 40h,al" modify exact [ax];
static void timer_normal(void);
#pragma aux timer_normal = "mov al,36h" "out 43h,al" \
 "xor al,al" "out 40h,al" "out 40h,al" modify exact [ax];
#else
static unsigned lock_irq(void){return 0;}
static void unlock_irq(unsigned flags){(void)flags;}
static void opl_raw(unsigned reg,unsigned value){orbit_test_opl(reg,value);}
#endif

static void opl(unsigned reg,unsigned value){
 if(reg_cache[reg]!=(unsigned char)value){
  reg_cache[reg]=(unsigned char)value;opl_raw(reg,value);
 }
}
static int detect(void){
#ifndef HOST
 unsigned a,b,i;
 opl_raw(4,0x60);opl_raw(4,0x80);a=inp(0x388)&0xe0;
 opl_raw(2,0xff);opl_raw(4,0x21);
 for(i=0;i<200;i++)inp(0x388);
 b=inp(0x388)&0xe0;opl_raw(4,0x60);opl_raw(4,0x80);
 return a==0 && b==0xc0;
#else
 return orbit_test_card;
#endif
}
static int audible(void){return available && !muted && !paused;}
static void keys_off(void){
 unsigned i;
 if(!available)return;
 for(i=0;i<9;i++)opl(0xb0+i,reg_cache[0xb0+i]&0x1f);
 opl(0xbd,0x20);
}
static unsigned level(unsigned value){
 value+=fade_level+OUTPUT_HEADROOM;return value>63?63:value;
}
static void voice_on(unsigned ch,unsigned pitch,unsigned volume){
 unsigned word;
 if(pitch>95)pitch=95;
 pitches[ch]=(unsigned char)pitch;levels[ch]=(unsigned char)volume;keys[ch]=1;
 if(!audible())return;
 word=pitch_word[pitch];
 opl(0xb0+ch,reg_cache[0xb0+ch]&0x1f);
 opl(0x43+op_slot[ch],level(volume));
 opl(0xa0+ch,word&255);opl(0xb0+ch,0x20|(word>>8));note_count++;
}
static void voice_off(unsigned ch){
 keys[ch]=0;gates[ch]=0;
 if(audible())opl(0xb0+ch,reg_cache[0xb0+ch]&0x1f);
}
static void voice_gate(unsigned ch,unsigned pitch,unsigned volume,unsigned duration){
 voice_on(ch,pitch,volume);gates[ch]=(unsigned char)duration;
}
static void restore_voices(void){
 unsigned i;
 if(!audible())return;
 /* Mute can span filter changes: restore the logical timbre as well as
  * the notes, rather than leaking the pre-mute filter into the new bar. */
 opl(0x41,bass_filter);opl(0x51,level(hat_level));
 for(i=0;i<6;i++)if(keys[i])voice_on(i,pitches[i],levels[i]);
}
static void drums(unsigned bits,unsigned snare_level){
 rhythm=(unsigned char)bits;
 if(audible()){
  opl(0x54,level(snare_level));
  opl(0x51,level(hat_level));
  opl(0x55,level(27));opl(0x52,level(17));opl(0x53,level(25));
  opl(0xbd,0x20);opl(0xbd,0x20|bits);
  if(bits)note_count++;
 }
}
static void kick(void){
 kick_age=0;kick_active=1;kick_pulse=12;
 voice_gate(0,kick_sweep[0],0,12);
}
static void stab(unsigned root,unsigned third,unsigned strength,unsigned duration){
 voice_gate(3,root+24,strength,duration);
 voice_gate(4,root+24+third,strength+3,duration);
 voice_gate(5,root+31,strength+5,duration);
}
static void play_step(void){
 unsigned s=sequencer_step,bar=s>>4,pos=s&15,cycle=bar&7;
 unsigned root=roots[cycle],third=thirds[cycle],offset,bits=0;
 unsigned filter=24,full,build,breakdown,peak,bass,lead,stabs,hit;
 /* The final release happens at the exact end, not one step early. */
 if(s==SCORE_STEPS){
  unsigned i;
  keys_off();for(i=0;i<6;i++)keys[i]=gates[i]=0;
  rhythm=kick_active=kick_pulse=0;finished=1;return;
 }
 full=(bar>=16 && bar<48) || (bar>=64 && bar<104);
 breakdown=bar>=48 && bar<56;build=bar>=56 && bar<64;
 peak=bar>=96 && bar<104;
 bass=(bar>=8 && bar<48) || (bar>=60 && bar<108);
 lead=(bar>=24 && bar<48) || (bar>=64 && bar<104);
 stabs=(bar>=16 && bar<48) || (bar>=64 && bar<108);
 fade_level=bar>=108?(unsigned char)((bar-108)*9+(pos>>1)):0;
 /* Acid timbre opens across each eight-bar phrase, closes for breakdown,
  * then climbs through the build. Modulator attenuation is the FM filter. */
 if(bar<16)filter=42-((bar&7)<<1);
 else if(build)filter=40-((bar-56)<<2);
 else if(full)filter=29-((bar&7)<<1);
 if(peak)filter=13+(pos&3);
 if(pos==3 || pos==10 || pos==14){if(filter>8)filter-=6;}
 bass_filter=(unsigned char)filter;
 if(audible())opl(0x41,filter);
 /* Four-on-the-floor. Eight bars of drums establish the groove before
  * the rolling bass enters; the last bar before each drop makes a gap. */
 hit=!(pos&3) && !breakdown && (!build || bar<60);
 if(bar==63 || (bar==47 && pos>=12) || (bar==103 && pos>=12))hit=0;
 if(bar>=110)hit=!(pos&7);
 if(hit){kick();bits|=BD;}
 if(bass){
  offset=(bar&8)?bass_b[pos]:bass_a[pos];
  if(offset!=REST && !(bar==63 && pos>=8))
   voice_gate(1,root+offset,(pos==3 || pos==10 || pos==14)?4:8,
              (pos==3 || pos==11)?5:4);
 }
 /* An audible quarter-note pump: bass carrier level recovers after each
  * kick while its new notes remain firmly articulated. */
 if(audible() && keys[1] && !(pos&3))opl(0x44,level(15));
 if(stabs && (pos==2 || pos==6 || pos==11 || (pos==14 && (bar&1))))
  stab(root,third,peak?16:20,(pos==11)?5:3);
 if(lead && !(pos&1)){
  offset=(bar>=80?hook_b[cycle][pos>>1]:hook[cycle][pos>>1]);
  if(offset!=REST)voice_gate(2,root+24+offset,peak?10:14,5);
 }
 /* A rising arpeggiator carries the intro and breakdown. It returns an
  * octave higher at the peak, occupying gaps in the hook rather than
  * replacing the underlying eight-bar theme. */
 if((bar>=4 && bar<24) || breakdown || build || (peak && (pos&1))){
  if(!(pos&1) || bar>=62 || peak){
   offset=arp_order[(pos+(bar&3))&7];
   if(third==4 && (offset==3 || offset==15))offset++;
   voice_gate(2,root+(breakdown?24:36)+offset,breakdown?19:20,3);
  }
 }
 if(breakdown && !pos)stab(root,third,28,23);
 if(build && (pos==2 || pos==10))stab(root,third,27-(bar-56),4);
 /* Snare on two and four, accented offbeat hats, sixteenth ghost hats,
  * short tom turnarounds and a genuine rising snare-roll before the drop. */
 hat_level=(unsigned char)((pos&3)==2?12:27);
 if(!breakdown && !(bar==63 && pos>=12)){
  if((pos&3)==2 || (full && (pos&1)) || (bar>=60 && !(pos&1)))bits|=HH;
  if((pos==4 || pos==12) && bar>=4)bits|=SD;
 }
 if(build){
  if(bar<60){if(!(pos&3))bits|=SD;}
  else if(bar<62){if(!(pos&1))bits|=SD;}
  else if(bar==62 || pos<12)bits|=SD;
 }
 if(full && (bar&7)==7 && pos>=13)bits|=(pos&1)?TOM:SD;
 if(!pos && (bar==0 || bar==16 || bar==32 || bar==48 || bar==64 ||
             bar==80 || bar==96 || bar==104))bits|=CYM;
 if(bits)drums(bits,build?(unsigned)(24-(bar-56)*2):8);
 sequencer_step++;
}

/* Returns nonzero on the interrupts that belong to the original BIOS. */
static int sequencer_tick(void){
 unsigned i,word;
 int chain;
 bios_divider=(unsigned char)((bios_divider+1)&3);chain=!bios_divider;
#ifdef HOST
 if(chain)bios_chains++;
#endif
 if(paused)return chain;
 clock_divider=(unsigned char)((clock_divider+1)&3);
 if(!clock_divider)clock_count++;
 if(finished)return chain;
 if(kick_pulse)kick_pulse--;
 if(rhythm){rhythm=0;if(audible())opl(0xbd,0x20);}
 for(i=0;i<6;i++)if(gates[i] && !--gates[i])voice_off(i);
 /* 55 -> 43 -> 36 -> 32 -> 31 at 13.7 ms intervals gives a pitched
  * attack followed by a sub-bass body. This deliberately bends a live
  * key; every ordinary melodic re-trigger still keys off first. */
 if(kick_active){
  if(kick_age<4){
   kick_age++;pitches[0]=kick_sweep[kick_age];
   if(audible()){
    word=pitch_word[pitches[0]];
    opl(0xa0,word&255);opl(0xb0,0x20|(word>>8));
   }
  }else kick_active=0;
 }
 step_phase+=STEP_ADD;
 if(step_phase>=STEP_CLOCK){step_phase-=STEP_CLOCK;play_step();}
 return chain;
}
#ifndef HOST
static void __interrupt __far timer_handler(void){
 if(sequencer_tick())_chain_intr(old_tick);
 else outp(0x20,0x20); /* Exactly one EOI: the chained BIOS supplies its own. */
}
#endif

static void init_card(void){
 unsigned i,j,slot,word;
 static const unsigned char regs[11]={0x20,0x23,0x40,0x43,0x60,0x63,0x80,0x83,0xe0,0xe3,0xc0};
 for(i=0;i<256;i++)reg_cache[i]=0xff;
 for(i=0;i<9;i++)opl(0xb0+i,0);
 opl(0xbd,0);opl(1,0x20);opl(8,0);
 for(i=0;i<6;i++)for(j=0;j<11;j++)
  opl(regs[j]+(j==10?i:op_slot[i]),patches[i][j]);
 /* A quiet, short noisy kick layer gives the melodic sine kick a click. */
 slot=op_slot[6];
 opl(0x20+slot,1);opl(0x23+slot,1);
 opl(0x40+slot,9);opl(0x43+slot,25);
 opl(0x60+slot,0xfa);opl(0x63+slot,0xfa);
 opl(0x80+slot,0xfa);opl(0x83+slot,0xfa);
 opl(0xe0+slot,0);opl(0xe3+slot,0);opl(0xc6,0x0e);
 for(i=7;i<9;i++){
  slot=op_slot[i];
  opl(0x20+slot,1);opl(0x23+slot,1);
  opl(0x40+slot,i==7?27:17);opl(0x43+slot,i==7?8:27);
  opl(0x60+slot,i==7?0xfd:0xf8);opl(0x63+slot,i==7?0xf8:0xf5);
  opl(0x80+slot,i==7?0xdd:0xa8);opl(0x83+slot,i==7?0xa8:0x75);
  opl(0xe0+slot,0);opl(0xe3+slot,0);opl(0xc0+i,0);
 }
 for(i=6;i<9;i++){
  word=pitch_word[i==6?43:i==7?55:48];
  opl(0xa0+i,word&255);opl(0xb0+i,word>>8);
 }
 opl(0xbd,0x20);
}

int music_start(int silent){
 unsigned i,octave=0,semitone=0;
#ifndef HOST
 unsigned flags;
#endif
 music_stop();
 clock_count=0;sequencer_step=note_count=0;paused=muted=0;
 bios_divider=clock_divider=finished=0;rhythm=kick_age=kick_active=kick_pulse=0;
 step_phase=STEP_CLOCK-STEP_ADD;bass_filter=36;hat_level=27;fade_level=0;
#ifdef HOST
 bios_chains=0;
#endif
 for(i=0;i<6;i++){keys[i]=gates[i]=0;pitches[i]=48;levels[i]=20;}
 for(i=0;i<96;i++){
  if(i<12)pitch_word[i]=fnum[0];
  else{
   pitch_word[i]=(unsigned short)(fnum[semitone]|(octave<<10));
   if(++semitone==12){semitone=0;octave++;}
  }
 }
 available=(!silent && detect());
 if(available)init_card();
#ifndef HOST
 old_tick=_dos_getvect(8);
 flags=lock_irq();_dos_setvect(8,timer_handler);timer_fast();hooked=1;unlock_irq(flags);
#else
 hooked=1;
#endif
 return available;
}
void music_stop(void){
#ifndef HOST
 unsigned flags;
 if(hooked){
  flags=lock_irq();timer_normal();_dos_setvect(8,old_tick);hooked=0;unlock_irq(flags);
 }
#else
 hooked=0;
#endif
 if(available){keys_off();opl(0xbd,0);}
 available=0;
}
unsigned long music_ticks(void){
 unsigned flags=lock_irq();Clock32 value=clock_count;unlock_irq(flags);
 return (unsigned long)value;
}
void music_pause(int wanted){
 unsigned flags=lock_irq();
 wanted=wanted!=0;
 if(paused!=wanted){paused=wanted;if(paused)keys_off();else restore_voices();}
 unlock_irq(flags);
}
void music_mute(int wanted){
 unsigned flags=lock_irq();
 wanted=wanted!=0;
 if(muted!=wanted){muted=wanted;if(muted)keys_off();else restore_voices();}
 unlock_irq(flags);
}
void music_section(unsigned wanted){
 /* Scenes are visual chapters of one track, including manual N/1..5. */
 (void)wanted;
}
void music_restart(void){
 unsigned i,flags=lock_irq();
 keys_off();for(i=0;i<6;i++)keys[i]=gates[i]=0;
 sequencer_step=0;finished=0;rhythm=kick_age=kick_active=kick_pulse=0;
 step_phase=STEP_CLOCK-STEP_ADD;fade_level=0;bass_filter=36;hat_level=27;
 unlock_irq(flags);
}
unsigned music_pulse(void){return kick_pulse;}
unsigned music_beat(void){return sequencer_step;}
unsigned music_notes(void){return note_count;}
int music_present(void){return available;}
int music_finished(void){return finished;}

#ifdef HOST
void orbit_test_tick(void){if(hooked)sequencer_tick();}
void orbit_test_clock(unsigned long value){clock_count=(Clock32)value;}
unsigned long orbit_test_bios(void){return bios_chains;}
unsigned long orbit_test_phase(void){return step_phase;}
#endif
