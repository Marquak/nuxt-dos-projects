/* ORBIT 87: sparse VGA vectors, 8086 assembly, no 186+ instructions.
 * VGA write mode 3 combines CPU pixel masks with set/reset colour and latches.
 * Thus the inner line loop needs no per-pixel VGA port writes. */
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdint.h>
#include "ORBIT.H"
#include "FONT.H"

typedef struct { unsigned off,bank; } Row;
static Row rows[768];
static unsigned char info[512];
static unsigned pitch,segment=0xa000,span_k=64,bank_factor=1,window;
static int width,height,driver,current_bank=-1,current_color=-1,reference;
static unsigned char trident_flags;
static int buffered; /* 1=TVGA9000i native, 2=VBE display start */
static unsigned draw_page,draw_bank,draw_offset;
static int page_fits_bank;
static uint32_t page_base[2];
static unsigned char trident_module,trident_original_module;
/* Assembly consumes these two per-line constants, never touched by IRQ code. */
static unsigned line_major;
static int line_step;
static uint32_t crc_table[256];
static int crc_ready;

static unsigned word(int at){return info[at]|((unsigned)info[at+1]<<8);}
static void biosmode(unsigned mode){union REGS r;memset(&r,0,sizeof(r));r.w.ax=mode;int86(0x10,&r,&r);}
static int trident(void){unsigned i;const char __far *rom=MK_FP(0xc000,0);for(i=0;i<32760;i++)if(!_fmemcmp(rom+i,"Trident",7)||!_fmemcmp(rom+i,"TRIDENT",7))return 1;return 0;}
static int setbank(unsigned b){
 union REGS r;if(current_bank==(int)b)return 1;
 if(driver==1){memset(&r,0,sizeof(r));r.w.ax=0x4f05;r.w.bx=window;r.w.dx=b*bank_factor;int86(0x10,&r,&r);if(r.w.ax!=0x004f){current_bank=-1;return 0;}}
 else if(driver==2)outpw(0x3c4,((trident_flags|(b^2))<<8)|0x0e);
 current_bank=b;return 1;
}
static unsigned char __far *address(uint32_t a){
 unsigned b;uint32_t bytes=(uint32_t)span_k*1024UL;
 a+=page_base[draw_page];
 if(span_k==64){b=(unsigned)(a>>16);if(!setbank(b))return 0;return MK_FP(segment,(unsigned)a);}
 b=(unsigned)(a/bytes);if(!setbank(b))return 0;return MK_FP(segment,(unsigned)(a-(uint32_t)b*bytes));
}
static int vbe(unsigned mode){
 union REGS r;struct SREGS sr;unsigned attr,gran,size;int win;
 memset(info,0,sizeof(info));memset(&r,0,sizeof(r));segread(&sr);
 r.w.ax=0x4f01;r.w.cx=mode;r.w.di=FP_OFF(info);sr.es=FP_SEG(info);int86x(0x10,&r,&r,&sr);
 if(r.w.ax!=0x004f)return 0;attr=word(0);
 if((attr&0x79)!=0x19||word(18)!=width||word(20)!=height||info[25]!=4||info[27]!=3||info[24]!=4)return 0;
 win=(info[2]&7)==7?0:(info[3]&7)==7?1:-1;if(win<0)return 0;
 gran=word(4);size=word(6);if(!gran||gran>64||!size)return 0;
 span_k=size>64?64:size;if(span_k%gran)return 0;
 pitch=word(16);segment=word(win?10:8);if(!segment||pitch<(unsigned)width/8)return 0;
 memset(&r,0,sizeof(r));r.w.ax=0x4f02;r.w.bx=mode;int86(0x10,&r,&r);if(r.w.ax!=0x004f)return 0;
 driver=1;window=win;bank_factor=span_k/gran;return 1;
}
static int native_mode(int res){
 static const unsigned modes[3]={0x12,0x5b,0x5f};union REGS r;unsigned mode=modes[res];
 if(!trident())return 0;biosmode(mode);memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);if((r.h.al&127)!=mode)return 0;
 driver=2;pitch=width/8;segment=0xa000;span_k=64;bank_factor=1;current_bank=-1;
 outp(0x3c4,0x0b);inp(0x3c5);outp(0x3c4,0x0e);trident_flags=inp(0x3c5)&0xf0;return 1;
}
static void draw_mode(void){
 outpw(0x3c4,0x0f02);outpw(0x3ce,0x0f01);outpw(0x3ce,0x0003);
 outpw(0x3ce,0x0004);outpw(0x3ce,0x0305);outpw(0x3ce,0xff08);current_color=-1;
}
static void configure(void){
 int i;union REGS r;
 /* Force byte addressing where VBE BIOSes leave doubleword defaults behind. */
 outp(0x3d4,0x14);outp(0x3d5,inp(0x3d5)&~0x40);
 outp(0x3d4,0x17);outp(0x3d5,inp(0x3d5)|0x40);
 outpw(0x3c4,0x0604);outpw(0x3ce,0x0506);
 for(i=0;i<16;i++){memset(&r,0,sizeof(r));r.w.ax=0x1000;r.h.bl=i;r.h.bh=i;int86(0x10,&r,&r);}
 outp(0x3c6,255);draw_mode();
}
static void ink(unsigned color){color&=15;if(current_color!=(int)color){outpw(0x3ce,(color<<8)|0);current_color=color;}}
static void mask_pixel(unsigned off,unsigned mask,unsigned seg);
#pragma aux mask_pixel = "push es" "mov es,ax" "mov al,es:[di]" "mov es:[di],bl" "pop es" parm [di] [bx] [ax] modify exact [ax];

static int probe_at(uint32_t a){
 volatile unsigned char __far *v;unsigned c,p,want;
 v=address(a);if(!v)return 0;
 for(c=0;c<16;c++){
  /* A complement background proves the mode preserves unselected pixels. */
  outpw(0x3ce,0x0001);outpw(0x3ce,0x0005);v[0]=0x5a;v[1]=0xa5;
  draw_mode();ink(c);mask_pixel(FP_OFF(v),0xa5,segment);mask_pixel(FP_OFF(v)+1,0x5a,segment);
  for(p=0;p<4;p++){outpw(0x3ce,(p<<8)|4);want=(c&(1U<<p))?255:0x5a;if(v[0]!=want||v[1]!=((c&(1U<<p))?255:0xa5)){draw_mode();return 0;}}
 }
 draw_mode();return 1;
}
static int probe(void){uint32_t total=(uint32_t)pitch*height,edge=(uint32_t)span_k*1024UL;return probe_at(256)&&(total<=edge||probe_at(edge+256));}

int video_open(int res){
 unsigned y;uint32_t a,bytes;static const unsigned modes[3]={0x12,0x102,0x104};
 if(res<0||res>2){video_close();return 0;}
 width=res==0?640:res==1?800:1024;height=res==0?480:res==1?600:768;
 pitch=width/8;segment=0xa000;span_k=64;bank_factor=1;driver=0;current_bank=-1;reference=0;
 buffered=0;draw_page=draw_bank=draw_offset=0;page_base[0]=page_base[1]=0;
 biosmode(0x12);
 if(res&&!vbe(modes[res])&&!native_mode(res)){video_close();return 0;}
 configure();
 if(!probe()){
  if(driver!=1||!native_mode(res)){video_close();return 0;}
  configure();if(!probe()){video_close();return 0;}
 }
 bytes=(uint32_t)span_k*1024UL;
 page_fits_bank=(uint32_t)pitch*height<=bytes;
 for(y=0;y<(unsigned)height;y++){a=(uint32_t)y*pitch;rows[y].bank=(unsigned)(a/bytes);rows[y].off=(unsigned)(a%bytes);}
 video_clear();return 1;
}
void video_close(void){
 if(buffered==1){setbank(0);outpw(0x3d4,(trident_original_module<<8)|0x1e);}
 biosmode(3);current_bank=current_color=-1;buffered=page_fits_bank=0;draw_page=draw_bank=draw_offset=0;page_base[0]=page_base[1]=0;
}
int video_width(void){return width;}
int video_height(void){return height;}
const char *video_driver(void){return driver==1?"VBE planar":driver==2?"Trident native":"VGA mode 12h";}
const char *video_buffer_name(void){return buffered==1?"Trident page flip":buffered==2?"VBE page flip":"single page";}

/* Keep the music IRQ enabled while waiting. The finite timeout also makes
 * broken or absent retrace status harmless. No video timing is changed. */
static int retrace(void){
 unsigned n=65535;
 while((inp(0x3da)&8)&&--n){}
 n=65535;while(!(inp(0x3da)&8)&&--n){}
 return n!=0;
}
static unsigned video_lock(void);
#pragma aux video_lock = "pushf" "pop ax" "cli" value [ax];
static void video_unlock(unsigned flags);
#pragma aux video_unlock = "push ax" "popf" parm [ax];
static int display_start(unsigned y){
 union REGS r;memset(&r,0,sizeof(r));r.w.ax=0x4f07;r.w.dx=y;
 int86(0x10,&r,&r);return r.w.ax==0x004f;
}
void video_select_page(unsigned page){
 uint32_t bytes=(uint32_t)span_k*1024UL;
 draw_page=buffered?(page&1):0;
 draw_bank=(unsigned)(page_base[draw_page]/bytes);
 draw_offset=(unsigned)(page_base[draw_page]%bytes);
 page_fits_bank=(uint32_t)draw_offset+(uint32_t)pitch*height<=bytes;
}
void video_show_page(unsigned page){
 unsigned flags,tries;int seen;
 if(!buffered)return;page&=1;
 if(buffered==1){
  /* Byte addressing: start bits15..0 stay zero; CRTC1E supplies bit16.
   * Preserve interlace and other mode bits. Pages are exactly64KB apart. */
  for(tries=0;tries<3;tries++){
   seen=retrace();flags=video_lock();
   /* An AdLib IRQ may have consumed the blanking interval. Recheck with
    * IRQs masked only around this tiny register update, never the wait. */
   if(!seen||(inp(0x3da)&8)||tries==2){
    outpw(0x3d4,((trident_module|(page?0x20:0))<<8)|0x1e);
    video_unlock(flags);break;
   }
   video_unlock(flags);
  }
 }else{retrace();display_start(page?(unsigned)height:0);}
}

/* Some BIOSes claim banking support but alias the same RAM. Prove page
 * independence with different patterns in all four planes, then restore it.
 * This probes CPU addressing; native Trident scanout still needs real hardware. */
static int independent_pages(uint32_t second){
 unsigned p;int ok=1;unsigned char saved[2][4];
 volatile unsigned char __far *v;
 for(p=0;p<4;p++){
  outpw(0x3ce,(p<<8)|4);v=address(256);if(!v){setbank(0);draw_mode();return 0;}saved[0][p]=*v;
  v=address(second+256);if(!v){setbank(0);draw_mode();return 0;}saved[1][p]=*v;
 }
 outpw(0x3ce,0x0001);outpw(0x3ce,0x0005);outpw(0x3ce,0xff08);
 for(p=0;p<4;p++){
  outpw(0x3c4,((1U<<p)<<8)|2);
  v=address(256);if(!v){ok=0;break;}*v=(unsigned char)(0x35+p*19);
  v=address(second+256);if(!v){ok=0;break;}*v=(unsigned char)(0xca-p*17);
 }
 for(p=0;p<4;p++){
  outpw(0x3ce,(p<<8)|4);
  v=address(256);if(!v||*v!=(unsigned char)(0x35+p*19))ok=0;
  v=address(second+256);if(!v||*v!=(unsigned char)(0xca-p*17))ok=0;
 }
 for(p=0;p<4;p++){
  outpw(0x3c4,((1U<<p)<<8)|2);
  v=address(256);if(v)*v=saved[0][p];
  v=address(second+256);if(v)*v=saved[1][p];
 }
 setbank(0);draw_mode();return ok;
}
int video_buffering(int enabled){
 union REGS r;unsigned id;unsigned char old_module;
 int old_driver=driver;
 if(buffered||!enabled)return buffered!=0;
 /* Restrict direct extended-register access to the user's9000i (ID43h).
  * Other cards can use their VBE BIOS, or the original single-page path. */
 if(height<=600&&(uint32_t)pitch*height<=65536UL&&trident()){
  outp(0x3c4,0x0b);id=inp(0x3c5);
  if(id==0x43){
   outp(0x3c4,0x0e);trident_flags=inp(0x3c5)&0xf0;
   outp(0x3d4,0x1e);old_module=inp(0x3d5);
   trident_module=(old_module|0x80)&~0x20;
   outpw(0x3d4,(trident_module<<8)|0x1e);
   driver=2;current_bank=-1;
   if(span_k==64&&segment==0xa000&&independent_pages(65536UL)){
    trident_original_module=old_module;buffered=1;page_base[1]=65536UL;return 1;
   }
   setbank(0);outpw(0x3d4,(old_module<<8)|0x1e);driver=old_driver;current_bank=-1;
  }
 }
 /* VGA mode12 BIOSes generally do not expose VBE display-start services.
  * Never infer support from4F05 alone: DOSBox correctly aliases its VGA RAM. */
 memset(&r,0,sizeof(r));r.w.ax=0x4f06;r.h.bl=1;int86(0x10,&r,&r);
 if(r.w.ax!=0x004f||r.w.bx!=pitch||r.w.dx<(unsigned)height*2)return 0;
 if(driver!=1)return 0;
 if(!independent_pages((uint32_t)pitch*height))return 0;
 if(!display_start((unsigned)height))return display_start(0)?0:-1;
 if(!display_start(0))return -1;
 page_base[1]=(uint32_t)pitch*height;buffered=2;return 1;
}
void video_reference(int enabled){reference=enabled!=0;}
void video_palette(const unsigned char *rgb){unsigned i;outp(0x3c8,0);for(i=0;i<48;i++)outp(0x3c9,rgb[i]&63);}
void video_clear(void){
 uint32_t a=0,total=(uint32_t)pitch*height,bytes=(uint32_t)span_k*1024UL,left;unsigned n;unsigned char __far *v;
 outpw(0x3c4,0x0f02);outpw(0x3ce,0x0001);outpw(0x3ce,0x0005);outpw(0x3ce,0xff08);
 while(a<total){left=bytes-((a+page_base[draw_page])%bytes);if(left>total-a)left=total-a;n=left>16384UL?16384:(unsigned)left;v=address(a);if(!v)break;_fmemset(v,0,n);a+=n;}
 draw_mode();
}
static void xy_address(unsigned x,unsigned y,unsigned *off,unsigned *bank){
 unsigned old=rows[y].off;*off=old+(x>>3);*bank=rows[y].bank+draw_bank;
 if(span_k==64){if(*off<old)++*bank;}else if(*off>=span_k*1024U){*off-=span_k*1024U;++*bank;}
 if(draw_offset){old=*off;*off+=draw_offset;if(span_k==64){if(*off<old)++*bank;}else if(*off<old||*off>=span_k*1024U){*off-=span_k*1024U;++*bank;}}
}
void video_pixel(int x,int y,unsigned color){
 unsigned off,b;if((unsigned)x>=(unsigned)width||(unsigned)y>=(unsigned)height)return;
 xy_address(x,y,&off,&b);if(!setbank(b))return;ink(color);mask_pixel(off,128U>>(x&7),segment);
}
/* Signed endpoint differences can reach 65535. Multiply magnitudes as
 * unsigned 32-bit values (65535 squared still fits), then restore the sign.
 * Intersection distances lie on the segment, so the quotient is <=65535. */
static long interpolate(long delta,long distance,long denominator){
 unsigned long a=delta<0?-delta:delta,b=distance<0?-distance:distance;
 unsigned long c=denominator<0?-denominator:denominator;
 long q=(long)((a*b)/c);
 return ((delta<0)^(distance<0)^(denominator<0))?-q:q;
}
/* Cohen-Sutherland, with every subtraction widened before evaluation. */
static int region(int x,int y){return (x<0?1:x>=width?2:0)|(y<0?4:y>=height?8:0);}
static int clip(int *x0,int *y0,int *x1,int *y1){
 int a=region(*x0,*y0),b=region(*x1,*y1),c,x,y;
 while(a|b){if(a&b)return 0;c=a?a:b;
  if(c&4){y=0;x=(int)((long)*x0+interpolate((long)*x1-*x0,0L-*y0,(long)*y1-*y0));}
  else if(c&8){y=height-1;x=(int)((long)*x0+interpolate((long)*x1-*x0,(long)y-*y0,(long)*y1-*y0));}
  else if(c&2){x=width-1;y=(int)((long)*y0+interpolate((long)*y1-*y0,(long)x-*x0,(long)*x1-*x0));}
  else {x=0;y=(int)((long)*y0+interpolate((long)*y1-*y0,0L-*x0,(long)*x1-*x0));}
  if(c==a){*x0=x;*y0=y;a=region(x,y);}else{*x1=x;*y1=y;b=region(x,y);}
 }
 return 1;
}
/* Parameters: start byte, bit mask, minor delta, pixel count, initial error,
 * VRAM segment. BP and ES explicitly saved; all code remains 8086-compatible. */
static void line_x(unsigned off,unsigned mask,unsigned minor,unsigned count,unsigned error,unsigned seg);
#pragma aux line_x = \
 "push es" "mov es,ax" "push bp" "mov bp,line_major" \
 "lx: mov al,es:[di]" "mov es:[di],bl" \
 "ror bl,1" "jnc lxbyte" "inc di" \
 "lxbyte: sub si,dx" "jns lxnext" "add si,bp" "add di,line_step" \
 "lxnext: loop lx" "pop bp" "pop es" \
 parm [di] [bx] [dx] [cx] [si] [ax] modify exact [ax bx cx si di];
/* Horizontal spans write full bytes without reading VGA latches: mask FF
 * replaces every bit in all enabled planes. Partial edge bytes still latch. */
static void line_h(unsigned off,unsigned mask,unsigned count,unsigned seg);
#pragma aux line_h = \
 "push es" "mov es,ax" "cmp bl,128" "je lhfull" "xor bh,bh" \
 "lhprefix: or bh,bl" "ror bl,1" "jc lhfirst" "loop lhprefix" \
 "mov al,es:[di]" "mov es:[di],bh" "jmp lhdone" \
 "lhfirst: mov al,es:[di]" "mov es:[di],bh" "inc di" "dec cx" "jz lhdone" \
 "lhfull: mov bx,cx" "and bx,7" "shr cx,1" "shr cx,1" "shr cx,1" \
 "mov al,255" "rep stosb" "mov cx,bx" "jcxz lhdone" \
 "neg cl" "add cl,8" "shl al,cl" "mov bl,al" "mov al,es:[di]" "mov es:[di],bl" \
 "lhdone: pop es" \
 parm [di] [bx] [cx] [ax] modify exact [ax bx cx di];
/* Aggregate pixels that share a VGA byte. Each flush is one latch read and
 * one mask write, regardless of the number of pixels gathered. */
static void line_x_span(unsigned off,unsigned mask,unsigned minor,unsigned count,unsigned error,unsigned seg);
#pragma aux line_x_span = \
 "push es" "mov es,ax" "push bp" "mov bp,line_major" "xor bh,bh" \
 "ls: or bh,bl" "ror bl,1" "jc lsbyte" \
 "sub si,dx" "js lsrow" "loop ls" "jmp lsfinal" \
 "lsbyte: mov al,es:[di]" "mov es:[di],bh" "xor bh,bh" "inc di" \
 "sub si,dx" "jns lsnext" "add si,bp" "add di,line_step" \
 "lsnext: loop ls" "jmp lsdone" \
 "lsrow: mov al,es:[di]" "mov es:[di],bh" "xor bh,bh" "add si,bp" "add di,line_step" \
 "loop ls" "jmp lsdone" \
 "lsfinal: mov al,es:[di]" "mov es:[di],bh" \
 "lsdone: pop bp" "pop es" \
 parm [di] [bx] [dx] [cx] [si] [ax] modify exact [ax bx cx si di];
static void line_y(unsigned off,unsigned mask,unsigned minor,unsigned count,unsigned error,unsigned seg);
#pragma aux line_y = \
 "push es" "mov es,ax" "push bp" "mov bp,line_major" \
 "ly: mov al,es:[di]" "mov es:[di],bl" "add di,line_step" \
 "sub si,dx" "jns lynext" "add si,bp" "ror bl,1" "jnc lynext" "inc di" \
 "lynext: loop ly" "pop bp" "pop es" \
 parm [di] [bx] [dx] [cx] [si] [ax] modify exact [ax bx cx si di];
static void line_c(int x,int y,int dx,int dy,int sy,unsigned color){
 int err,count;unsigned off,b,mask;volatile unsigned char __far *v;unsigned char latch;
 count=(dx>=dy?dx:dy)+1;err=(dx>=dy?dx:dy)/2;
 while(count--){
  xy_address(x,y,&off,&b);if(!setbank(b))return;
  v=MK_FP(segment,off);mask=128U>>(x&7);latch=*v;*v=(unsigned char)mask;
  if(dx>=dy){++x;err-=dy;if(err<0){y+=sy;err+=dx;}}
  else {y+=sy;err-=dx;if(err<0){++x;err+=dy;}}
 }
 (void)latch;(void)color;
}
/* On planar modes whose banks end on scanline boundaries, derive the exact
 * Bresenham state at each bank edge. Only the bank change goes through C;
 * every pixel remains in the assembly loop. Other VBE layouts use line_c. */
static void line_banks(int x,int y,int dx,int dy,int sy){
 unsigned remaining=line_major+1,err=line_major/2,off,b,n,ysteps,minor,advance;
 unsigned rows_per_bank=(unsigned)(((uint32_t)span_k*1024UL)/pitch);
 uint32_t numerator;
 minor=dx>=dy?dy:dx;
 while(remaining){
  ysteps=sy>0?rows_per_bank-(unsigned)y%rows_per_bank:(unsigned)y%rows_per_bank+1;
  if(dx>=dy){numerator=(uint32_t)ysteps*line_major-(line_major-1-err);n=(unsigned)((numerator+dy-1)/dy);}
  else n=ysteps;
  if(n>remaining)n=remaining;
  xy_address(x,y,&off,&b);if(!setbank(b))return;
  if(dx>=dy){if(!dy)line_h(off,128U>>(x&7),n,segment);else if(dy<=((unsigned)dx>>2))line_x_span(off,128U>>(x&7),dy,n,err,segment);else line_x(off,128U>>(x&7),dy,n,err,segment);}
  else line_y(off,128U>>(x&7),dx,n,err,segment);
  remaining-=n;if(!remaining)break;
  numerator=(uint32_t)n*minor;
  advance=(unsigned)((numerator+line_major-1-err)/line_major);
  err=(unsigned)((uint32_t)err+(uint32_t)advance*line_major-numerator);
  if(dx>=dy){x+=n;y+=sy*(int)advance;}else{x+=advance;y+=sy*(int)n;}
 }
}
void video_line(int x0,int y0,int x1,int y1,unsigned color){
 int t,dx,dy,sy,ymin,ymax;unsigned off,b,lo,hi,blo,bhi;
 /* Most scene endpoints are already bounded by add_line(). Keep arbitrary
  * callers safe, without building four outcodes for every visible line. */
 if((unsigned)x0>=(unsigned)width||(unsigned)x1>=(unsigned)width||
    (unsigned)y0>=(unsigned)height||(unsigned)y1>=(unsigned)height)
  if(!clip(&x0,&y0,&x1,&y1))return;
 if(x1<x0){t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;}
 dx=x1-x0;dy=y1-y0;sy=1;if(dy<0){dy=-dy;sy=-1;}
 ink(color);
 if(reference){line_c(x0,y0,dx,dy,sy,color);return;}
 line_step=sy>0?(int)pitch:-(int)pitch;line_major=dx>=dy?dx:dy;
 if(page_fits_bank){
  /* Standard VGA and each native Trident 640/800 page fit one aperture.
   * Page selection proved this once; no per-line bounding-bank work. */
  off=rows[y0].off+draw_offset+((unsigned)x0>>3);b=draw_bank;
 }else{
  /* The bounding rectangle, rather than only endpoints, must fit one bank. */
  ymin=y0<y1?y0:y1;ymax=y0>y1?y0:y1;
  xy_address(x0,ymin,&lo,&blo);xy_address(x1,ymax,&hi,&bhi);
  if(blo!=bhi){
   if(!draw_offset&&((uint32_t)span_k*1024UL)%pitch==0)line_banks(x0,y0,dx,dy,sy);
   else line_c(x0,y0,dx,dy,sy,color);
   return;
  }
  xy_address(x0,y0,&off,&b);
 }
 if(!setbank(b))return;
 if(dx>=dy){if(!dy)line_h(off,128U>>(x0&7),dx+1,segment);else if(dy<=((unsigned)dx>>2))line_x_span(off,128U>>(x0&7),dy,dx+1,dx/2,segment);else line_x(off,128U>>(x0&7),dy,dx+1,dx/2,segment);}
 else line_y(off,128U>>(x0&7),dx,dy+1,dy/2,segment);
}
/* Text owns its character cells: a black background prevents stale HUD digits.
 * Eight (or 16/24/32) pixels are submitted as byte masks, not individual C pixels. */
static void row_masks(int x,int y,const unsigned char *data,unsigned count){
 unsigned off,b,i,shift=(unsigned)x&7,mask,previous=0;unsigned char value;
 xy_address(x,y,&off,&b);if(!setbank(b))return;
 for(i=0;i<count+(shift!=0);i++){
  value=i<count?data[i]:0;mask=(value>>shift)|previous;previous=shift?(unsigned)value<<(8-shift):0;
  if(mask&255)mask_pixel(off,mask,segment);
  ++off;
  if(span_k==64?!off:off==span_k*1024U){off=0;if(!setbank(++b))return;}
 }
}
static void black_rect(int x,int y,int w,int h){
 int right=x+w-1,bottom=y+h-1,yy;unsigned off,b,n,mask,available;int leftbyte,rightbyte;
 if(x<0)x=0;if(y<0)y=0;if(right>=width)right=width-1;if(bottom>=height)bottom=height-1;
 if(x>right||y>bottom)return;ink(0);leftbyte=x>>3;rightbyte=right>>3;
 for(yy=y;yy<=bottom;yy++){
  xy_address(x,yy,&off,&b);if(!setbank(b))return;
  available=span_k==64?0xffffU-off:span_k*1024U-1-off;
  if((unsigned)(rightbyte-leftbyte)<=available){line_h(off,128U>>(x&7),right-x+1,segment);continue;}
  for(n=0;n<=(unsigned)(rightbyte-leftbyte);n++){
   mask=255;if(!n)mask&=255U>>(x&7);if(n==(unsigned)(rightbyte-leftbyte))mask&=255U<<(7-(right&7));
   mask_pixel(off,mask,segment);++off;
   if(span_k==64?!off:off==span_k*1024U){off=0;if(!setbank(++b))return;}
  }
 }
}
void video_text(int x,int y,unsigned color,const char *s,int scale){
 unsigned ch,row,bit,j,k,n;int xx,yy,base=x;unsigned char expanded[4];const char *end;
 if(scale<1)scale=1;if(scale>4)scale=4;
 while(*s){
  end=s;while(*end&&*end!='\n')++end;n=(unsigned)(end-s);
  black_rect(base,y,n*8*scale,8*scale);ink(color);x=base;
  while(s<end){ch=(unsigned char)*s++&127;
   for(row=0;row<8;row++){
    memset(expanded,0,sizeof(expanded));
    for(bit=0;bit<8;bit++)if(font[ch][row]&(128U>>bit))for(j=0;j<(unsigned)scale;j++){k=bit*scale+j;expanded[k>>3]|=128U>>(k&7);}
    for(yy=0;yy<scale;yy++){
     if(x>=0&&x+8*scale<=width&&y+(int)row*scale+yy>=0&&y+(int)row*scale+yy<height)row_masks(x,y+row*scale+yy,expanded,scale);
     else for(bit=0;bit<(unsigned)(8*scale);bit++)if(expanded[bit>>3]&(128U>>(bit&7))){xx=x+bit;video_pixel(xx,y+row*scale+yy,color);}
    }
   }
   x+=8*scale;
  }
  if(*s=='\n'){++s;y+=9*scale;}
 }
}
unsigned long video_checksum(void){
 uint32_t a,total=(uint32_t)pitch*height,left,bytes=(uint32_t)span_k*1024UL,crc=0xffffffffUL,value;
 unsigned p,i,j,n;unsigned char __far *v;
 if(!crc_ready){for(i=0;i<256;i++){value=i;for(j=0;j<8;j++)value=(value>>1)^((value&1)?0xedb88320UL:0);crc_table[i]=value;}crc_ready=1;}
 for(p=0;p<4;p++){
  outpw(0x3ce,(p<<8)|4);
  for(a=0;a<total;a+=n){left=bytes-((a+page_base[draw_page])%bytes);if(left>total-a)left=total-a;n=left>16384UL?16384:(unsigned)left;v=address(a);if(!v){draw_mode();return 0;}
   for(i=0;i<n;i++)crc=(crc>>8)^crc_table[(crc^v[i])&255];
  }
 }
 /* Leave the renderer's register cache consistent after diagnostic readback. */
 draw_mode();return crc^0xffffffffUL;
}
