/* Full-frame readback against an independent point raster, plus FPU checks. */
#define main star_main
#include "STAR.C"
#undef main
#include <malloc.h>
static unsigned errors;
static unsigned char expected[1024];
static Star before[32],after[32];
static void status87(unsigned *p);
#pragma aux status87 = 0x9b 0xdd 0x3c 0x9b parm [si] modify exact [];
static void test_points(Star *v,unsigned phase){
 unsigned i;for(i=0;i<32;i++){
  v[i].sx=(unsigned long)(i*179U+phase*29U)%(width-2);
  v[i].sy=(unsigned long)(i*71U+phase*13U)%(height-2);
  v[i].size=(i&1)+1;v[i].shade=(i%15)+1;
 }
 /* Same span, overlapping colours, and every screen edge. */
 v[0].sx=v[1].sx=7;v[0].sy=v[1].sy=0;v[0].size=v[1].size=2;
 v[2].sx=width-2;v[2].sy=height-2;v[2].size=2;
 v[3].sx=0;v[3].sy=height-1;v[3].size=1;
 v[4].sx=width-1;v[4].sy=0;v[4].size=1;
}
static void verify(Star *v,unsigned n){
 unsigned y,x,i,j,k,p,a,c;unsigned char __far *mem;
 for(y=0;y<(unsigned)height;y++){
  memset(expected,0,width);
  for(i=0;i<n;i++)if(y>=(unsigned)v[i].sy&&y<(unsigned)v[i].sy+v[i].size)
   for(j=0;j<v[i].size;j++){x=v[i].sx+j;if(expected[x]<v[i].shade)expected[x]=v[i].shade;}
  if(depth==256){
   for(x=0;x<(unsigned)width;x++)if(*address((uint32_t)y*pitch+x)!=expected[x])errors++;
  }else{
   for(p=0;p<4;p++){
    outpw(0x3ce,(p<<8)|4);
    for(x=0;x<(unsigned)width;x+=8){
     c=0;for(k=0;k<8;k++)if(expected[x+k]&(1U<<p))c|=128U>>k;
     a=x/8;mem=address((uint32_t)y*pitch+a);if(*mem!=c)errors++;
    }
   }
  }
 }
 outpw(0x3ce,0x0004);
}
int main(void){
 FILE *log=fopen("CHECK.TXT","w");unsigned i,cw,aftercw,status,scratch[2],start,prev;
 unsigned __far *saved=_fmalloc(8192);int res,col,found=probe87(scratch),x,w;
 if(!log||!saved)return 2;
 fprintf(log,"8087 detected: %d\n",found);
 for(res=0;res<4;res++){
  width=res==0?640:res==1?800:res==2?1024:320;height=res==0?480:res==1?600:res==2?768:200;pitch=width;
  make_tables(0);_fmemcpy(saved,coeff_x,4096);_fmemcpy(saved+2048,coeff_y,4096);
  if(found){
   control87(&cw);aftercw=0x077f;setcontrol87(&aftercw);make_tables(1);control87(&aftercw);status87(&status);
   if(aftercw!=0x077f||(status&0x3841))errors++;setcontrol87(&cw);
   for(i=0;i<2048;i++)if(saved[i]!=coeff_x[i]||saved[i+2048]!=coeff_y[i])errors++;
  }
  for(i=64;i<2048;i+=7)for(x=-1024;x<1024;x+=13){
   long product=(long)x*coeff_x[i];w=(int)(product>=0?product/256:-((-product+255)/256));
   if(project(x,coeff_x[i])!=w)errors++;
  }
 }
 fprintf(log,"Perspective, integer assembly, FPU rounding/stack errors: %u\n",errors);_ffree(saved);
 for(res=0;res<4;res++)for(col=0;col<2;col++){
  if(!startmode(res,col)){fprintf(log,"mode %d/%d unavailable\n",res,col);continue;}
  start=ticks();while((unsigned)(ticks()-start)<2)idle();
  palette();if(depth==16&&!planar_probe()){errors++;fprintf(log,"planar probe FAILED\n");continue;}
  clear_video();make_tables(0);test_points(before,0);test_points(after,1);prev=errors;
  events_begin();for(i=0;i<32;i++)footprint(&before[i],0);compose();present();verify(before,32);
  events_begin();for(i=0;i<32;i++){footprint(&before[i],1);footprint(&after[i],0);}compose();present();verify(after,32);
  if(res==0){
   clear_video();count=256;memset(stars,0,sizeof(stars));
   for(i=0;i<count;i++){spawn(&stars[i],1);locate(&stars[i]);}
   for(i=0;i<3;i++){advance(8);verify(stars,count);}
  }
  fprintf(log,"%dx%d/%d: full-frame initial + move/erase/overlap errors %u\n",width,height,depth,errors-prev);fflush(log);
 }
 biosmode(3);fprintf(log,"TOTAL ERRORS: %u\n",errors);fclose(log);return errors?1:0;
}
