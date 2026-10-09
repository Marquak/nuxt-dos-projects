/* Deterministic CRC32 equality + BIOS-tick benchmark of C and 8086 line paths.
 * Run under DOS. All visible bytes of all four VGA planes are verified. */
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include "../ORBIT.H"
static unsigned random_state;
static unsigned rnd(void){random_state=random_state*25173U+13849U;return random_state;}
static unsigned ticks(void){return *(volatile unsigned __far *)MK_FP(0x40,0x6c);}
static void workload(void){
 int i,j,w=video_width(),h=video_height(),x,y,dx,dy;unsigned c;
 random_state=12345;
 /* Every octant, ties, zero lengths, colour zero, reversed endpoint order,
  * clipped lines and pixel-bit / scanline / 64K-bank boundaries. */
 for(i=0;i<8;i++)for(j=0;j<64;j++){
  x=rnd()%w;y=rnd()%h;dx=(int)(rnd()%120)-60;dy=(int)(rnd()%120)-60;c=rnd()&15;
  video_line(x,y,x+dx,y+dy,c);
 }
 for(i=0;i<32;i++){
  video_line(-i*7,i*19,w+i*3,h-1-i*13,(i%15)+1);
  video_line(w-1-i*31,h+64,i*17,-64,(i+5)&15);
 }
 for(i=0;i<16;i++){
  video_line(i,0,w-1-i,0,i);video_line(0,i,0,h-1-i,i);
  video_line(w-1,i,w-1,h-1-i,i);video_line(i,h-1,w-1-i,h-1,i);
  video_pixel(i*13,i*7,i);video_line(44+i,44+i,44+i,44+i,i);
 }
 for(i=1;i<32;i++){
  video_line(0,512-i,w-1,512+i,i&15);
  video_line(0,512+i,w-1,512-i,(i+7)&15);
  video_line(w/2-i,0,w/2+i,h-1,i&15);
 }
 video_line(0,511,w-1,512,15);video_line(0,512,w-1,511,14);
 video_line(w/2,h/2,-32768,-32768,1);
 video_line(w/2,h/2,-32768,32767,2);
 video_line(w/2,h/2,32767,-32768,3);
 video_line(w/2,h/2,32767,32767,4);
 video_line(-32768,-32768,32767,32767,13);
 video_line(32767,-32768,-32768,32767,12);
 video_line(-32768,0,32767,h-1,11);
 video_line(0,-32768,w-1,32767,10);
 video_line(-32768,-7,32767,-7,15); /* completely invisible */
 video_text(16,16,15,"ORBIT 87: VECTOR TEST",1);
 video_text(w-90,h-22,7,"BANK / EDGE",2);
 video_text(19,79,3,"SCALE 4",4);
 video_text(19,79,14,"UPDATE ",4);
}
int main(int argc,char **argv){
 FILE *f=fopen("VIDTEST.TXT","w");unsigned start,elapsed[2],warm;unsigned long crc[2],post[2];int res,pass,fail=0,first=0,last=2;
 if(!f)return 2;if(argc>1)first=last=atoi(argv[1]);
 fprintf(f,"ORBIT87 C versus 8086 assembly; timings are DOSBox BIOS ticks, not bare-metal claims.\n");
 for(res=first;res<=last;res++){
  if(!video_open(res)){fprintf(f,"Resolution %d unavailable; correctly returned to text mode.\n",res);fflush(f);continue;}
  fprintf(f,"%dx%d %s\n",video_width(),video_height(),video_driver());fflush(f);
  for(pass=0;pass<2;pass++){
   video_clear();video_reference(pass==0);warm=ticks();while(ticks()==warm){}start=ticks();workload();elapsed[pass]=ticks()-start;
   crc[pass]=video_checksum();
   /* Checksum changes VGA read-plane state: drawing afterwards must still work. */
   video_line(1,1,video_width()-2,video_height()-2,3);
   video_line(video_width()-2,1,1,video_height()-2,14);
   post[pass]=video_checksum();
  }
  video_close();
  fprintf(f,"C:   %u ticks CRC32 %08lx post %08lx\nASM: %u ticks CRC32 %08lx post %08lx\n",elapsed[0],crc[0],post[0],elapsed[1],crc[1],post[1]);
  if(crc[0]!=crc[1]||post[0]!=post[1]){fprintf(f,"FAIL mismatch\n");fail=1;}else fprintf(f,"PASS exact frame and post-readback match\n");fflush(f);
 }
 fclose(f);return fail;
}
