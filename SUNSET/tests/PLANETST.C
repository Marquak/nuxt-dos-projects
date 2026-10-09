/* All three true planar modes: independent plane readback, all semantic
 * palette mappings, unaligned patches, guards, clear, palette and bounds. */
#include "../SUNSET.H"
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include "../COLOR16.H"
static unsigned char src[802],got[802],expect[800],direct[800],raw[4][100];
static unsigned errors,checks;
static FILE *logfile;
#define CHECK(c) do{checks++;if(!(c)){if(errors<20)fprintf(logfile,"FAIL line %u\n",(unsigned)__LINE__);errors++;}}while(0)
static unsigned char pattern(unsigned x,unsigned y){return (unsigned char)((x*17+y*29+(x>>2))^(y>>1));}
static unsigned char mapped(unsigned c,unsigned x,unsigned y){return quant16[((y&1)<<1)|(x&1)][c];}
static void reference(unsigned y,unsigned w){
 unsigned p,b,i,x,mask;volatile unsigned char __far *v=MK_FP(0xa000,y*(w>>3));
 for(p=0;p<4;p++){outpw(0x3ce,(p<<8)|4);for(b=0;b<(w>>3);b++)raw[p][b]=v[b];}
 for(x=0;x<w;x++){
  mask=0x80>>(x&7);b=x>>3;i=0;
  for(p=0;p<4;p++)if(raw[p][b]&mask)i|=1U<<p;
  direct[x]=(unsigned char)i;
 }
 outpw(0x3ce,0x0004);
}
static void mode(int res){
 unsigned w,h,y,x,n,k,c;union REGS r;
 video_depth(16);CHECK(video_colors()==16);CHECK(video_open(res));
 w=video_width();h=video_height();
 fprintf(logfile,"%ux%u, %s\n",w,h,video_driver());fflush(logfile);
 if(w!=(res==2?320:res?800:640)||h!=(res==2?200:res?600:480)){errors++;return;}
 if(res!=1){memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);CHECK((r.h.al&127)==(res==2?0x0d:0x12));}
 video_palette(video_palette16());outp(0x3c7,0);
 for(x=0;x<48;x++)CHECK((inp(0x3c9)&63)==neon_palette16[x]);
 for(x=0;x<16;x++){inp(0x3da);outp(0x3c0,x);CHECK((inp(0x3c1)&63)==x);}
 inp(0x3da);outp(0x3c0,0x20);
 CHECK(video_clear(91));
 CHECK(!video_write_span(-1,0,src,1));CHECK(!video_write_span(w-1,0,src,2));
 CHECK(!video_write_span(0,h,src,1));CHECK(!video_write_span(0,0,src,65535));
 CHECK(!video_read_span(0,-1,src,1));CHECK(video_write_span(w,0,0,0));CHECK(video_read_span(w,0,0,0));
 for(y=0;y<h;y++){
  CHECK(video_read_span(0,y,got,w));
  for(x=0;x<w;x++)if(got[x]!=mapped(91,x,y)){CHECK(0);break;}
 }
 for(y=0;y<h;y++){
  for(x=0;x<w;x++)src[x+1]=pattern(x,y);
  CHECK(video_write_span(0,y,src+1,w));
  /* Exercise all starting bit positions and odd tails without disturbing
   * pixels outside the specified span. */
  x=1+(y*13)%(w-34);n=1+y%31;
  for(k=0;k<n;k++)src[k+1]=(unsigned char)(0xe3+k*7+y);
  CHECK(video_write_span(x,y,src+1,n));
 }
 for(y=0;y<h;y++){
  for(x=0;x<w;x++)expect[x]=mapped(pattern(x,y),x,y);
  x=1+(y*13)%(w-34);n=1+y%31;
  for(k=0;k<n;k++)expect[x+k]=mapped((unsigned char)(0xe3+k*7+y),x+k,y);
  memset(got,0xa7,sizeof(got));CHECK(video_read_span(0,y,got+1,w));
  CHECK(got[0]==0xa7&&got[w+1]==0xa7);CHECK(!memcmp(got+1,expect,w));
  reference(y,w);CHECK(!memcmp(direct,expect,w));
  memset(got,0xa7,sizeof(got));CHECK(video_read_span(x,y,got+1,n));
  CHECK(got[0]==0xa7&&got[n+1]==0xa7);CHECK(!memcmp(got+1,expect+x,n));
 }
 /* Read back palette patches and ensure an invalid range is ignored. */
 for(k=0;k<3;k++)src[k]=(unsigned char)(11+k*13);
 video_palette_range(15,1,src);video_palette_range(15,2,src);outp(0x3c7,15);
 for(k=0;k<3;k++){c=inp(0x3c9)&63;CHECK(c==src[k]);}
 video_close();memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);CHECK(r.h.al==3);
}
int main(void){
 logfile=fopen("PLANETST.TXT","w");if(!logfile)return 2;
 mode(2);mode(0);mode(1);video_depth(256);
 fprintf(logfile,"checks=%u errors=%u\n",checks,errors);fclose(logfile);return errors?1:0;
}
