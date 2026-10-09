/* SUNSET: HORIZON RUNNER, original 104 BPM synthwave.
 * 96 bars / 221.54 seconds, continuously looping with alternate lead answers.
 * Three-note pads, FM bass envelopes, lead scoops, timed echo and ducking.
 * Hardware bass drum frees all six melodic channels for pitched instruments.
 * IRQ0 runs at PIT / 16384 (72.8 Hz). Every fourth interrupt chains the
 * original INT 08h, preserving the normal BIOS clock and INT 1Ch users.
 * Public animation time is PIT / 65536 (18.2 Hz). No DOS calls, allocation,
 * division or floating point occurs in the ISR. ISA-safe OPL write delays.
 */
#include "SUNSET.H"
#ifdef HOST
#include <stdint.h>
typedef uint32_t Clock32;
extern void sunset_test_opl(unsigned reg,unsigned value);
extern int sunset_test_card;
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
#define SCORE_STEPS 1536U
#define OUTPUT_HEADROOM 7U /* Carrier-only 5.25 dB summed-output headroom. */
/* Exact rational tempo for a 1,193,182 Hz PIT: no per-tick division. */
#define STEP_ADD 6815744UL       /* 104 BPM * 4 steps/beat * 16384 */
#define STEP_CLOCK 71590920UL    /* 60 seconds * 1193182 */

static volatile Clock32 clock_count;
static volatile unsigned sequencer_step,note_count;
static volatile int paused,muted,available;
static int hooked;
static unsigned char bios_divider,clock_divider;
static volatile unsigned char kick_pulse;
static unsigned char score_cycle;
static Clock32 step_phase;
static unsigned short gates[6];
static unsigned char pitches[6],levels[6],keys[6];
static unsigned char rhythm,kick_age,kick_active,reg_cache[256];
static unsigned char bass_filter,hat_level,duck_age,duck_level;
static unsigned char bass_age,bass_shade,lead_age,clap_delay;
static unsigned char echo_note[16];
static unsigned char macro_divider;
static unsigned short pitch_word[96];
static const unsigned short fnum[12]={345,365,387,410,435,460,488,517,548,580,615,651};
static const unsigned char op_slot[9]={0,1,2,8,9,10,16,17,18};
#ifdef HOST
static unsigned long bios_chains;
#endif

/* OPL2 only. Operator macros change brightness rather than imitating a
 * sample mixer. Patch layout: 20/23,40/43,60/63,80/83,E0/E3,C0.
 * Ch0 echo/arp, ch1 bass, ch2 lead, ch3..5 sustained chord voices.
 * The melodic carrier levels are always set through level() at key-on. */
static const unsigned char patches[6][11]={
 {0x21,0x21,0x28,0x1b,0xf5,0xf5,0x54,0x65,0,0,0x08},
 {0x21,0x21,0x19,0x04,0xf6,0xf5,0x54,0x57,0,0,0x0a},
 {0x21,0x61,0x23,0x0c,0xd3,0xe3,0x24,0x25,0,1,0x08},
 {0x21,0x61,0x2c,0x17,0x83,0x93,0x24,0x35,1,0,0x06},
 {0x21,0x61,0x2d,0x19,0x83,0x93,0x24,0x35,1,0,0x08},
 {0x21,0x61,0x2b,0x1a,0x83,0x93,0x24,0x35,0,0,0x06}
};
/* Original B-minor melody and voicings, not a transcription. */
static const unsigned char roots[8]={35,35,31,31,38,38,33,30};
static const unsigned char chords[8][3]={
 {59,62,66},{59,62,66},{59,62,67},{59,62,67},
 {57,62,66},{57,62,66},{57,61,64},{58,61,66}
};
static const unsigned char bass_a[16]={0,REST,12,0,REST,0,12,REST,0,REST,7,12,REST,0,12,7};
static const unsigned char bass_b[16]={0,REST,12,0,REST,7,12,REST,0,12,7,REST,0,REST,12,10};
static const unsigned char hook[8][16]={
 {71,REST,REST,74,REST,REST,78,REST,76,REST,REST,74,REST,REST,71,REST},
 {REST,REST,69,REST,71,REST,REST,REST,66,REST,REST,69,REST,71,REST,REST},
 {71,REST,REST,74,REST,REST,79,REST,78,REST,REST,74,REST,REST,71,REST},
 {69,REST,REST,71,REST,REST,74,REST,71,REST,REST,REST,67,REST,REST,REST},
 {74,REST,REST,78,REST,REST,81,REST,78,REST,REST,76,REST,REST,74,REST},
 {REST,REST,73,REST,74,REST,REST,REST,69,REST,REST,71,REST,74,REST,REST},
 {73,REST,REST,76,REST,REST,81,REST,78,REST,REST,76,REST,REST,73,REST},
 {73,REST,REST,70,REST,REST,66,REST,70,REST,REST,73,REST,REST,70,REST}
};
static const unsigned char hook_b[8][16]={
 {78,REST,REST,76,REST,74,REST,REST,71,REST,REST,74,REST,78,REST,REST},
 {81,REST,REST,78,REST,REST,76,REST,74,REST,REST,71,REST,REST,69,REST},
 {79,REST,REST,78,REST,74,REST,REST,71,REST,REST,74,REST,79,REST,REST},
 {78,REST,REST,74,REST,REST,71,REST,69,REST,REST,67,REST,REST,71,REST},
 {81,REST,REST,78,REST,76,REST,REST,74,REST,REST,78,REST,81,REST,REST},
 {83,REST,REST,81,REST,REST,78,REST,76,REST,REST,74,REST,REST,69,REST},
 {81,REST,REST,78,REST,76,REST,REST,73,REST,REST,76,REST,81,REST,REST},
 {78,REST,REST,76,REST,REST,73,REST,70,REST,REST,66,REST,REST,70,REST}
};
static const unsigned char arp_order[8]={0,2,1,2,0,1,2,1};
static const unsigned char kick_sweep[6]={53,45,38,32,29,28};
static const unsigned char duck_curve[7]={12,9,6,3,1,0,0};
static const unsigned char bass_curve[7]={22,25,29,33,36,38,39};
static const signed char lead_scoop[5]={-12,-7,-3,0,0};

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
static void opl_raw(unsigned reg,unsigned value){sunset_test_opl(reg,value);}
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
 return sunset_test_card;
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
 value+=OUTPUT_HEADROOM;return value>63?63:value;
}
static unsigned carrier_level(unsigned ch){
 unsigned amount=levels[ch];
 if(ch>=3)amount+=duck_level;
 else if(ch==0)amount+=duck_level>>1;
 return level(amount);
}
static unsigned voice_pitch(unsigned ch){
 unsigned word=pitch_word[pitches[ch]];
 /* All fnums are safely between 345 and 651: these small offsets never
  * cross an octave field. Slight detune plus OPL vibrato softens the pads. */
 if(ch==3)word-=2;
 if(ch==5)word+=2;
 if(ch==2 && lead_age<5)word=(unsigned)((int)word+lead_scoop[lead_age]);
 return word;
}
static void voice_on(unsigned ch,unsigned pitch,unsigned volume){
 unsigned word;
 if(pitch>95)pitch=95;
 pitches[ch]=(unsigned char)pitch;levels[ch]=(unsigned char)volume;keys[ch]=1;
 if(!audible())return;
 word=voice_pitch(ch);
 opl(0xb0+ch,reg_cache[0xb0+ch]&0x1f);
 opl(0x43+op_slot[ch],carrier_level(ch));
 opl(0xa0+ch,word&255);opl(0xb0+ch,0x20|(word>>8));note_count++;
}
static void voice_off(unsigned ch){
 keys[ch]=0;gates[ch]=0;
 if(audible())opl(0xb0+ch,reg_cache[0xb0+ch]&0x1f);
}
static void voice_gate(unsigned ch,unsigned pitch,unsigned volume,unsigned duration){
 voice_on(ch,pitch,volume);gates[ch]=(unsigned short)duration;
}
static void restore_voices(void){
 unsigned i;
 if(!audible())return;
 opl(0x41,bass_filter);opl(0x51,level(hat_level));
 for(i=0;i<6;i++)if(keys[i])voice_on(i,pitches[i],levels[i]);
}
static void drums(unsigned bits,unsigned snare_level){
 rhythm=(unsigned char)bits;
 if(audible()){
  opl(0x54,level(snare_level));opl(0x51,level(hat_level));
  opl(0xbd,0x20);opl(0xbd,0x20|bits);
  if(bits)note_count++;
 }
}
static void kick(void){
 unsigned i,word=pitch_word[kick_sweep[0]];
 kick_age=0;kick_active=1;kick_pulse=12;duck_age=0;duck_level=duck_curve[0];
 if(audible()){
  /* The OPL rhythm key in BD triggers this channel, never B6 bit 5. */
  opl(0xa6,word&255);opl(0xb6,word>>8);
  for(i=3;i<6;i++)if(keys[i])opl(0x43+op_slot[i],carrier_level(i));
  if(keys[0])opl(0x43,carrier_level(0));
 }
}
static void chord(unsigned cycle,unsigned strength,unsigned duration){
 unsigned i;
 for(i=0;i<3;i++)voice_gate(3+i,chords[cycle][i],strength+i*2,duration);
}
static void lead(unsigned note,unsigned volume,unsigned duration,unsigned pos){
 lead_age=0;voice_gate(2,note,volume,duration);
 echo_note[(pos+3)&15]=(unsigned char)note;
}
static void play_step(void){
 unsigned s=sequencer_step,bar=s>>4,pos=s&15,cycle=bar&7;
 unsigned root=roots[cycle],offset,bits=0,note;
 unsigned breakdown=bar>=40 && bar<48,build=bar>=48 && bar<56;
 unsigned main=(bar>=8 && bar<40) || (bar>=56 && bar<88);
 unsigned answer=((bar>=72) ^ (score_cycle&1)) && main;
 unsigned bass=bar<44 || bar>=48;
 unsigned kick_hit=!(pos&3) && !breakdown;
 if(bar==55 && pos>=8)kick_hit=0;
 if(kick_hit){kick();bits|=BD;}
 bass_shade=(unsigned char)(main?0:5);
 if(build)bass_shade=(unsigned char)(10-(bar-48));
 if(bass){
  offset=(main && (bar&2))?bass_b[pos]:bass_a[pos];
  if(bar>=40 && bar<44 && pos)offset=REST;
  if(bar==55 && pos>=12)offset=REST;
  if(offset!=REST){
   bass_age=0;bass_filter=(unsigned char)(bass_curve[0]+bass_shade);
   if(audible())opl(0x41,bass_filter);
   voice_gate(1,root+offset,(pos&3)?6:3,
              (bar>=40 && bar<44)?54:((pos&3)==0?10:7));
  }
 }
 /* Two-bar voicings, with one-bar dominant turnarounds. */
 if(!pos && (!(cycle&1) || cycle>=6))chord(cycle,breakdown?20:22,cycle<6?336:168);
 /* Echo is a quiet independent FM voice, not PCM or software mixing. */
 note=echo_note[pos];echo_note[pos]=REST;
 if(note!=REST)voice_gate(0,note,26,12);
 else if((pos&3)==2 && (!main || (bar&3)==2)){
  note=chords[cycle][arp_order[((pos>>1)+(bar&1))&7]];
  voice_gate(0,note+12,breakdown?26:30,breakdown?18:9);
 }
 if(main){
  note=answer?hook_b[cycle][pos]:hook[cycle][pos];
  if(note!=REST)lead(note,12,(pos==0||pos==8)?24:18,pos);
 }else if((pos&3)==0){
  note=chords[cycle][arp_order[((pos>>1)+(bar&1))&7]];
  if(breakdown)note+=12;
  if(bar<8 && (pos&7))note=REST;
  if(bar>=88)note=hook[cycle][pos];
  if(note!=REST)lead(note,breakdown?21:19,breakdown?32:22,pos);
 }
 hat_level=(unsigned char)((pos&3)==2?23:((pos&1)?36:31));
 if(!breakdown){
  if(!(pos&1) || (main && (bar&1)))bits|=HH;
  if(pos==4 || pos==12){bits|=SD;clap_delay=2;}
 }
 if(breakdown && (bar&1) && pos==12)bits|=SD;
 if(main && (bar&7)==7 && pos>=13){
  if(pos==13 || pos==15)bits|=TOM;else bits|=SD;
 }
 if(build && bar>=52){
  if((bar<54 && !(pos&3)) || (bar==54 && !(pos&1)) ||
     (bar==55 && pos<14))bits|=SD;
 }
 if(bar==95 && pos>=12)bits|=(pos&1)?TOM:SD;
 if(!pos && (bar==0 || bar==8 || bar==24 || bar==40 || bar==56 ||
             bar==72 || bar==88))bits|=CYM;
 if(bits)drums(bits,(build && bar>=52)?(unsigned)(20-(bar-52)*3):7);
 if(++sequencer_step==SCORE_STEPS){sequencer_step=0;score_cycle++;}
}

/* Small tracker-style macros. Table lookups and cached register writes only;
 * no floating point, divides, sample mixing or unbounded work in IRQ0. */
static void macros(void){
 unsigned i,word,new_duck;
 if(kick_active){
  if(kick_age<5){
   kick_age++;word=pitch_word[kick_sweep[kick_age]];
   if(audible()){opl(0xa6,word&255);opl(0xb6,word>>8);}
  }else kick_active=0;
 }
 macro_divider^=1;if(macro_divider)return;
 if(duck_age<6){
  duck_age++;new_duck=duck_curve[duck_age];
  if(new_duck!=duck_level){
   duck_level=(unsigned char)new_duck;
   if(audible()){
    for(i=3;i<6;i++)if(keys[i])opl(0x43+op_slot[i],carrier_level(i));
    if(keys[0])opl(0x43,carrier_level(0));
   }
  }
 }
 if(keys[1] && bass_age<6){
  bass_age++;bass_filter=(unsigned char)(bass_curve[bass_age]+bass_shade);
  if(audible())opl(0x41,bass_filter);
 }
 if(keys[2] && lead_age<4){
  lead_age++;word=voice_pitch(2);
  if(audible()){opl(0xa2,word&255);opl(0xb2,0x20|(word>>8));}
 }
}

/* Returns nonzero on the interrupts that belong to the original BIOS. */
static int sequencer_tick(void){
 unsigned i;
 int chain;
 bios_divider=(unsigned char)((bios_divider+1)&3);chain=!bios_divider;
#ifdef HOST
 if(chain)bios_chains++;
#endif
 if(paused)return chain;
 clock_divider=(unsigned char)((clock_divider+1)&3);
 if(!clock_divider)clock_count++;
 if(kick_pulse)kick_pulse--;
 if(rhythm){rhythm=0;if(audible())opl(0xbd,0x20);}
 for(i=0;i<6;i++)if(gates[i] && !--gates[i])voice_off(i);
 if(clap_delay && !--clap_delay)drums(SD,18);
 macros();
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
 /* Hardware BD: a clean low sine, rapid live-pitch attack/body sweep. */
 slot=op_slot[6];
 opl(0x20+slot,1);opl(0x23+slot,1);
 opl(0x40+slot,63);opl(0x43+slot,level(1));
 opl(0x60+slot,0xf8);opl(0x63+slot,0xf7);
 opl(0x80+slot,0x77);opl(0x83+slot,0x77);
 opl(0xe0+slot,0);opl(0xe3+slot,0);opl(0xc6,0x00);
 for(i=7;i<9;i++){
  slot=op_slot[i];
  opl(0x20+slot,1);opl(0x23+slot,1);
  opl(0x40+slot,level(i==7?30:16));opl(0x43+slot,level(i==7?7:29));
  opl(0x60+slot,i==7?0xfd:0xf8);opl(0x63+slot,i==7?0xf6:0xf5);
  opl(0x80+slot,i==7?0xdd:0xa8);opl(0x83+slot,i==7?0x96:0x75);
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
 bios_divider=clock_divider=score_cycle=0;rhythm=kick_age=kick_active=kick_pulse=0;
 step_phase=STEP_CLOCK-STEP_ADD;bass_filter=27;hat_level=30;
 duck_age=6;duck_level=0;bass_age=6;bass_shade=5;lead_age=4;clap_delay=0;macro_divider=0;
 for(i=0;i<16;i++)echo_note[i]=REST;
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
 /* Changing a visual mode leaves this continuous soundtrack intact. */
 (void)wanted;
}
unsigned music_pulse(void){return kick_pulse;}
unsigned music_beat(void){return sequencer_step;}
unsigned music_notes(void){return note_count;}
int music_present(void){return available;}

#ifdef HOST
void sunset_test_tick(void){if(hooked)sequencer_tick();}
void sunset_test_clock(unsigned long value){clock_count=(Clock32)value;}
unsigned long sunset_test_bios(void){return bios_chains;}
unsigned long sunset_test_phase(void){return step_phase;}
unsigned sunset_test_cycle(void){return score_cycle;}
#endif
