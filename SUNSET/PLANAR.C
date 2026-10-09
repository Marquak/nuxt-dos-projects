/* Real 4-plane VGA/EGA-layout 16-colour pixels, not an 8-bit fake mode.
 * Semantic 8-bit drawing indices are mapped through a fixed ordered palette.
 * Four plane bytes describe eight pixels. All selected modes fit each plane
 * in its 64 KB aperture (800x600 takes 60,000 bytes per plane). */
#include "SUNSET.H"
#include <dos.h>
#include <conio.h>
#include <string.h>
#include "COLOR16.H"
static int pw,ph,active;
static unsigned stride;
static unsigned short spread[4][256][2];
static unsigned char planes[4][100],mi[256];
static unsigned word(unsigned at){return mi[at]|((unsigned)mi[at+1]<<8);}
const unsigned char *video_palette16(void){return neon_palette16;}
unsigned char plane_map(unsigned char c,unsigned x,unsigned y){return quant16[((y&1)<<1)|(x&1)][c];}
static void configure(void){
 unsigned i;
 outpw(0x3ce,0x0000);outpw(0x3ce,0x0001);outpw(0x3ce,0x0003);
 outpw(0x3ce,0x0005);outpw(0x3ce,0xff08);outpw(0x3c4,0x0f02);
 for(i=0;i<16;i++){inp(0x3da);outp(0x3c0,i);outp(0x3c0,i);}
 inp(0x3da);outp(0x3c0,0x14);outp(0x3c0,0);
 inp(0x3da);outp(0x3c0,0x20);outp(0x3c6,255);
}
int plane_open(int res,int native){
 union REGS r;struct SREGS sr;unsigned mode,phase,c,q,last,start;unsigned long guard=1000000UL;
 volatile unsigned __far *ticks=MK_FP(0x40,0x6c);
 volatile unsigned char __far *v=MK_FP(0xa000,0);
 active=0;pw=res==2?320:res==1?800:640;ph=res==2?200:res==1?600:480;
 stride=pw>>3;mode=res==2?0x0d:res==0?0x12:0x5b;
 if(res==1&&!native){
  memset(&r,0,sizeof(r));memset(mi,0,sizeof(mi));segread(&sr);
  r.w.ax=0x4f01;r.w.cx=0x102;r.w.di=FP_OFF(mi);sr.es=FP_SEG(mi);
  int86x(0x10,&r,&r,&sr);
  if(r.w.ax!=0x004f||(word(0)&0x79)!=0x19||word(18)!=800||word(20)!=600||
     mi[24]!=4||mi[25]!=4||mi[27]!=3||word(16)!=100||word(8)!=0xa000||
     (mi[2]&7)!=7||word(6)<64)return 0;
  memset(&r,0,sizeof(r));r.w.ax=0x4f02;r.w.bx=0x102;int86(0x10,&r,&r);
  if(r.w.ax!=0x004f)return 0;
 }else{
  memset(&r,0,sizeof(r));r.w.ax=mode;int86(0x10,&r,&r);
  r.h.ah=15;int86(0x10,&r,&r);if((r.h.al&127)!=mode)return 0;
 }
 start=*ticks;while((unsigned)(*ticks-start)<2&&--guard){} /* mode settle */
 configure();last=(unsigned)((long)stride*ph-1);
 /* Independent plane isolation and the highest visible address. */
 for(c=0;c<4;c++){outpw(0x3c4,((1U<<c)<<8)|2);v[0]=(unsigned char)(0x35+c*19);v[last]=(unsigned char)(0xab-c*23);}
 for(c=0;c<4;c++){
  outpw(0x3ce,(c<<8)|4);
  if(v[0]!=(unsigned char)(0x35+c*19)||v[last]!=(unsigned char)(0xab-c*23))return 0;
 }
 for(phase=0;phase<4;phase++)for(c=0;c<256;c++){
  q=quant16[phase][c];spread[phase][c][0]=(q&1)|((q&2)<<7);
  spread[phase][c][1]=((q>>2)&1)|((q&8)<<5);
 }
 active=1;return plane_clear(0);
}
void plane_close(void){active=0;}
int plane_width(void){return pw;}
int plane_height(void){return ph;}
void plane_palette(unsigned first,unsigned n,const unsigned char *rgb){
 unsigned i;if(!active||first>=16||n>16-first)return;
 outp(0x3c8,first);for(i=0;i<n*3;i++)outp(0x3c9,rgb[i]&63);
}
static void copy_out(const unsigned char *src,unsigned off,unsigned n);
#pragma aux copy_out = "push es" "mov ax,0a000h" "mov es,ax" "cld" \
 "shr cx,1" "rep movsw" "adc cx,cx" "rep movsb" "pop es" \
 parm [si][di][cx] modify exact[ax si di cx];
static void copy_in(unsigned char *dst,unsigned off,unsigned n);
#pragma aux copy_in = "push ds" "push es" "push ds" "pop es" "mov ax,0a000h" "mov ds,ax" "cld" \
 "shr cx,1" "rep movsw" "adc cx,cx" "rep movsb" "pop es" "pop ds" \
 parm[di][si][cx] modify exact[ax si di cx];
static void fill(unsigned off,unsigned n,unsigned color);
#pragma aux fill = "push es" "mov dx,0a000h" "mov es,dx" "mov ah,al" "cld" \
 "shr cx,1" "rep stosw" "adc cx,cx" "rep stosb" "pop es" \
 parm[di][cx][ax] modify exact[ax dx di cx];
/* Eight semantic pixels become four plane bytes. Lookup tables have only
 * bit0/bit8 set, so eight shifts never leak into an adjacent plane byte.
 * Unrolled V20 word shifts/ORs replace 32 per-pixel bit-test branches. */
static void pack_groups(const unsigned char *src,unsigned char *dst,unsigned groups,const unsigned short *lut);
#pragma aux pack_groups = \
 "push bp" "cld" "pg: push di" "xor bp,bp" "xor dx,dx" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+0]" "or dx,[bx+di+2]" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+1024]" "or dx,[bx+di+1026]" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+0]" "or dx,[bx+di+2]" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+1024]" "or dx,[bx+di+1026]" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+0]" "or dx,[bx+di+2]" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+1024]" "or dx,[bx+di+1026]" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+0]" "or dx,[bx+di+2]" \
 "lodsb" "xor ah,ah" "mov di,ax" "shl di,2" "shl bp,1" "shl dx,1" "or bp,[bx+di+1024]" "or dx,[bx+di+1026]" \
 "pop di" "mov ax,bp" "mov [di],al" "mov [di+100],ah" "mov [di+200],dl" "mov [di+300],dh" \
 "inc di" "dec cx" "jz pgdone" "jmp pg" "pgdone: pop bp" \
 parm[si][di][cx][bx] modify exact[ax cx dx si di];
static void pixel(unsigned x,unsigned y,unsigned char color){
 unsigned p,mask=0x80>>(x&7),c=plane_map(color,x,y);
 volatile unsigned char __far *v=MK_FP(0xa000,y*stride+(x>>3));
 for(p=0;p<4;p++){
  outpw(0x3ce,(p<<8)|4);outpw(0x3c4,((1U<<p)<<8)|2);
  *v=(unsigned char)((*v&~mask)|((c&(1U<<p))?mask:0));
 }
}
static int valid(int x,int y,unsigned n){return active&&x>=0&&x<=pw&&(unsigned)y<(unsigned)ph&&n<=(unsigned)(pw-x);}
int plane_write(int x,int y,const unsigned char *data,unsigned n){
 unsigned groups,off,p;
 if(!valid(x,y,n))return 0;
 while(n&&(x&7)){pixel(x++,y,*data++);--n;}
 groups=n>>3;
 if(groups){
  pack_groups(data,planes[0],groups,&spread[(y&1)<<1][0][0]);off=(unsigned)y*stride+((unsigned)x>>3);
  for(p=0;p<4;p++){outpw(0x3c4,((1U<<p)<<8)|2);copy_out(planes[p],off,groups);}
  x+=groups<<3;data+=groups<<3;n-=groups<<3;
 }
 while(n){pixel(x++,y,*data++);--n;}
 outpw(0x3c4,0x0f02);return 1;
}
int plane_read(int x,int y,unsigned char *data,unsigned n){
 unsigned first,bytes,p,i,at,mask;unsigned char c;
 if(!valid(x,y,n))return 0;if(!n)return 1;
 first=(unsigned)x>>3;bytes=(((unsigned)x+n+7)>>3)-first;
 for(p=0;p<4;p++){outpw(0x3ce,(p<<8)|4);copy_in(planes[p],(unsigned)y*stride+first,bytes);}
 for(i=0;i<n;i++){
  at=((unsigned)x+i)>>3;at-=first;mask=0x80>>(((unsigned)x+i)&7);c=0;
  for(p=0;p<4;p++)if(planes[p][at]&mask)c|=1U<<p;
  data[i]=c;
 }
 outpw(0x3ce,0x0004);return 1;
}
int plane_clear(unsigned char color){
 unsigned y,p,a,b,v;if(!active)return 0;
 for(p=0;p<4;p++){
  outpw(0x3c4,((1U<<p)<<8)|2);
  for(y=0;y<(unsigned)ph;y++){
   a=plane_map(color,0,y);b=plane_map(color,1,y);
   v=((a&(1U<<p))?0xaa:0)|((b&(1U<<p))?0x55:0);fill(y*stride,stride,v);
  }
 }
 outpw(0x3c4,0x0f02);outpw(0x3ce,0x0004);return 1;
}
