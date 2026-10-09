/* SUNSET packed-video test. Uploads every visible byte, then compares against
 * independently generated expected rows through both the backend and a direct
 * BIOS/native aperture reader. No frame-size conventional-memory allocation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include <stdint.h>
#include "../SUNSET.H"

static unsigned char source[802],actual[802],expect[800],modeinfo[512];
static FILE *report;
static unsigned ref_pitch,ref_seg,ref_gran,ref_size,ref_window,ref_flags;
static int ref_native,ref_linear,ref_bank=-1,failures;
static const uint32_t crc_nibble[16]={
 0x00000000UL,0x1db71064UL,0x3b6e20c8UL,0x26d930acUL,
 0x76dc4190UL,0x6b6b51f4UL,0x4db26158UL,0x5005713cUL,
 0xedb88320UL,0xf00f9344UL,0xd6d6a3e8UL,0xcb61b38cUL,
 0x9b64c2b0UL,0x86d3d2d4UL,0xa00ae278UL,0xbdbdf21cUL
};
static uint32_t crc_byte(uint32_t c,unsigned char value){
 c^=value;c=(c>>4)^crc_nibble[c&15];return (c>>4)^crc_nibble[c&15];
}
static void failed(const char *what,int x,int y){
 if(failures<12)fprintf(report,"FAIL %s at (%d,%d)\n",what,x,y);++failures;
}
static unsigned info_word(unsigned n){return modeinfo[n]|((unsigned)modeinfo[n+1]<<8);}
static int reference_init(int res){
 union REGS r;struct SREGS s;
 ref_bank=-1;ref_linear=res==2;
 if(ref_linear){ref_pitch=320;ref_seg=0xa000;ref_gran=ref_size=64;return 1;}
 ref_native=!strncmp(video_driver(),"TVGA",4);
 if(ref_native){
  ref_pitch=video_width();ref_seg=0xa000;ref_gran=ref_size=64;
  outp(0x3c4,0x0b);if(inp(0x3c5)!=0x43)return 0;
  outp(0x3c4,0x0e);ref_flags=inp(0x3c5)&0xf0;return 1;
 }
 memset(&r,0,sizeof(r));memset(modeinfo,0,sizeof(modeinfo));segread(&s);
 r.w.ax=0x4f01;r.w.cx=res?0x103:0x101;r.w.di=FP_OFF(modeinfo);s.es=FP_SEG(modeinfo);
 int86x(0x10,&r,&r,&s);if(r.w.ax!=0x004f)return 0;
 ref_pitch=info_word(16);ref_window=(modeinfo[2]&7)==7?0:1;
 ref_seg=info_word(ref_window?10:8);ref_gran=info_word(4);ref_size=info_word(6);
 if(ref_size>64)ref_size=64;
 return ref_pitch>0&&ref_gran>0&&ref_size>0;
}
/* Deliberately use 32-bit linear address arithmetic independently for each
 * byte, rather than sharing the backend's 16-bit scanline/chunk algorithm. */
static int reference_byte(uint32_t a,unsigned char *value){
 uint32_t bytes=(uint32_t)ref_size*1024UL;
 unsigned b=(unsigned)(a/bytes),off=(unsigned)(a%bytes);union REGS r;
 if((int)b!=ref_bank){
  if(ref_linear){ref_bank=0;*value=*(volatile unsigned char __far *)MK_FP(0xa000,off);return !b;}
  if(ref_native)outpw(0x3c4,((ref_flags|(b^2))<<8)|0x0e);
  else{
   memset(&r,0,sizeof(r));r.w.ax=0x4f05;r.w.bx=ref_window;
   r.w.dx=(unsigned)((uint32_t)b*ref_size/ref_gran);int86(0x10,&r,&r);
   if(r.w.ax!=0x004f)return 0;
  }
  ref_bank=(int)b;
 }
 *value=*(volatile unsigned char __far *)MK_FP(ref_seg,off);return 1;
}
static unsigned char base_pixel(unsigned x,unsigned y){
 return (unsigned char)((x*17U+y*29U+(x>>3)+(y>>2))^(x>>1));
}
static void patch(unsigned y,unsigned w,unsigned *x,unsigned *n){
 *x=1U+(y*13U)%(w-34U);*n=1U+y%31U;
}
static unsigned char patch_pixel(unsigned i,unsigned y){return (unsigned char)(0xe3U+i*7U+y);}
static void test_palette(void){
 unsigned i,first;union REGS r;struct SREGS s;
 for(i=0;i<768;i++)source[i]=(unsigned char)((i*13U+7U)&63);
 video_palette(source);
 for(i=0;i<9;i++)actual[i]=(unsigned char)(0xc0U+i*5U);
 video_palette_range(253,3,actual);
 for(i=0;i<9;i++)source[759+i]=actual[i]&63;
 video_palette_range(255,2,actual); /* Invalid range must be ignored. */
 video_palette_range(256,1,actual);
 if(strstr(video_driver(),"BIOS palette")){
  for(first=0;first<256;first+=128){
   memset(&r,0,sizeof(r));segread(&s);r.w.ax=0x4f09;r.h.bl=1;
   r.w.cx=128;r.w.dx=first;r.w.di=FP_OFF(modeinfo);s.es=FP_SEG(modeinfo);
   int86x(0x10,&r,&r,&s);
   if(r.w.ax!=0x004f){failed("BIOS palette read",first,0);return;}
   for(i=0;i<128;i++)if(modeinfo[i*4]!=source[(first+i)*3+2]||
    modeinfo[i*4+1]!=source[(first+i)*3+1]||modeinfo[i*4+2]!=source[(first+i)*3])
    failed("BIOS palette/range",first+i,0);
  }
 }else{
  outp(0x3c7,0);
  for(i=0;i<768;i++)if((inp(0x3c9)&63)!=source[i])failed("palette/range",i/3,i%3);
 }
}
static void expected_row(unsigned y,unsigned w){
 unsigned x,n,i;uint32_t edge,total=(uint32_t)w*video_height();
 for(i=0;i<w;i++)expect[i]=base_pixel(i,y);
 patch(y,w,&x,&n);for(i=0;i<n;i++)expect[x+i]=patch_pixel(i,y);
 /* The standard 640/800 packed scanlines intersect every 64K boundary at
  * least 64 pixels from the edges. Nineteen bytes straddle each boundary. */
 for(edge=65536UL;edge<total;edge+=65536UL){
  if((unsigned)(edge/w)==y){x=(unsigned)(edge%w)-9;
   for(i=0;i<19;i++)expect[x+i]=(unsigned char)(0x81U+i*11U+(unsigned)(edge>>16));
  }
 }
}
static int test_mode(int res){
 unsigned w,h,x,y,n,i;int before=failures;unsigned char value;
 uint32_t a,edge,total,expected_crc=0xffffffffUL,read_crc=0xffffffffUL,direct_crc=0xffffffffUL;
 union REGS r;
 if(!video_open(res)){
  memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);
  fprintf(report,"%s resolution %d unavailable; BIOS mode %u\n",r.h.al==3?"SKIP":"FAIL",res,r.h.al);
  if(r.h.al!=3)++failures;return 0;
 }
 w=video_width();h=video_height();total=(uint32_t)w*h;
 fprintf(report,"%ux%u %s\n",w,h,video_driver());fflush(report);
 test_palette();
 if(!video_clear(0x6d))failed("clear",0,0);
 /* Invalid calls must leave the framebuffer untouched. Zero-length calls
  * permit an end-of-row pointer and do not dereference their data argument. */
 if(video_write_span(-1,0,source,1)||video_write_span(w-1,0,source,2)||
    video_write_span(0,h,source,1)||video_write_span(0,0,source,65535U)||
    video_read_span(0,-1,actual,1)||!video_write_span(w,0,0,0)||
    !video_read_span(w,0,0,0))failed("bounds contract",0,0);
 for(y=0;y<h;y++){
  if(!video_read_span(0,y,actual,w)){failed("clear read",0,y);break;}
  for(x=0;x<w;x++)if(actual[x]!=0x6d){failed("clear/bounds byte",x,y);break;}
 }
 /* Reverse row order repeatedly traverses the aperture in both directions.
  * Upload from source+1 exercises unaligned near source addresses. */
 for(y=h;y>0;y--){
  for(x=0;x<w;x++)source[x+1]=base_pixel(x,y-1);
  if(!video_write_span(0,y-1,source+1,w))failed("full row upload",0,y-1);
 }
 for(y=0;y<h;y++){
  patch(y,w,&x,&n);for(i=0;i<n;i++)source[i+1]=patch_pixel(i,y);
  if(!video_write_span(x,y,source+1,n))failed("odd patch",x,y);
 }
 for(edge=65536UL;edge<total;edge+=65536UL){
  y=(unsigned)(edge/w);x=(unsigned)(edge%w)-9;
  for(i=0;i<19;i++)source[i+1]=(unsigned char)(0x81U+i*11U+(unsigned)(edge>>16));
  if(!video_write_span(x,y,source+1,19))failed("bank crossing",x,y);
 }
 for(y=0;y<h;y++){
  expected_row(y,w);memset(actual,0xa7,sizeof(actual));
  if(!video_read_span(0,y,actual+1,w)){failed("row readback",0,y);break;}
  if(actual[0]!=0xa7||actual[w+1]!=0xa7)failed("read guard",0,y);
  for(x=0;x<w;x++){
   expected_crc=crc_byte(expected_crc,expect[x]);read_crc=crc_byte(read_crc,actual[x+1]);
   if(expect[x]!=actual[x+1])failed("backend pixel",x,y);
  }
 }
 /* Also test the odd-sized read kernel, including every crossing boundary. */
 for(edge=65536UL;edge<total;edge+=65536UL){
  y=(unsigned)(edge/w);x=(unsigned)(edge%w)-9;expected_row(y,w);
  memset(actual,0xa7,sizeof(actual));
  if(!video_read_span(x,y,actual+1,19))failed("cross-bank read",x,y);
  for(i=0;i<19;i++)if(actual[i+1]!=expect[x+i])failed("cross-bank byte",x+i,y);
  if(actual[0]!=0xa7||actual[20]!=0xa7)failed("odd read guard",x,y);
 }
 /* This reader changes the hardware bank behind the backend's cache. It is
  * intentionally the LAST graphics operation before closing the mode. */
 if(!reference_init(res))failed("reference setup",0,0);
 else for(y=0;y<h;y++){
  expected_row(y,w);a=(uint32_t)y*ref_pitch;
  for(x=0;x<w;x++){
   if(!reference_byte(a+x,&value)){failed("direct read",x,y);break;}
   direct_crc=crc_byte(direct_crc,value);
   if(value!=expect[x])failed("direct pixel",x,y);
  }
 }
 video_close();
 fprintf(report,"expected %08lx backend %08lx direct %08lx; %lu visible bytes, %lu 64K crossings\n",
  (unsigned long)~expected_crc,(unsigned long)~read_crc,(unsigned long)~direct_crc,
  (unsigned long)total,(unsigned long)((total-1)>>16));
 if(expected_crc!=read_crc||expected_crc!=direct_crc)failed("CRC mismatch",0,0);
 fprintf(report,"%s packed upload, guards, bank boundaries and independent aperture CRC\n",
  failures==before?"PASS":"FAIL");fflush(report);return 1;
}
int main(int argc,char **argv){
 int first=0,last=1,res,ran=0;
 report=fopen("VIDTEST.TXT","w");if(!report)return 2;
 if(argc>1)first=last=atoi(argv[1]);
 fputs("SUNSET banked 8-bit video: V20/80186 build, one framebuffer.\n",report);
 for(res=first;res<=last;res++)ran+=test_mode(res);
 fprintf(report,"%d mode(s) tested; %d failure(s). Native scanout still needs the physical card.\n",ran,failures);
 fclose(report);return failures?1:ran?0:2;
}
