/* Compare the actual RAM/assembly renderer with an independent reference,
 * including every octant, tile crossing, overlap, car mask and restoration. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "../SUNSET.H"
static unsigned char __far *ref;
static unsigned char row[320];
static unsigned decode_calls,failures,seed=1973;
static FILE *logfile;
static unsigned random16(void){seed=seed*25173U+13849U;return seed;}
static unsigned char bg(unsigned x,unsigned y){
 if((x>125&&x<190&&y>120&&y<190)||((x+y)%103==0))return 192+(x+y)%64;
 return 1+(x*3+y*7)%150;
}
static void decode(unsigned y,unsigned char *p){unsigned x;decode_calls++;for(x=0;x<320;x++)p[x]=bg(x,y);}
static void reset_ref(void){unsigned x,y;for(y=0;y<200;y++)for(x=0;x<320;x++)ref[y*320U+x]=bg(x,y);}
static void reference_line(int x0,int y0,int x1,int y1,unsigned char c){
 int dx=abs(x1-x0),dy=abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,e=dx-dy,e2;
 for(;;){
  if(bg(x0,y0)<192)ref[(unsigned)y0*320U+x0]=c;
  if(x0==x1&&y0==y1)break;
  e2=e+e;if(e2>-dy){e-=dy;x0+=sx;}if(e2<dx){e+=dx;y0+=sy;}
 }
}
static void draw(int a,int b,int c,int d,unsigned char ink){tiles_line(a,b,c,d,ink);reference_line(a,b,c,d,ink);}
static void check(unsigned colors,unsigned frame){
 unsigned x,y,want,before=failures;
 if(!tiles_present(decode)){failures++;return;}
 for(y=0;y<200;y++){
  if(!video_read_span(0,y,row,320)){failures++;return;}
  for(x=0;x<320;x++){
   want=ref[y*320U+x];if(colors==16)want=plane_map(want,x,y);
   if(row[x]!=want){if(failures<12)fprintf(logfile,"FAIL %u-col frame %u at %u,%u: %u != %u\n",colors,frame,x,y,row[x],want);failures++;}
  }
 }
 if(decode_calls!=200){fprintf(logfile,"FAIL unexpected decode count %u\n",decode_calls);failures++;}
 fprintf(logfile,"%u-col frame %u: %s, %lu bytes uploaded\n",colors,frame,before==failures?"PASS":"FAIL",tiles_bytes());fflush(logfile);
}
int main(void){
 unsigned colors,f,i,a,b,c,d;int res=0;
 logfile=fopen("FASTTEST.TXT","w");if(!logfile)return 2;
 ref=_fmalloc(64000U);if(!ref)return 2;
 for(colors=256;colors>=16;colors-=240){
  video_depth(colors);if(!video_open(2)||!tiles_open(320,200)){failures++;break;}
  if(strcmp(tiles_driver(),"cached 320 / assembly lines")){failures++;break;}
  decode_calls=0;if(!tiles_cache_background(decode)){failures++;break;}
  for(i=0;i<200;i++){unsigned x;for(x=0;x<320;x++)row[x]=bg(x,i);video_write_span(0,i,row,320);}
  reset_ref();tiles_begin();check(colors,0);
  for(f=1;f<=15;f++){
   reset_ref();tiles_begin();
   if(f<9){
    draw(0,0,319,199,177);draw(319,0,0,199,179);
    draw(319,199,0,0,178);draw(0,199,319,0,180);
    draw(0,7,319,7,181);draw(319,192,0,192,182);
    draw(7,199,7,0,183);draw(312,0,312,199,184);
    for(i=0;i<70;i++){
     a=random16()%320;b=random16()%200;c=random16()%320;d=random16()%200;
     draw(a,b,c,d,160+i%32);
    }
   }
   if(f==11){
    /* Alternating cells force the maximum twenty disjoint runs per row. */
    for(i=0;i<40;i+=2)draw(i*8,0,i*8,0,177);
    for(i=1;i<40;i+=2)draw(i*8,199,i*8,199,179);
   }
   if(f==12){draw(0,0,319,0,180);draw(0,199,319,199,181);}
   if(f==13)draw(319,199,319,199,183);
   check(colors,f);
   if((f==10||f==15)&&tiles_bytes()!=0){fprintf(logfile,"FAIL clean frame uploaded\n");failures++;}
  }
  if(_fheapchk()!=_HEAPOK||_nheapchk()!=_HEAPOK){failures++;fprintf(logfile,"FAIL heap guard\n");}
  tiles_close();video_close();if(colors==16)break;
 }
 _ffree(ref);res=failures!=0;fprintf(logfile,"%u failures\n",failures);fclose(logfile);return res;
}
