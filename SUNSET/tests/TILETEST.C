/* Integration checks for sparse overlays, restoration and foreground masks.
 * Expected pixels describe the scenes directly; they do not use the tile
 * allocator or rasterizer. Every visible byte is checked after every frame. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <dos.h>
#include <stdint.h>
#include "../SUNSET.H"
static FILE *report;
static unsigned w,h,decode_calls,failures,pool_limit;
static unsigned char scan[800];
static const uint32_t crc_table[16]={
 0x00000000UL,0x1db71064UL,0x3b6e20c8UL,0x26d930acUL,
 0x76dc4190UL,0x6b6b51f4UL,0x4db26158UL,0x5005713cUL,
 0xedb88320UL,0xf00f9344UL,0xd6d6a3e8UL,0xcb61b38cUL,
 0x9b64c2b0UL,0x86d3d2d4UL,0xa00ae278UL,0xbdbdf21cUL
};
static uint32_t crc_byte(uint32_t c,unsigned char v){
 c^=v;c=(c>>4)^crc_table[c&15];return (c>>4)^crc_table[c&15];
}
static void failure(const char *s,unsigned x,unsigned y){
 if(failures<12)fprintf(report,"FAIL %s at %u,%u\n",s,x,y);++failures;
}
static unsigned char background(unsigned x,unsigned y){
 if((x>=w/2-20&&x<=w/2+30&&y>=h/2-15&&y<=h/2+20)||
    (!(x%67)&&!(y%13)))return (unsigned char)(192+((x+y)&63));
 return (unsigned char)(1+(x*3U+y*5U+(x>>4))%160);
}
static void decode(unsigned y,unsigned char *row){
 unsigned x;++decode_calls;for(x=0;x<w;x++)row[x]=background(x,y);
}
static int initialize_background(void){
 unsigned y;
 for(y=0;y<h;y++){decode(y,scan);if(!video_write_span(0,y,scan,w))return 0;}
 decode_calls=0;return 1;
}
static unsigned long far_used(void){
 struct _heapinfo item;unsigned long bytes=0;
 memset(&item,0,sizeof(item));
 while(_fheapwalk(&item)==_HEAPOK)if(item._useflag==_USEDENTRY)bytes+=item._size;
 return bytes;
}
/* Count this PSP's program/far-heap MCBs. The environment, normally preceding
 * the PSP, is excluded; it is not renderer storage. */
static unsigned long dos_owned(void){
 unsigned seg=_psp-1,size,owner,i;unsigned long bytes=0;
 const unsigned char __far *m;
 for(i=0;i<256;i++){
  m=MK_FP(seg,0);if(m[0]!='M'&&m[0]!='Z')break;
  owner=m[1]|((unsigned)m[2]<<8);size=m[3]|((unsigned)m[4]<<8);
  if(owner==_psp)bytes+=(unsigned long)size*16UL;
  if(m[0]=='Z'||seg+size+1<=seg)break;seg+=size+1;
 }
 return bytes;
}
static unsigned char expected(unsigned x,unsigned y,unsigned scene){
 unsigned char bg=background(x,y),v=0;uint32_t edge,total=(uint32_t)w*h;
 if(scene==1){
  if(y==7)v=176;
  if(x==5)v=177;
  if(x==y)v=178;
  if(y==8&&x>=8&&x<=39)v=179;
  if(y==8&&x>=16&&x<=24)v=180;
  if(y==h/2)v=181;
  for(edge=65536UL;edge<total;edge+=65536UL)
   if(y==(unsigned)(edge/w)&&x>=(unsigned)(edge%w)-10&&x<=(unsigned)(edge%w)+10)v=182;
 }else if(scene==2){if(y==h-12&&x>=w-17&&x<=w-2)v=187;}
 else if(scene==3){if(!(x&7)&&!(y&7)&&(y>>3)*(w>>3)+(x>>3)<pool_limit)v=189;}
 else if(scene==4){
  if(x==36&&y<=18)v=185;
  if(y==18&&x<=36)v=186;
 }
 return bg>=192||!v?bg:v;
}
static void check_frame(unsigned scene,const char *label){
 unsigned x,y;unsigned char want;uint32_t got=0xffffffffUL,ref=0xffffffffUL;
 for(y=0;y<h;y++){
  if(!video_read_span(0,y,scan,w)){failure("readback",0,y);return;}
  for(x=0;x<w;x++){
   want=expected(x,y,scene);got=crc_byte(got,scan[x]);ref=crc_byte(ref,want);
   if(scan[x]!=want){if(failures<12)fprintf(report,"pixel expected %u actual %u\n",want,scan[x]);failure(label,x,y);}
  }
 }
 fprintf(report,"%s expected=%08lx actual=%08lx upload=%lu decode=%u tiles=%u\n",
  label,(unsigned long)~ref,(unsigned long)~got,tiles_bytes(),decode_calls,tiles_count());
 if(got!=ref)failure("CRC",scene,0);fflush(report);
}
static int present(void){
 decode_calls=0;if(!tiles_present(decode)){failure("present",0,0);return 0;}return 1;
}
static void scene_one(void){
 uint32_t edge,total=(uint32_t)w*h;unsigned y,x,count;
 tiles_begin();
 tiles_line(-32768,7,32767,7,176);
 tiles_line(5,32767,5,-32768,177);
 tiles_line(-32768,-32768,32767,32767,178);
 tiles_line(39,8,8,8,179);tiles_line(16,8,24,8,180);
 tiles_line(0,h/2,w-1,h/2,181);
 for(edge=65536UL;edge<total;edge+=65536UL){
  y=(unsigned)(edge/w);x=(unsigned)(edge%w);tiles_line(x-10,y,x+10,y,182);
 }
 count=tiles_count();
 tiles_line(-32768,7,32767,7,0); /* Transparent: no erase and no allocation. */
 tiles_line(-32768,-7,32767,-7,190);
 tiles_line(-32768,32767,32767,-32768,190);
 if(tiles_count()!=count)failure("invisible allocation",0,0);
 if(tiles_overflow())failure("unexpected overflow",0,0);
}
static void test_mode(int res){
 unsigned x,y,i,before=failures;unsigned long heap0,heap1,owned;
 if(!video_open(res)){failure("video mode unavailable",res,0);return;}
 w=video_width();h=video_height();
 fprintf(report,"%ux%u %s\n",w,h,video_driver());fflush(report);
 if(!initialize_background()){failure("background upload",0,0);video_close();return;}
 check_frame(0,"initial background");
 heap0=far_used();
 if(!tiles_open(w,h)){failure("tile allocation",0,0);video_close();return;}
 heap1=far_used();owned=dos_owned();pool_limit=tiles_capacity();
 fprintf(report,"capacity=%u tile bytes=%lu far heap delta=%lu owned DOS bytes=%lu\n",
  pool_limit,(unsigned long)pool_limit*64UL,heap1-heap0,owned);
 if(pool_limit!=2048)failure("full test pool unavailable",pool_limit,0);
 if(heap1-heap0<pool_limit*64UL||heap1-heap0>pool_limit*64UL+64UL)failure("heap footprint",0,0);
 if(owned>307200UL)failure("DOS footprint exceeds 300KB",0,0);
 tiles_begin();present();
 if(tiles_bytes()||decode_calls)failure("empty frame work",0,0);
 check_frame(0,"empty");
 scene_one();present();check_frame(1,"crossings overlap occlusion clipped extremes");
 tiles_begin();tiles_line(w-17,h-12,w-2,h-12,187);present();
 if(tiles_count()!=3)failure("moving line tile count",0,0);
 check_frame(2,"moving scene restores stale tiles");
 tiles_begin();present();
 if(tiles_bytes()!=24UL||decode_calls!=1)failure("old-only upload size",0,0);
 check_frame(0,"old-only tiles restored");
 tiles_begin();present();
 if(tiles_bytes()||decode_calls)failure("clean frame work",0,0);
 /* Force each of the four far blocks to be used, then exceed capacity by
  * one tile. Exhaustion must flag/drop the new tile without corrupting RAM. */
 tiles_begin();
 for(i=0;i<=pool_limit;i++){
  x=(i%(w>>3))<<3;y=(i/(w>>3))<<3;tiles_line(x,y,x,y,189);
 }
 if(tiles_count()!=pool_limit||!tiles_overflow())failure("overflow contract",0,0);
 present();if(tiles_bytes()!=pool_limit*8UL)failure("pool upload size",0,0);
 check_frame(3,"all blocks and overflow");
 if(_fheapchk()!=_HEAPOK||_nheapchk()!=_HEAPOK)failure("heap guards",0,0);
 tiles_begin();if(tiles_overflow())failure("overflow reset",0,0);
 present();check_frame(0,"overflow scene restored");
 tiles_close();
 if(far_used()!=heap0)failure("freed tile pool",0,0);
 /* A 37x19 compositor exercises partial right/bottom tiles; the rest of
  * the physical framebuffer must remain the original static background. */
 if(!tiles_open(37,19))failure("partial geometry",0,0);
 else{
  tiles_begin();tiles_line(36,-32768,36,32767,185);tiles_line(-32768,18,32767,18,186);
  present();if(tiles_bytes()!=127UL||tiles_count()!=7)failure("partial upload size",0,0);
  check_frame(4,"partial edges");
  tiles_begin();present();check_frame(0,"partial edges restored");tiles_close();
 }
 if(far_used()!=heap0)failure("reopened pool freed",0,0);
 video_close();fprintf(report,"%s mode %d\n",before==failures?"PASS":"FAIL",res);fflush(report);
}
int main(int argc,char **argv){
 int first=0,last=1,res;if(argc>1)first=last=atoi(argv[1]);
 report=fopen("TILETEST.TXT","w");if(!report)return 2;
 fputs("SUNSET sparse-tile integration: full visible-byte checks each frame.\n",report);
 for(res=first;res<=last;res++)test_mode(res);
 fprintf(report,"%u failure(s).\n",failures);fclose(report);return failures?1:0;
}
