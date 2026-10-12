/* 320x200 compositor: decoded background + reusable frame, 65K each.
 * Each buffer's tail holds 1000 8x8 dirty masks. Old geometry is restored
 * in RAM before drawing; only completed old/new dirty runs reach VGA.
 * Higher resolutions retain the sparse tile renderer. */
#include <dos.h>
#include <malloc.h>
#include <string.h>
#include "SUNSET.H"
static void __far *raw_bg;
static void __far *raw_frame;
static unsigned char __far *bg;
static unsigned char __far *frame;
static unsigned bgseg,frameseg,ready,used;
static unsigned char scan[320],changed[40];
static unsigned runs[40];
static unsigned long uploaded;
static const unsigned char bits[8]={1,2,4,8,16,32,64,128};
typedef struct {unsigned off,tile,ink;int sx,sy;unsigned subx,major,minor,frame,bg,ymajor;} Line;
static Line line;
extern void fast320_line_asm(const Line *p);
#pragma aux fast320_line_asm parm [si] modify exact [ax bx cx dx si di];
extern unsigned fast320_runs(const unsigned char *,unsigned,unsigned *);
#pragma aux fast320_runs parm [si] [ax] [di] value [ax] modify exact [ax bx cx dx si di];
/* Scan/coalesce one dirty row and copy it in one assembly call. The source
 * and destination share offsets; masks stay in near memory. */
extern unsigned fast320_blitrow(const unsigned char *,unsigned,unsigned,unsigned,unsigned);
#pragma aux fast320_blitrow parm [si] [bx] [di] [ax] [dx] value [ax] modify exact [ax bx cx dx si di];
extern unsigned fast320_masks(unsigned,unsigned,unsigned,unsigned char *);
#pragma aux fast320_masks parm [ax] [dx] [si] [di] value [ax] modify exact [ax bx cx dx si di];
static void copy_run(unsigned srcseg,unsigned dstseg,unsigned off,unsigned n);
#pragma aux copy_run = \
 "push ds" "push es" "mov ds,ax" "mov es,dx" "mov di,si" \
 "cld" "shr cx,1" "rep movsw" "adc cx,cx" "rep movsb" "pop es" "pop ds" \
 parm [ax] [dx] [si] [cx] modify exact [si di cx];
static void hspan(unsigned srcseg,unsigned dstseg,unsigned off,unsigned n,unsigned color);
#pragma aux hspan = \
 "push ds" "push es" "mov ds,dx" "mov es,bx" \
 "fhpixel: cmp byte ptr [di],192" "jae fhskip" "mov es:[di],al" \
 "fhskip: inc di" "loop fhpixel" "pop es" "pop ds" \
 parm [dx] [bx] [di] [cx] [ax] modify exact [di cx];

void fast320_close(void){
 if(raw_bg)_ffree(raw_bg);if(raw_frame)_ffree(raw_frame);
 raw_bg=raw_frame=0;bg=frame=0;ready=used=0;uploaded=0;
 _fheapshrink();
}
int fast320_open(void){
 fast320_close();
 raw_bg=_fmalloc(65016U);raw_frame=_fmalloc(65016U);
 if(!raw_bg||!raw_frame){fast320_close();return 0;}
 /* Round up within the allocation so offsets 0..64999 never wrap. */
 bgseg=FP_SEG(raw_bg)+((FP_OFF(raw_bg)+15U)>>4);
 frameseg=FP_SEG(raw_frame)+((FP_OFF(raw_frame)+15U)>>4);
 bg=MK_FP(bgseg,0);frame=MK_FP(frameseg,0);
 _fmemset(bg+64000U,0,1000);_fmemset(frame+64000U,0,1000);
 _fheapshrink();return 1;
}
int fast320_cache(void (*decode)(unsigned,unsigned char *)){
 unsigned y;
 if(!bg||!decode)return 0;
 for(y=0;y<200;y++){
  decode(y,scan);_fmemcpy(bg+y*320U,scan,320);
 }
 copy_run(bgseg,frameseg,0,64000U);ready=1;return 1;
}
static void restore_old(void){
 unsigned tr,r,bit,off;unsigned char any;
 for(tr=0;tr<25;tr++){
  any=(unsigned char)fast320_masks(bgseg,0,64000U+tr*40,changed);
  if(!any)continue;
  for(r=0;r<8;r++){
   bit=bits[r];if(!(any&bit))continue;off=(tr*8+r)*320U;
   fast320_blitrow(changed,bit,off,bgseg,frameseg);
  }
 }
}
void fast320_begin(void){
 if(!ready)return;
 restore_old();_fmemset(frame+64000U,0,1000);used=0;uploaded=0;
}
void fast320_line(int x0,int y0,int x1,int y1,unsigned char color){
 unsigned dx,dy,c,end,off;int n;
 if(!ready)return; /* Endpoints already clipped by the public renderer. */
 if(y0==y1){
  if(x0>x1){n=x0;x0=x1;x1=n;}
  off=(unsigned)y0*320U+x0;
  hspan(bgseg,frameseg,off,x1-x0+1,color);
  c=(unsigned)x0>>3;end=(unsigned)x1>>3;off=64000U+((unsigned)y0>>3)*40;
  for(;c<=end;c++)frame[off+c]|=bits[y0&7];return;
 }
 dx=x0<x1?x1-x0:x0-x1;dy=y0<y1?y1-y0:y0-y1;
 line.off=(unsigned)y0*320U+x0;line.tile=((unsigned)y0>>3)*40+((unsigned)x0>>3);
 line.ink=color+((unsigned)bits[y0&7]<<8);line.sx=x0<x1?1:-1;line.sy=y0<y1?320:-320;
 line.subx=line.sx>0?0:7;line.ymajor=dy>dx;line.major=dy>dx?dy:dx;line.minor=dy>dx?dx:dy;
 line.frame=frameseg;line.bg=bgseg;fast320_line_asm(&line);
}
int fast320_present(void){
 unsigned tr,r,bit,start,off,y,n,count,k,masks;unsigned char any;
 uploaded=0;used=0;if(!ready)return 0;
 for(tr=0;tr<25;tr++){
  masks=fast320_masks(bgseg,frameseg,64000U+tr*40,changed);
  any=(unsigned char)masks;used+=masks>>8;
  if(!any)continue;
  for(r=0;r<8;r++){
   bit=bits[r];if(!(any&bit))continue;y=tr*8+r;off=y*320U;
   if(video_colors()==256){
    uploaded+=fast320_blitrow(changed,bit,off,frameseg,0xa000);
    continue;
   }
   count=fast320_runs(changed,bit,runs);
   for(k=0;k<count;k++){
    start=runs[k*2];n=runs[k*2+1];
    {
     _fmemcpy(scan,frame+off+start,n);
     if(!video_write_span(start,y,scan,n)){
      /* Preserve old/new union if the caller retries or starts another frame. */
      unsigned i;for(i=0;i<1000;i++)bg[64000U+i]|=frame[64000U+i];return 0;
     }
     uploaded+=(n>>3)*4U;
    }
   }
  }
 }
 copy_run(frameseg,bgseg,64000U,1000);return 1;
}
unsigned fast320_count(void){return used;}
unsigned long fast320_bytes(void){return uploaded;}
