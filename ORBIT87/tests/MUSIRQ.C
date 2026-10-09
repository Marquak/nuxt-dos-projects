/* Real-mode test: INT08 ownership, BIOS chaining, PIT divisor restoration,
 * silent clock, pause/mute and shutdown. No output while timer is active.
 */
#include "../ORBIT.H"
#include <dos.h>
#include <conio.h>
#include <stdio.h>
static unsigned bt(void){return *(volatile unsigned __far *)MK_FP(0x40,0x6c);}
static void waitticks(unsigned n){unsigned start=bt();while((unsigned)(bt()-start)<n){} }
static unsigned lock_irq(void);
#pragma aux lock_irq = "pushf" "pop ax" "cli" value [ax];
static void unlock_irq(unsigned flags);
#pragma aux unlock_irq = "push ax" "popf" parm [ax];
static unsigned pit(void){
 unsigned f=lock_irq(),lo,hi;
 outp(0x43,0);lo=inp(0x40);hi=inp(0x40);unlock_irq(f);return lo|(hi<<8);
}
static unsigned pitmax(void){
 unsigned i,max=0,v;for(i=0;i<4096;i++){v=pit();if(v>max)max=v;}return max;
}
int main(void){
 void (__interrupt __far *old8)(void)=_dos_getvect(8);
 void (__interrupt __far *old1c)(void)=_dos_getvect(0x1c);
 unsigned begin,end,elapsed,notes,before,fast,normal,paused_bios,silent_elapsed;
 unsigned long count,pauseclock,silent_count;
 int present,restored,ok,owned,silent_ok;
 FILE *fp;
 present=music_start(0);music_section(4);
 owned=(_dos_getvect(8)!=old8 && _dos_getvect(0x1c)==old1c);
 fast=pitmax();begin=bt();count=music_ticks();waitticks(36);end=bt();elapsed=(unsigned)(end-begin);count=music_ticks()-count;notes=music_notes();
 music_pause(1);pauseclock=music_ticks();begin=bt();waitticks(5);paused_bios=(unsigned)(bt()-begin);
 ok=(music_ticks()==pauseclock && paused_bios>=5);
 music_pause(0);music_mute(1);before=music_notes();waitticks(5);
 ok=ok && (before==music_notes()) && (music_ticks()>pauseclock);
 music_mute(0);waitticks(5);music_stop();
 restored=(_dos_getvect(8)==old8 && _dos_getvect(0x1c)==old1c);
 normal=pitmax();
 silent_ok=!music_start(1);begin=bt();waitticks(12);silent_elapsed=(unsigned)(bt()-begin);silent_count=music_ticks();
 music_stop();silent_ok=silent_ok && silent_count>=11 && silent_count<=14 && _dos_getvect(8)==old8;
 fp=fopen("MUSIRQ.TXT","wt");if(!fp)return 2;
 fprintf(fp,"AdLib present %d\nIRQ08 owned / INT1C unchanged %d\nBIOS elapsed %u\nMusic elapsed %lu\nNotes %u\nPause/mute %d\nPaused BIOS ticks %u\nVectors restored %d\nPIT fast max %u\nPIT restored max %u\nSilent BIOS/music %u/%lu\nSilent clock/restoration %d\n",
         present,owned,elapsed,count,notes,ok,paused_bios,restored,fast,normal,silent_elapsed,silent_count,silent_ok);
 fclose(fp);
 return !present || !owned || count<35 || count>37 || notes<5 || !ok || !restored || fast>16384 || fast<10000 || normal<40000 || !silent_ok;
}
