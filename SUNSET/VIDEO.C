/* SUNSET video dispatcher: standard linear VGA 320x200, banked high-res
 * 256-colour modes, or true four-plane 16-colour modes through PLANAR.C.
 * Completed RAM-composed spans replace old pixels without an erase pass.
 * V20/80186 small model; hot string transfers keep music IRQs enabled. */
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdint.h>
#include "SUNSET.H"

typedef struct { unsigned off,bank; } Scanline;
static Scanline rows[600];
static unsigned char info[512];
static unsigned char probe_saved[512][2],probe_pixels[256];
static unsigned pitch,segment,span_k,bank_factor,window,window_bytes;
static int width,height,driver,current_bank=-1,opened,palette_vbe;
static unsigned char trident_flags;
static unsigned wanted_colors=256;
static int plane_native;
void video_depth(unsigned n){wanted_colors=n==16?16:256;}
unsigned video_colors(void){return wanted_colors;}

static unsigned word(unsigned at){return info[at]|((unsigned)info[at+1]<<8);}
static int bios_palette(unsigned first,unsigned count,const unsigned char *rgb){
 unsigned i,n;union REGS r;struct SREGS sr;
 /* The mode-info scratch buffer is no longer needed once a mode is set.
  * VBE palette entries are blue, green, red, reserved; a new mode uses a
  * six-bit DAC. Chunking avoids another 1 KB of scarce near data. */
 while(count){
  n=count>128?128:count;
  for(i=0;i<n;i++){
   info[i*4]=rgb[i*3+2]&63;info[i*4+1]=rgb[i*3+1]&63;
   info[i*4+2]=rgb[i*3]&63;info[i*4+3]=0;
  }
  memset(&r,0,sizeof(r));segread(&sr);r.w.ax=0x4f09;r.w.cx=n;r.w.dx=first;
  r.w.di=FP_OFF(info);sr.es=FP_SEG(info);int86x(0x10,&r,&r,&sr);
  current_bank=-1;if(r.w.ax!=0x004f)return 0;
  count-=n;first+=n;rgb+=n*3;
 }
 return 1;
}
static void biosmode(unsigned mode){
 union REGS r;memset(&r,0,sizeof(r));r.w.ax=mode;int86(0x10,&r,&r);
}
/* Avoid touching vendor registers on an unrelated VGA. A ROM signature and
 * the precise 9000i ID are both required before using its banking register. */
static int trident9000i(void){
 unsigned i;const char __far *rom=MK_FP(0xc000,0);
 for(i=0;i<32760;i++){
  if(!_fmemcmp(rom+i,"Trident",7)||!_fmemcmp(rom+i,"TRIDENT",7)){
   outp(0x3c4,0x0b);return inp(0x3c5)==0x43;
  }
 }
 return 0;
}
static int setbank(unsigned b){
 union REGS r;
 if(current_bank==(int)b)return 1;
 if(driver==3){if(b)return 0;current_bank=0;return 1;}
 if(driver==2){
  /* Reading SR0B selected the new register set. SR0E bit 1 is inverted. */
  outpw(0x3c4,((trident_flags|(b^2))<<8)|0x0e);
 }else if(driver==1){
  memset(&r,0,sizeof(r));r.w.ax=0x4f05;r.w.bx=window;r.w.dx=b*bank_factor;
  int86(0x10,&r,&r);
  if(r.w.ax!=0x004f){current_bank=-1;return 0;}
 }else return 0;
 current_bank=(int)b;return 1;
}
static volatile unsigned char __far *address(uint32_t a){
 unsigned b,off;
 if(span_k==64){b=(unsigned)(a>>16);off=(unsigned)a;}
 else {b=(unsigned)(a/window_bytes);off=(unsigned)(a-(uint32_t)b*window_bytes);}
 if(!setbank(b))return 0;
 return MK_FP(segment,off);
}
static void settle_mode(void){
 volatile unsigned __far *tick=MK_FP(0x40,0x6c);unsigned before=*tick;
 unsigned long guard=1000000UL;
 /* Allow the new mode's timing/window state to settle before VRAM probes.
  * Two BIOS tick changes provide at least one complete ~55 ms interval.
  * IRQs remain enabled; the finite guard also handles a stopped BIOS clock. */
 while((unsigned)(*tick-before)<2U&&--guard){}
}
static int native_mode(int res){
 union REGS r;unsigned mode=res?0x5e:0x5d;
 if(!trident9000i())return 0;
 biosmode(mode);memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);
 if((r.h.al&127)!=mode||!trident9000i())return 0;
 settle_mode();driver=2;palette_vbe=0;pitch=(unsigned)width;segment=0xa000;span_k=64;
 window_bytes=0;bank_factor=1;current_bank=-1;
 outp(0x3c4,0x0e);trident_flags=inp(0x3c5)&0xf0;
 return 1;
}
static int vbe_mode(int res){
 union REGS r;struct SREGS sr;unsigned attr,gran,size;int win;
 static const unsigned char black[3]={0,0,0};
 unsigned mode=res?0x103:0x101;
 memset(info,0,sizeof(info));memset(&r,0,sizeof(r));segread(&sr);
 r.w.ax=0x4f01;r.w.cx=mode;r.w.di=FP_OFF(info);sr.es=FP_SEG(info);
 int86x(0x10,&r,&r,&sr);
 if(r.w.ax!=0x004f)return 0;
 attr=word(0);
 /* Supported, colour, graphics and windowed; 1 plane,
  * exactly 8 bits/pixel, packed-pixel model. No planar-mode fallback. */
 if((attr&0x59)!=0x19||word(18)!=(unsigned)width||
    word(20)!=(unsigned)height||info[24]!=1||info[25]!=8||info[27]!=4)return 0;
 win=(info[2]&7)==7?0:(info[3]&7)==7?1:-1;
 if(win<0)return 0;
 gran=word(4);size=word(6);
 if(!gran||gran>64||!size)return 0;
 span_k=size>64?64:size;
 if(span_k%gran)return 0;
 pitch=word(16);segment=word(win?10:8);
 if(!segment||pitch<(unsigned)width||(uint32_t)pitch*height>524288UL)return 0;
 memset(&r,0,sizeof(r));r.w.ax=0x4f02;r.w.bx=mode;int86(0x10,&r,&r);
 if(r.w.ax!=0x004f)return 0;
 settle_mode();driver=1;window=(unsigned)win;bank_factor=span_k/gran;
 window_bytes=span_k*1024U;current_bank=-1;palette_vbe=(attr&0x20)!=0;
 /* Modes explicitly marked non-VGA-compatible must support BIOS palette
  * writes. Never assume their hardware has the standard DAC/Pel-mask ports. */
 return !palette_vbe||bios_palette(0,1,black);
}

/* Prove the entire visible allocation has independent banks. Merely getting
 * AX=004F from a BIOS bank call does not prove its window moved. Each bank
 * receives a unique two-byte value before ANY of them are read back. */
static int probe_banks(void){
 uint32_t total=(uint32_t)pitch*height,bytes=(uint32_t)span_k*1024UL;
 unsigned b,count=(unsigned)((total+bytes-1)/bytes),saved=0;int ok=1;
 volatile unsigned char __far *v;
 if(!count||count>512)return 0;
 for(b=0;b<count;b++){
  v=address((uint32_t)b*bytes);if(!v){ok=0;break;}
  probe_saved[b][0]=v[0];probe_saved[b][1]=v[1];++saved;
 }
 if(ok)for(b=0;b<count;b++){
  v=address((uint32_t)b*bytes);if(!v){ok=0;break;}
  v[0]=(unsigned char)(b^0x5a);v[1]=(unsigned char)((b>>8)^0xa5);
 }
 if(ok)for(b=0;b<count;b++){
  v=address((uint32_t)b*bytes);
  if(!v||v[0]!=(unsigned char)(b^0x5a)||
     v[1]!=(unsigned char)((b>>8)^0xa5)){ok=0;break;}
 }
 for(b=0;b<saved;b++){
  v=address((uint32_t)b*bytes);
  if(v){v[0]=probe_saved[b][0];v[1]=probe_saved[b][1];}else ok=0;
 }
 if(!setbank(0))ok=0;
 return ok;
}
/* Exercise every 8-bit index at both ends of VRAM, preserving the bytes. */
static int probe_indices(uint32_t base){
 unsigned i,saved=0;int ok=1;volatile unsigned char __far *v;
 for(i=0;i<256;i++){
  v=address(base+i);if(!v){ok=0;break;}probe_pixels[i]=*v;++saved;
 }
 if(ok)for(i=0;i<256;i++){
  v=address(base+i);if(!v){ok=0;break;}*v=(unsigned char)(i^0xa5);
 }
 if(ok)for(i=0;i<256;i++){
  v=address(base+i);if(!v||*v!=(unsigned char)(i^0xa5)){ok=0;break;}
 }
 for(i=0;i<saved;i++){v=address(base+i);if(v)*v=probe_pixels[i];else ok=0;}
 if(!setbank(0))ok=0;return ok;
}
static int probe(void){
 return probe_banks()&&probe_indices(256)&&probe_indices((uint32_t)pitch*height-256);
}

int video_open(int res){
 unsigned y;uint32_t a,bytes;int ready=0;
 if(res<0||res>2){video_close();return 0;}
 width=res==2?320:res?800:640;height=res==2?200:res?600:480;driver=0;opened=0;current_bank=-1;
 if(wanted_colors==16){
  plane_native=res==1&&trident9000i();
  if(!plane_open(res,plane_native)){video_close();return 0;}
  driver=4;palette_vbe=0;opened=1;return 1;
 }
 if(res==2){
  union REGS r;biosmode(0x13);memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);
  if((r.h.al&127)==0x13){
   settle_mode();driver=3;palette_vbe=0;pitch=320;segment=0xa000;span_k=64;
   window_bytes=0;bank_factor=1;current_bank=-1;ready=probe();
  }
 }else if(native_mode(res))ready=probe();
 if(!ready&&res!=2){driver=0;current_bank=-1;if(vbe_mode(res))ready=probe();}
 if(!ready){video_close();return 0;}
 bytes=(uint32_t)span_k*1024UL;
 for(y=0;y<(unsigned)height;y++){
  a=(uint32_t)y*pitch;rows[y].bank=(unsigned)(a/bytes);rows[y].off=(unsigned)(a%bytes);
 }
 if(!palette_vbe)outp(0x3c6,255);opened=1;
 if(!video_clear(0)){video_close();return 0;}
 return 1;
}
void video_close(void){plane_close();biosmode(3);opened=0;driver=0;palette_vbe=0;current_bank=-1;}
int video_width(void){return width;}
int video_height(void){return height;}
const char *video_driver(void){
 if(driver==4)return plane_native?"TVGA9000i planar 16":"VGA/VBE planar 16";
 if(driver==3)return "VGA mode 13h / linear 256";
 return driver==2?"TVGA9000i native 256":driver==1?
  (palette_vbe?"VBE packed 256 / BIOS palette":"VBE packed 256"):"closed";
}
void video_palette(const unsigned char *rgb){
 video_palette_range(0,wanted_colors,rgb);
}
void video_palette_range(unsigned first,unsigned count,const unsigned char *rgb){
 unsigned i;
 if(!opened||first>=wanted_colors||count>wanted_colors-first)return;
 if(driver==4){plane_palette(first,count,rgb);return;}
 if(palette_vbe){if(!bios_palette(first,count,rgb))opened=0;return;}
 outp(0x3c6,255);outp(0x3c8,first);
 for(i=0;i<count*3;i++)outp(0x3c9,rgb[i]&63);
}

/* REP does not modify carry: after the word copy, ADC converts the saved
 * odd-byte flag to CX=0/1. Neither loop uses per-pixel BIOS/port operations.
 * The caller splits at aperture boundaries; no MOVSW crosses a bank. */
static void put_block(const unsigned char *src,unsigned off,unsigned n,unsigned seg);
#pragma aux put_block = \
 "push es" "mov es,ax" "cld" "shr cx,1" "rep movsw" \
 "adc cx,cx" "rep movsb" "pop es" \
 parm [si] [di] [cx] [ax] modify exact [si di cx];
static void get_block(unsigned char *dst,unsigned off,unsigned n,unsigned seg);
#pragma aux get_block = \
 "push ds" "push es" "push ds" "pop es" "mov ds,ax" "cld" \
 "shr cx,1" "rep movsw" "adc cx,cx" "rep movsb" "pop es" "pop ds" \
 parm [di] [si] [cx] [ax] modify exact [si di cx];
static void fill_block(unsigned off,unsigned n,unsigned color,unsigned seg);
#pragma aux fill_block = \
 "push es" "mov es,dx" "mov ah,al" "cld" "shr cx,1" \
 "rep stosw" "adc cx,cx" "rep stosb" "pop es" \
 parm [di] [cx] [ax] [dx] modify exact [ax di cx];

static int xy_address(int x,int y,unsigned n,unsigned *off,unsigned *bank){
 unsigned old;
 if(!opened||x<0||x>width||(unsigned)y>=(unsigned)height||n>(unsigned)(width-x))return 0;
 old=rows[y].off;*off=old+(unsigned)x;*bank=rows[y].bank;
 if(span_k==64){if(*off<old)++*bank;}
 else if(*off>=window_bytes){*off-=window_bytes;++*bank;}
 return 1;
}
static unsigned chunk(unsigned off,unsigned n){
 unsigned left;
 if(span_k==64){if(!off)return n;left=0U-off;}
 else left=window_bytes-off;
 return n<left?n:left;
}
int video_write_span(int x,int y,const unsigned char *data,unsigned n){
 unsigned off,b,part;
 if(driver==4)return plane_write(x,y,data,n);
 if(!xy_address(x,y,n,&off,&b))return 0;
 while(n){
  part=chunk(off,n);if(!setbank(b))return 0;
  put_block(data,off,part,segment);data+=part;n-=part;off=0;++b;
 }
 return 1;
}
int video_read_span(int x,int y,unsigned char *data,unsigned n){
 unsigned off,b,part;
 if(driver==4)return plane_read(x,y,data,n);
 if(!xy_address(x,y,n,&off,&b))return 0;
 while(n){
  part=chunk(off,n);if(!setbank(b))return 0;
  get_block(data,off,part,segment);data+=part;n-=part;off=0;++b;
 }
 return 1;
}
int video_clear(unsigned char color){
 uint32_t a=0,total=(uint32_t)pitch*height,bytes=(uint32_t)span_k*1024UL,left;
 unsigned n,b=0,off=0;
 if(driver==4)return plane_clear(color);
 if(!opened)return 0;
 while(a<total){
  left=bytes-off;if(left>total-a)left=total-a;
  /* A nonzero 16-bit count, including an exactly 64K aperture. */
  n=left>32768UL?32768:(unsigned)left;
  if(!setbank(b))return 0;fill_block(off,n,color,segment);
  a+=n;left=(uint32_t)off+n;
  if(left==bytes){++b;off=0;}else off=(unsigned)left;
 }
 return 1;
}
