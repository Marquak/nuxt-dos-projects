/* STARFIELD: sparse VGA star flight for the NuXT V20 + optional 8087.
 * No sound, IRQ replacement, full-screen redraw, or animation-time FP.
 */
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAXSTARS 256
#define MAXEVENTS (MAXSTARS*8)
#define NONE 0xffffU
typedef struct { int x,y,z,sx,sy; unsigned char size,shade; } Star;
typedef struct { unsigned x,next; unsigned char color; } Event;
typedef struct { unsigned bank,offset; unsigned char data[8]; } Write;
static Star stars[MAXSTARS];
static Event events[MAXEVENTS];
static Write writes[MAXEVENTS];
static unsigned heads[768],event_count,write_count;
static unsigned char row[1024],marked[128],dirtyrows[96],spans[128];
static unsigned char info[512];
static unsigned coeff_x[2048],coeff_y[2048];
static uint32_t rowaddr[768];
static int width,height,depth,driver,bank=-1,window;
static unsigned pitch,segment=0xa000,gran=64,span_k=64;
static unsigned bank_factor,window_bytes;
static unsigned char trident_flags;
static volatile int running=1;
static unsigned count=128,speed=12,seed=0xace1;
static unsigned long frames,elapsed,uploaded;
static int have_fpu;
static unsigned ticks(void){return *(volatile unsigned __far *)MK_FP(0x40,0x6c);}
static void idle(void);
#pragma aux idle = "sti" "hlt";
static void __interrupt __far stop(void){running=0;}
static unsigned random16(void){seed^=seed<<7;seed^=seed>>9;seed^=seed<<8;return seed;}

int probe87(unsigned *scratch);
#pragma aux probe87 parm [si] value [ax] modify exact [ax];
void control87(unsigned *cw);
#pragma aux control87 parm [si] modify exact [];
void setcontrol87(unsigned *cw);
#pragma aux setcontrol87 parm [si] modify exact [];
void finish87(unsigned *cw);
#pragma aux finish87 parm [si] modify exact [];
void table87(long *numerator,unsigned *out);
#pragma aux table87 parm [si] [di] modify exact [cx];

/* Signed 16x16 -> 32 multiply, then arithmetic >>8 without a library call.
 * Bounds: coordinate +/-1024, coefficient <=2048, result fits int16. */
static int project(int world,unsigned coefficient);
#pragma aux project = "imul bx" "mov al,ah" "mov ah,dl" \
 parm [ax] [bx] value [ax] modify exact [ax dx];

#include "VIDEO.INC"

static void make_tables(int use87){
 unsigned i,cw,truncation=0x0f7f;long n;
 n=(long)(width/2)*256L;
 memset(coeff_x,0,sizeof(coeff_x));memset(coeff_y,0,sizeof(coeff_y));
 if(use87){control87(&cw);setcontrol87(&truncation);table87(&n,coeff_x);}
 else for(i=64;i<2048;i++)coeff_x[i]=(unsigned)(n/i);
 /* 320x200 pixels are taller on a 4:3 display. Correct Y focal length. */
 n=(long)(height*2/3)*256L;
 if(use87){table87(&n,coeff_y);finish87(&cw);}
 else for(i=64;i<2048;i++)coeff_y[i]=(unsigned)(n/i);
 for(i=0;i<(unsigned)height;i++)rowaddr[i]=(uint32_t)i*pitch;
}

static void palette(void){
 int i,k;union REGS r;
 if(depth==16){
  for(i=0;i<16;i++){memset(&r,0,sizeof(r));r.w.ax=0x1000;r.h.bl=i;r.h.bh=i;int86(0x10,&r,&r);}
  memset(&r,0,sizeof(r));r.w.ax=0x1013;r.h.bl=0;r.h.bh=0;int86(0x10,&r,&r);
  r.w.ax=0x1013;r.h.bl=1;r.h.bh=0;int86(0x10,&r,&r);
  outp(0x3d4,0x14);outp(0x3d5,inp(0x3d5)&~0x40);
  outp(0x3d4,0x17);outp(0x3d5,inp(0x3d5)|0x40);
  outpw(0x3c4,0x0604);outpw(0x3ce,0x0506);
  outpw(0x3ce,0x0000);outpw(0x3ce,0x0001);outpw(0x3ce,0x0003);
  outpw(0x3ce,0x0005);outpw(0x3ce,0xff08);outpw(0x3c4,0x0f02);
 }
 outp(0x3c6,255);outp(0x3c8,0);
 for(i=0;i<256;i++){
  k=i<16?i:0;
  outp(0x3c9,k*63/15);outp(0x3c9,k?k*3+18:0);outp(0x3c9,k?k*2+33:0);
 }
}

static void spawn(Star *s,int initial){
 s->x=(int)(random16()&2047)-1024;s->y=(int)(random16()%1536)-768;
 /* Avoid a central stationary dot; a fresh star starts behind the viewer's field. */
 if(s->x>-12&&s->x<12)s->x+=24;
 s->z=initial?64+(random16()%1984):1536+(random16()&511);
}
static void locate(Star *s){
 s->sx=width/2+project(s->x,coeff_x[s->z]);
 s->sy=height/2+project(s->y,coeff_y[s->z]);
 s->shade=(unsigned char)(2+((2047-s->z)*13/1984));
 s->size=(unsigned char)(s->z<512?2:1);
 if(s->sx<0||s->sy<0||s->sx+s->size>width||s->sy+s->size>height)s->size=0;
}

static void events_begin(void){
 memset(heads,0xff,height*sizeof(heads[0]));memset(dirtyrows,0,sizeof(dirtyrows));event_count=0;
}
static void point(unsigned x,unsigned y,unsigned color){
 Event *e;
 if(x>=(unsigned)width||y>=(unsigned)height||event_count>=MAXEVENTS)return;
 e=&events[event_count];e->x=x;e->color=(unsigned char)color;e->next=heads[y];heads[y]=event_count++;
 dirtyrows[y>>3]|=(unsigned char)(1U<<(y&7));
}
static void footprint(Star *s,int erase){
 unsigned x,y;
 for(y=0;y<s->size;y++)for(x=0;x<s->size;x++)point(s->sx+x,s->sy+y,erase?0:s->shade);
}

/* Build the final contents of each affected 8-pixel span in RAM, then
 * write it once. Old stars never erase new stars at crossing positions.
 * Planar bytes are grouped by plane, with only four map-mask changes. */
static void compose(void){
 unsigned y,j,e,x,c,p,k,n,ns;uint32_t a,bytes=(uint32_t)span_k*1024UL;Write *w;
 write_count=0;
 for(y=0;y<(unsigned)height;y++){
  if(!(dirtyrows[y>>3]&(1U<<(y&7))))continue;
  memset(marked,0,width/8);ns=0;
  for(e=heads[y];e!=NONE;e=events[e].next){
   x=events[e].x;c=events[e].color;j=x>>3;
   if(!marked[j]){marked[j]=1;spans[ns++]=(unsigned char)j;memset(row+j*8,0,8);}
   if(row[x]<c)row[x]=(unsigned char)c;
  }
  for(n=0;n<ns;n++){
   j=spans[n];
   w=&writes[write_count++];a=rowaddr[y]+(depth==16?j:j*8);
   if(span_k==64){w->bank=(unsigned)(a>>16);w->offset=(unsigned)a;}
   else{w->bank=(unsigned)(a/bytes);w->offset=(unsigned)(a%bytes);}
   if(depth==256)memcpy(w->data,row+j*8,8);
   else{
    memset(w->data,0,4);
    for(k=0;k<8;k++){
     c=row[j*8+k];if(!c)continue;x=128U>>k;
     for(p=0;p<4;p++)if(c&(1U<<p))w->data[p]|=(unsigned char)x;
    }
   }
  }
 }
}

static void copy8(unsigned seg,unsigned offset,const unsigned char *src);
#pragma aux copy8 = "push es" "mov es,ax" "mov ax,[si]" "mov es:[di],ax" \
 "mov ax,[si+2]" "mov es:[di+2],ax" "mov ax,[si+4]" "mov es:[di+4],ax" \
 "mov ax,[si+6]" "mov es:[di+6],ax" "pop es" \
 parm [ax] [di] [si] modify exact [ax];

static void present(void){
 unsigned p,i,k,b,off;Write *w;unsigned char __far *v;
 if(depth==16){
  for(p=0;p<4;p++){
   outpw(0x3c4,((1U<<p)<<8)|2);
   for(i=0;i<write_count;i++){w=&writes[i];if(driver)setbank(w->bank);v=MK_FP(segment,w->offset);*v=w->data[p];}
  }
  outpw(0x3c4,0x0f02);uploaded+=(unsigned long)write_count*4;
 }else{
  for(i=0;i<write_count;i++){
   w=&writes[i];if(driver)setbank(w->bank);
   if(w->offset<=65528U&&(span_k==64||w->offset<=window_bytes-8))copy8(segment,w->offset,w->data);
   else{
    b=w->bank;off=w->offset;
    for(k=0;k<8;k++){
     if(k&&(span_k==64?!off:off==window_bytes)){off=0;if(driver)setbank(++b);}
     v=MK_FP(segment,off++);*v=w->data[k];
    }
   }
  }
  uploaded+=(unsigned long)write_count*8;
 }
}

static void advance(unsigned delta){
 unsigned i;Star *s;events_begin();
 for(i=0;i<count;i++){
  s=&stars[i];footprint(s,1);s->z-=(int)(speed*delta);
  if(s->z<64||!s->size)spawn(s,0);
  locate(s);footprint(s,0);
 }
 compose();present();frames++;
}

static int flight(unsigned limit,int report){
 unsigned i,last,now,d,settle;int key,paused=0;FILE *log;
 /* Let emulators finish deferred mode changes before touching VGA state. */
 settle=ticks();while((unsigned)(ticks()-settle)<2&&running)idle();
 palette();if(depth==16&&!planar_probe())return 0;
 clear_video();make_tables(have_fpu);
 frames=elapsed=uploaded=0;memset(stars,0,sizeof(stars));
 events_begin();for(i=0;i<count;i++){spawn(&stars[i],1);locate(&stars[i]);footprint(&stars[i],0);}compose();present();
 last=ticks();
 while(running){
  if(kbhit()){
   key=getch();if(key==27||key=='q'||key=='Q')break;
   if(key==' ')paused=!paused;
   if((key=='+'||key=='=')&&speed<64)speed=speed<63?speed+2:64;
   if((key=='-'||key=='_')&&speed>2)speed=speed>3?speed-2:2;
  }
  now=ticks();d=(unsigned)(now-last);
  if(d){last=now;if(!paused){elapsed+=d;if(d>8)d=8;advance(d);if(limit&&frames>=limit)break;}}
  else idle();
 }
 if(report){log=fopen("STAR.LOG","w");if(log){fprintf(log,"width=%d height=%d colours=%d\n8087=%d stars=%u speed=%u\nframes=%lu ticks=%lu uploaded_bytes=%lu\n",width,height,depth,have_fpu,count,speed,frames,elapsed,uploaded);fclose(log);}}
 return 1;
}

static void help(void){
 puts("STARFIELD - silent NuXT star flight\nSTAR [/320|/640|/800|/1024] [/16|/256] [/AUTO] [/NO87]\n     [/STARS:16..256] [/SPEED:2..64] [/FRAMES:1..65000] [/REPORT]\nDefault: 640x480, 16 colours, 128 stars. Menu unless /AUTO.\nSPACE pauses; +/- changes speed; ESC/Q returns to menu (or exits /AUTO).\n/REPORT writes STAR.LOG on return. /NO87 bypasses coprocessor detection.");
}
static unsigned number(const char *s,unsigned lo,unsigned hi){
 unsigned long n=0;if(!*s)return 0;
 while(*s){if(*s<'0'||*s>'9')return 0;n=n*10+(*s++-'0');if(n>hi)return 0;}
 return n<lo?0:(unsigned)n;
}
int main(int argc,char **argv){
 int i,res=0,color=0,auto_run=0,no87=0,report=0,key,oldmode,result=0;unsigned limit=0,n,probe[2];union REGS r;
 void (__interrupt __far *oldbreak)(void);
 for(i=1;i<argc;i++){
  if(!stricmp(argv[i],"/320"))res=3;else if(!stricmp(argv[i],"/640"))res=0;
  else if(!stricmp(argv[i],"/800"))res=1;else if(!stricmp(argv[i],"/1024"))res=2;
  else if(!stricmp(argv[i],"/16"))color=0;else if(!stricmp(argv[i],"/256"))color=1;
  else if(!stricmp(argv[i],"/AUTO"))auto_run=1;else if(!stricmp(argv[i],"/NO87"))no87=1;
  else if(!stricmp(argv[i],"/REPORT"))report=1;
  else if(!strnicmp(argv[i],"/STARS:",7)&&(n=number(argv[i]+7,16,256)))count=n;
  else if(!strnicmp(argv[i],"/SPEED:",7)&&(n=number(argv[i]+7,2,64)))speed=n;
  else if(!strnicmp(argv[i],"/FRAMES:",8)&&(n=number(argv[i]+8,1,65000)))limit=n;
  else{help();return stricmp(argv[i],"/?")?1:0;}
 }
 memset(&r,0,sizeof(r));r.w.ax=0x1a00;int86(0x10,&r,&r);if(r.h.al!=0x1a){puts("A VGA-compatible card is required.");return 1;}
 r.h.ah=15;int86(0x10,&r,&r);oldmode=r.h.al&127;
 have_fpu=no87?0:probe87(probe);seed=ticks()|1;
 oldbreak=_dos_getvect(0x23);_dos_setvect(0x23,stop);
 while(running){
  if(!auto_run){
   biosmode(3);
   printf("STARFIELD - NuXT\nPerspective tables: %s\n\n",have_fpu?"8087 / x87":"integer fallback");
   puts("1: 320x200    2: 640x480    3: 800x600    4: 1024x768\nC: 16/256 colours    D: star density    ENTER: start    ESC/Q: exit\n\nIn flight: SPACE pauses, +/- speed, ESC/Q returns. No sound.");
   printf("\nSelected: %s / %d colours / %u stars\n",res==3?"320x200":res==0?"640x480":res==1?"800x600":"1024x768",color?256:16,count);
   key=getch();if(key==27||key=='q'||key=='Q')break;
   if(key>='1'&&key<='4'){res=key=='1'?3:key-'2';continue;}
   if(key=='c'||key=='C'){color=!color;continue;}
   if(key=='d'||key=='D'){count=count<64?64:count<128?128:count<256?256:32;continue;}
   if(key!=13)continue;
  }
  if(!startmode(res,color)||!flight(limit,report)){
   biosmode(3);puts("This video mode is unavailable or failed its memory check.\n1024x768/256 needs more than the NuXT's 512 KB VRAM.");
   if(auto_run){result=1;break;}puts("Press a key for the menu.");getch();
  }
  if(auto_run)break;
 }
 biosmode(oldmode);_dos_setvect(0x23,oldbreak);
 printf("Starfield closed. %lu frames. Video restored.\n",frames);return result;
}
