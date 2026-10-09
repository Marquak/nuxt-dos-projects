/* Matrix rain for DOS/8086. Standard VGA, banked VBE, and TVGA9000. */
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define MAXCOL 128
#define MAXROW 64
#include "FONT.H"
#include "FPU.H"
#define MAXCELLS (MAXCOL*MAXROW)
#define MAXDIRTY 1024
/* Desired cell state doubles as the rain's stored characters. */
typedef struct { unsigned char ch,color; } Cell;
typedef struct { unsigned offset,bank; } Scanline;
static Cell cells[MAXCELLS];
static unsigned char info[512],dirty_bits[MAXCELLS/8];
static unsigned dirty_ids[MAXDIRTY],dirty_next[MAXDIRTY],dirty_count;
static unsigned buckets[16][MAXROW],colour_counts[16];
static Scanline scan[MAXROW][8];
static unsigned char row_safe[MAXROW],expand[256][8];
static unsigned packed_ink,bank_factor,window_bytes;
static unsigned char glow[MATRIX_GLOW_STEPS];
static int have_fpu,glow_enabled=1;
static int head[MAXCOL],length[MAXCOL],period[MAXCOL],countdown[MAXCOL];
static int width,height,depth,cols,rows,driver,bank=-1,window;
static unsigned pitch,segment=0xa000,gran=64,span_k=64;
static unsigned char trident_flags;
static uint32_t rng=1;
static volatile int running=1;
static unsigned ticks(void){return *(volatile unsigned __far *)MK_FP(0x40,0x6c);}
static unsigned rnd(unsigned n){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (unsigned)(rng>>16)%n;}
static void idle(void);
#pragma aux idle = "sti" "hlt";
static void __interrupt __far stop(void){running=0;}
static unsigned word(int at){return info[at]|((unsigned)info[at+1]<<8);}
static void biosmode(unsigned mode){union REGS r;memset(&r,0,sizeof(r));r.w.ax=mode;int86(0x10,&r,&r);}
static int trident(void){unsigned i;const char __far *rom=MK_FP(0xc000,0);for(i=0;i<32760;i++)if(!_fmemcmp(rom+i,"Trident",7)||!_fmemcmp(rom+i,"TRIDENT",7))return 1;return 0;}
static void setbank(unsigned b){
 union REGS r;if(bank==(int)b)return;bank=b;
 if(driver==1){memset(&r,0,sizeof(r));r.w.ax=0x4f05;r.w.bx=window;r.w.dx=b*bank_factor;int86(0x10,&r,&r);if(r.w.ax!=0x004f)running=0;}
 else if(driver==2)outpw(0x3c4,((trident_flags|(b^2))<<8)|0x0e);
}
static unsigned char __far *address(uint32_t a){
 unsigned b;if(!driver)return MK_FP(0xa000,(unsigned)a);
 if(span_k==64){setbank((unsigned)(a>>16));return MK_FP(segment,(unsigned)a);}
 b=(unsigned)(a/((uint32_t)span_k*1024));setbank(b);
 return MK_FP(segment,(unsigned)(a-(uint32_t)b*span_k*1024));
}
static int vbe(unsigned mode){
 union REGS r;struct SREGS sr;unsigned attr,size;int win;
 memset(info,0,sizeof(info));memset(&r,0,sizeof(r));segread(&sr);
 r.w.ax=0x4f01;r.w.cx=mode;r.w.di=FP_OFF(info);sr.es=FP_SEG(info);int86x(0x10,&r,&r,&sr);
 if(r.w.ax!=0x004f)return 0;attr=word(0);
 if((attr&0x79)!=0x19||word(18)!=width||word(20)!=height||info[25]!=(depth==16?4:8)||info[27]!=(depth==16?3:4))return 0;
 if(depth==16&&info[24]!=4)return 0;
 size=depth==16?7:5;
 win=(info[2]&size)==size?0:(info[3]&size)==size?1:-1;if(win<0)return 0;
 gran=word(4);size=word(6);if(!gran||gran>64||!size)return 0;
 span_k=size>64?64:size;if(span_k%gran)return 0;
 pitch=word(16);segment=word(win?10:8);if(!segment||pitch<(unsigned)(depth==16?width/8:width))return 0;
 memset(&r,0,sizeof(r));r.w.ax=0x4f02;r.w.bx=mode;int86(0x10,&r,&r);if(r.w.ax!=0x004f)return 0;
 driver=1;window=win;bank_factor=span_k/gran;window_bytes=span_k*1024U;return 1;
}
static int startmode(int res,int color){
 static const unsigned vmode[3][2]={{0x12,0x101},{0x102,0x103},{0x104,0x105}};
 static const unsigned tmode[3][2]={{0x12,0x5d},{0x5b,0x5e},{0x5f,0}};
 union REGS r;unsigned mode;
 width=res==0?640:res==1?800:1024;height=res==0?480:res==1?600:768;depth=color?256:16;
 cols=width/8;rows=height/12;pitch=depth==16?width/8:width;segment=0xa000;span_k=gran=64;driver=0;bank=-1;bank_factor=1;window_bytes=0;
 if(!res&&!color){biosmode(0x12);return 1;}
 /* Some VBE ROMs leave packed-mode CRTC state behind in planar modes. */
 if(depth==16)biosmode(0x12);
 if(vbe(vmode[res][color]))return 1;
 /* The NuXT's 512 KB TVGA9000i ROM provides these native modes. */
 if(!trident()||!(mode=tmode[res][color]))return 0;
 biosmode(mode);memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);if((r.h.al&127)!=mode)return 0;
 driver=2;pitch=depth==16?width/8:width;span_k=gran=64;segment=0xa000;
 outp(0x3c4,0x0b);inp(0x3c5);outp(0x3c4,0x0e);trident_flags=inp(0x3c5)&0xf0;
 return 1;
}
static void palette(void){
 int i,k,r,g,b;union REGS reg;
 /* Map planar pixel indices directly to DAC slots. */
 if(depth==16)for(i=0;i<16;i++){memset(&reg,0,sizeof(reg));reg.w.ax=0x1000;reg.h.bl=i;reg.h.bh=i;int86(0x10,&reg,&reg);}
 outp(0x3c6,255);outp(0x3c8,0);
 for(i=0;i<256;i++){
  k=depth==16?i:i/16;r=b=0;g=k?3+k*4:0;if(g>63)g=63;
  if(k>=14){r=b=k==15?47:19;g=63;}
  outp(0x3c9,r);outp(0x3c9,g);outp(0x3c9,b);
 }
 if(depth==16){
  /* Planar byte addressing, including VBE ROMs using doubleword defaults. */
  outp(0x3d4,0x14);outp(0x3d5,inp(0x3d5)&~0x40);
  outp(0x3d4,0x17);outp(0x3d5,inp(0x3d5)|0x40);
  outpw(0x3c4,0x0604);outpw(0x3ce,0x0506);outpw(0x3ce,0x0000);outpw(0x3ce,0x0001);outpw(0x3ce,0x0003);outpw(0x3ce,0x0005);outpw(0x3ce,0xff08);outpw(0x3c4,0x0f02);}
}
/* The hot blitters contain only 8086 instructions. SI is a near font/LUT
 * pointer; video ES is restored. No per-pixel far-pointer construction. */
static void blit16(const unsigned char *src,unsigned off,unsigned step,unsigned seg);
#pragma aux blit16 = \
 "push es" "mov es,ax" \
 "mov al,[si]"   "mov es:[di],al" "add di,bx" \
 "mov al,[si+1]" "mov es:[di],al" "add di,bx" \
 "mov al,[si+2]" "mov es:[di],al" "add di,bx" \
 "mov al,[si+3]" "mov es:[di],al" "add di,bx" \
 "mov al,[si+4]" "mov es:[di],al" "add di,bx" \
 "mov al,[si+5]" "mov es:[di],al" "add di,bx" \
 "mov al,[si+6]" "mov es:[di],al" "add di,bx" \
 "mov al,[si+7]" "mov es:[di],al" "pop es" \
 parm [si] [di] [bx] [ax] modify exact [ax di];

static void blit256(const unsigned char *src,unsigned off,unsigned step,unsigned seg,const unsigned char *lut);
#pragma aux blit256 = \
 "push es" "mov es,ax" "push bp" "mov bp,8" "mov cx,packed_ink" \
 "rowloop: lodsb" "xor ah,ah" "shl ax,1" "shl ax,1" "shl ax,1" \
 "add ax,bx" "push si" "mov si,ax" \
 "mov ax,[si]" "and ax,cx" "mov es:[di],ax" \
 "mov ax,[si+2]" "and ax,cx" "mov es:[di+2],ax" \
 "mov ax,[si+4]" "and ax,cx" "mov es:[di+4],ax" \
 "mov ax,[si+6]" "and ax,cx" "mov es:[di+6],ax" \
 "pop si" "add di,dx" "dec bp" "jnz rowloop" "pop bp" "pop es" \
 parm [si] [di] [dx] [ax] [bx] modify exact [ax cx si di];

static void renderer_init(void){
 unsigned y,j,i,k;uint32_t a,bytes=(uint32_t)span_k*1024;
 memset(cells,0,sizeof(cells));memset(dirty_bits,0,sizeof(dirty_bits));dirty_count=0;
 bank_factor=span_k/gran;window_bytes=span_k*1024U;
 for(y=0;y<(unsigned)rows;y++){
  a=(uint32_t)(y*12+2)*pitch;
  for(j=0;j<8;j++,a+=pitch){
   scan[y][j].bank=(unsigned)(a/bytes);scan[y][j].offset=(unsigned)(a%bytes);
  }
  row_safe[y]=scan[y][0].bank==scan[y][7].bank &&
   (uint32_t)scan[y][7].offset+(depth==16?(unsigned)cols:(unsigned)width)<=bytes;
 }
 if(depth==256)for(i=0;i<256;i++)for(k=0;k<8;k++)expand[i][k]=(i&(128U>>k))?255:0;
}

/* A slow edge path is needed only for glyphs straddling a bank boundary.
 * The common path uses the precomputed row mapping and one unrolled blit. */
static void edge_glyph(unsigned x,unsigned y,const unsigned char *f,unsigned color){
 unsigned j,k,b,off,add=depth==16?x:x*8,old;unsigned char __far *v;
 for(j=0;j<8;j++){
  b=scan[y][j].bank;old=scan[y][j].offset;off=old+add;
  if(span_k==64){if(off<old)b++;}
  else if(off>=window_bytes){off-=window_bytes;b++;}
  if(driver)setbank(b);v=MK_FP(segment,off);
  if(depth==16)*v=f[j];
  else for(k=0;k<8;k++){
   if(k && (span_k==64?!off:off==window_bytes)){
    off=0;if(driver)setbank(++b);v=MK_FP(segment,0);
   }
   *v++=(f[j]&(128U>>k))?(unsigned char)color:0;off++;
  }
 }
}

static void flush_draw(void){
 unsigned i,id,y,x,c,n,pixel;const unsigned char *f;
 if(!dirty_count)return;
 memset(buckets,0xff,sizeof(buckets));memset(colour_counts,0,sizeof(colour_counts));
 for(i=0;i<dirty_count;i++){
  id=dirty_ids[i];y=id>>7;c=cells[id].color;
  dirty_next[i]=buckets[c][y];buckets[c][y]=i;colour_counts[c]++;
 }
 /* Sorting by shade and row bounds both VGA port writes and bank changes. */
 for(c=0;c<16;c++)if(colour_counts[c]){
  pixel=c?c*16+15:0;packed_ink=pixel|(pixel<<8);
  if(depth==16)outpw(0x3ce,(((~c)&15)<<8)|1);
  for(y=0;y<(unsigned)rows;y++){
   n=buckets[c][y];if(n==0xffffU)continue;
   if(row_safe[y]&&driver)setbank(scan[y][0].bank);
   while(n!=0xffffU){
    id=dirty_ids[n];x=id&127;f=font[cells[id].ch];
    if(row_safe[y]){
     if(depth==16)blit16(f,scan[y][0].offset+x,pitch,segment);
     else blit256(f,scan[y][0].offset+x*8,pitch,segment,&expand[0][0]);
    }else edge_glyph(x,y,f,pixel);
    n=dirty_next[n];
   }
  }
 }
 memset(dirty_bits,0,sizeof(dirty_bits));dirty_count=0;
}

static void draw(int x,int y,unsigned ch,unsigned color){
 unsigned id,bit;
 if(x<0||x>=cols||y<0||y>=rows)return;
 id=((unsigned)y<<7)+(unsigned)x;color&=15;ch&=127;
 if(cells[id].ch==ch&&cells[id].color==color)return;
 bit=1U<<(id&7);
 if(!(dirty_bits[id>>3]&bit)){
  if(dirty_count==MAXDIRTY)flush_draw();
  dirty_bits[id>>3]|=bit;dirty_ids[dirty_count++]=id;
 }
 cells[id].ch=(unsigned char)ch;cells[id].color=(unsigned char)color;
}

static int planar_probe(void){
 volatile unsigned char __far *v=address(256);unsigned c,p,want;
 /* Set/reset forces background planes to zero; other planes take host data.
  * Test both adjacent bytes in all sixteen colours, without plane switching
  * on writes. This also rejects unusable advertised VBE planar modes. */
 outpw(0x3c4,0x0f02);outpw(0x3ce,0x0000);
 for(c=0;c<16;c++){
  outpw(0x3ce,(((~c)&15)<<8)|1);v[0]=0xa5;v[1]=0x5a;
  for(p=0;p<4;p++){
   outpw(0x3ce,(p<<8)|4);want=(c&(1U<<p))?0xa5:0;
   if(v[0]!=want||v[1]!=(want?0x5a:0)){outpw(0x3ce,0x0004);outpw(0x3ce,0x0001);return 0;}
  }
 }
 outpw(0x3ce,0x0004);outpw(0x3ce,0x0001);return 1;
}

static void clear_video(void){
 uint32_t a=0,total=(uint32_t)pitch*height,bytes=(uint32_t)span_k*1024,left;
 unsigned n;
 if(depth==16){outpw(0x3c4,0x0f02);outpw(0x3ce,0x0001);}
 while(a<total){
  left=bytes-(a%bytes);if(left>total-a)left=total-a;
  n=left>16384UL?16384:(unsigned)left;_fmemset(address(a),0,n);a+=n;
 }
}

static void palette_glow(unsigned gain){
 static const unsigned char shades[4]={2,6,11,15};unsigned i,k,r,g;
 for(i=0;i<4;i++){
  k=shades[i];r=k==15?47:0;g=k==15?63:3+k*4;
  outp(0x3c8,depth==16?k:k*16+15);
  outp(0x3c9,(r*gain)>>6);outp(0x3c9,(g*gain)>>6);outp(0x3c9,(r*gain)>>6);
 }
}
static unsigned shade(int distance,int len){return distance==0?15:distance<len/2?11:distance<len*3/4?6:2;}
static void paint(int x,int y,unsigned color){if(y>=0&&y<rows)draw(x,y,cells[((unsigned)y<<7)+x].ch,color);}
static void reset(int x){length[x]=10+rnd(rows/2);period[x]=1+rnd(3);countdown[x]=rnd(period[x]);head[x]=-(int)rnd(18);}
static void advance(void){
 int x,y,old,d;
 for(x=0;x<cols;x++){
  if(countdown[x]-- >0)continue;countdown[x]=period[x]-1;old=head[x]++;
  paint(x,old,11);paint(x,head[x]-length[x]/2,6);paint(x,head[x]-length[x]*3/4,2);paint(x,head[x]-length[x],0);
  y=head[x];if(y>=0&&y<rows)draw(x,y,33+rnd(94),15);
  if(rnd(5)==0){d=1+rnd(length[x]-1);y=head[x]-d;if(y>=0&&y<rows)draw(x,y,33+rnd(94),shade(d,length[x]));}
  if(head[x]-length[x]>=rows)reset(x);
 }
}
static int rain(void){
 int x,y,d,key,paused=0;unsigned last=ticks(),phase=0,pulse=0;
 rng=((uint32_t)ticks()<<16)|12345;palette();
 if(depth==16&&!planar_probe())return 0;
 clear_video();renderer_init();
 for(x=0;x<cols;x++){
  reset(x);head[x]=rnd(rows+length[x]);
  for(y=0;y<rows;y++)cells[((unsigned)y<<7)+x].ch=(unsigned char)(33+rnd(94));
  for(d=length[x]-1;d>=0;d--)paint(x,head[x]-d,shade(d,length[x]));
 }
 flush_draw();if(glow_enabled)palette_glow(56+glow[0]);
 while(running){
  if(kbhit()){key=getch();if(!key){getch();continue;}if(key==27||key=='q'||key=='Q')break;if(key==' ')paused=!paused;if(key=='g'||key=='G'){glow_enabled=!glow_enabled;palette_glow(glow_enabled?56+glow[phase]:64);}}
  if(ticks()!=last){
   last=ticks();if(!paused){advance();flush_draw();
    if(glow_enabled&&++pulse==8){pulse=0;phase=(phase+1)&63;palette_glow(56+glow[phase]);}
   }
  }else idle();
 }
 return 1;
}
int main(int argc,char **argv){
 union REGS r;int res,color,key,oldmode,no87=0,i;
 void (__interrupt __far *oldbreak)(void);
 for(i=1;i<argc;i++){
  if(!stricmp(argv[i],"/NO87"))no87=1;
  else if(!stricmp(argv[i],"/NOGLOW"))glow_enabled=0;
  else{puts("MATRIX [/NO87] [/NOGLOW]\n/NO87 uses integer maths without probing an 8087.\n/NOGLOW starts with the gentle palette glow disabled.");return 0;}
 }
 have_fpu=no87?0:matrix_fpu_detect();matrix_glow_table(glow,have_fpu);
 memset(&r,0,sizeof(r));r.w.ax=0x1a00;int86(0x10,&r,&r);if(r.h.al!=0x1a){puts("MATRIX needs a VGA-compatible card.");return 1;}
 r.h.ah=15;int86(0x10,&r,&r);oldmode=r.h.al&127;
 oldbreak=_dos_getvect(0x23);_dos_setvect(0x23,stop);
 while(running){
  biosmode(3);printf("MATRIX 2.0 - NuXT digital rain\nGlow maths: %s\n",have_fpu?"8087 / x87 detected":"integer CPU fallback");puts("\nSilent. ESC/Q returns to this menu. Space pauses; G toggles glow.\n\nResolution: 1  640x480 (default)\n            2  800x600\n            3  1024x768\n\nENTER = 640x480     Q = quit");
  key=getch();if(key=='q'||key=='Q'||key==27)break;res=key=='2'?1:key=='3'?2:0;
  puts("\nColours: 1  16 (default)    2  256\nENTER = 16 colours");key=getch();if(key==27)continue;color=key=='2'?1:0;
  if(!startmode(res,color)){biosmode(3);puts("That mode is unavailable.\nHigher modes need a supported Trident BIOS or banked VESA BIOS/driver.\n1024x768 in 256 colours needs at least 768 KB VRAM (NuXT has 512 KB).\n\nPress a key to choose again.");getch();continue;}
  if(!rain()){biosmode(3);puts("This BIOS does not provide working planar video memory for that mode.\nChoose 640x480/16 or a 256-colour mode. Press a key.");getch();}
 }
 biosmode(oldmode);_dos_setvect(0x23,oldbreak);puts("Matrix closed.");return 0;
}
