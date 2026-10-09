/* Sparse 8x8 overlay compositor for a single 256-colour framebuffer.
 * Four independently allocated 32 KB far blocks hold at most 2048 tiles.
 * Near metadata describes where those tiles belong; no full frame is stored.
 * Zero is transparent. Background indices 192..255 form the foreground car.
 * Complete new/background runs are composed in RAM before each video upload. */
#include <malloc.h>
#include <dos.h>
#include <string.h>
#include <stdint.h>
#include "SUNSET.H"

#define TILE_LIMIT 2048
#define MAP_LIMIT 7500
#define ROW_LIMIT 75
#define COL_LIMIT 100
static unsigned char __far *blocks[4];
static unsigned map[MAP_LIMIT],active_ids[TILE_LIMIT];
static unsigned char current_mask[TILE_LIMIT];
static const unsigned char row_bits[8]={1,2,4,8,16,32,64,128};
static unsigned char previous[MAP_LIMIT],current_rows[ROW_LIMIT],previous_rows[ROW_LIMIT];
static unsigned char row_buffer[800];
static unsigned char __far *row_tiles[COL_LIMIT];
static unsigned run_first[50],run_last[50];
static unsigned used,capacity,cols,rows,screen_w,screen_h;
static int lost,opened,planar,fast;
static uint32_t last_bytes;

void tiles_close(void){
 unsigned i,had_blocks=0;
 if(fast){fast320_close();fast=0;}
 for(i=0;i<4;i++)if(blocks[i]){_ffree(blocks[i]);blocks[i]=0;had_blocks=1;}
 if(had_blocks)_fheapshrink();
 used=capacity=cols=rows=screen_w=screen_h=0;lost=opened=0;last_bytes=0;
}
int tiles_open(int w,int h){
 unsigned i;
 tiles_close();
 if(w<1||w>800||h<1||h>600||w>video_width()||h>video_height())return 0;
 screen_w=(unsigned)w;screen_h=(unsigned)h;cols=(screen_w+7)>>3;rows=(screen_h+7)>>3;
 if(w==320&&h==200&&fast320_open()){fast=1;capacity=1000;opened=1;return 1;}
 for(i=0;i<4;i++){
  blocks[i]=_fmalloc(32768U);if(!blocks[i])break;capacity+=512;
 }
 if(!capacity){tiles_close();return 0;}
 /* Watcom reserves growth room beyond each 32 KB request. Return those
  * unused tails now; the allocated tile pointers remain at their addresses. */
 _fheapshrink();
 memset(map,0,cols*rows*sizeof(map[0]));memset(previous,0,cols*rows);
 memset(current_rows,0,sizeof(current_rows));memset(previous_rows,0,sizeof(previous_rows));
 planar=video_colors()==16;opened=1;return 1;
}
const char *tiles_driver(void){return fast?"cached 320 / assembly lines":"sparse tiles";}
int tiles_cache_background(void (*decode)(unsigned,unsigned char *)){
 return !fast||fast320_cache(decode);
}
void tiles_begin(void){
 unsigned i;
 if(fast){fast320_begin();return;}
 /* Clearing only the allocated entries avoids writing a 15 KB map on every
  * frame. The old dirty bitmap remains until that region has been replaced. */
 for(i=0;i<used;i++)map[active_ids[i]]=0;
 used=0;lost=0;memset(current_rows,0,sizeof(current_rows));
}
unsigned tiles_count(void){return fast?fast320_count():used;}
unsigned tiles_capacity(void){return capacity;}
int tiles_overflow(void){return lost;}
unsigned long tiles_bytes(void){return fast?fast320_bytes():last_bytes;}

static unsigned char __far *tile_pointer(unsigned index){
 return blocks[index>>9]+((index&511U)<<6);
}
static unsigned char __far *get_tile(unsigned id,unsigned tile_row){
 unsigned index=map[id];unsigned char __far *p;
 if(index)return tile_pointer(index-1);
 if(used==capacity){lost=1;return 0;}
 p=tile_pointer(used);_fmemset(p,0,64);
 current_mask[used]=0;active_ids[used]=id;map[id]=++used;current_rows[tile_row]=1;
 return p;
}

/* Signed endpoint differences reach 65535. Products of unsigned magnitudes
 * still fit 32 bits; widening BEFORE subtraction avoids 16-bit overflow. */
static long interpolate(long delta,long distance,long denominator){
 uint32_t a=delta<0?-delta:delta,b=distance<0?-distance:distance;
 uint32_t c=denominator<0?-denominator:denominator;
 long q=(long)((a*b)/c);
 return ((delta<0)^(distance<0)^(denominator<0))?-q:q;
}
static int region(int x,int y){
 return (x<0?1:x>=(int)screen_w?2:0)|(y<0?4:y>=(int)screen_h?8:0);
}
static int clip(int *x0,int *y0,int *x1,int *y1){
 int a=region(*x0,*y0),b=region(*x1,*y1),c,x,y;
 while(a|b){
  if(a&b)return 0;c=a?a:b;
  if(c&4){y=0;x=(int)((long)*x0+interpolate((long)*x1-*x0,0L-*y0,(long)*y1-*y0));}
  else if(c&8){y=screen_h-1;x=(int)((long)*x0+interpolate((long)*x1-*x0,(long)y-*y0,(long)*y1-*y0));}
  else if(c&2){x=screen_w-1;y=(int)((long)*y0+interpolate((long)*y1-*y0,(long)x-*x0,(long)*x1-*x0));}
  else {x=0;y=(int)((long)*y0+interpolate((long)*y1-*y0,0L-*x0,(long)*x1-*x0));}
  if(c==a){*x0=x;*y0=y;a=region(x,y);}else {*x1=x;*y1=y;b=region(x,y);}
 }
 return 1;
}
static void fill_tile_run(unsigned off,unsigned seg,unsigned n,unsigned color);
#pragma aux fill_tile_run = \
 "push es" "mov es,dx" "mov ah,al" "cld" "shr cx,1" \
 "rep stosw" "adc cx,cx" "rep stosb" "pop es" \
 parm [di] [dx] [cx] [ax] modify exact [ax di cx];

static void horizontal(int x0,int x1,unsigned y,unsigned char color){
 unsigned id,tr=y>>3,off=(y&7)<<3,n,left,x;unsigned char bit=row_bits[y&7];
 unsigned char __far *p;int swap;
 if(x0>x1){swap=x0;x0=x1;x1=swap;}
 id=tr*cols+((unsigned)x0>>3);x=x0&7;left=x1-x0+1;
 while(left){
  n=8-x;if(n>left)n=left;p=get_tile(id,tr);
  if(p){
   fill_tile_run(FP_OFF(p)+off+x,FP_SEG(p),n,color);
   current_mask[map[id]-1]|=bit;
  }
  left-=n;x=0;++id;
 }
}
void tiles_line(int x0,int y0,int x1,int y1,unsigned char color){
 int dx,dy,sx,sy,error,e2,subx,suby,last_suby=-1;unsigned id,tile_row,last_id=65535U;
 unsigned char __far *p=0;
 unsigned char *mask=0;
 if(!opened||!color||!clip(&x0,&y0,&x1,&y1))return;
 if(fast){fast320_line(x0,y0,x1,y1,color);return;}
 /* Road cross-lines are long horizontal runs. Fill at most eight RAM
  * pixels per tile, with one segment setup and one row-mask update. */
 if(y0==y1){horizontal(x0,x1,(unsigned)y0,color);return;}
 dx=x1>x0?x1-x0:x0-x1;dy=y1>y0?y1-y0:y0-y1;
 sx=x0<x1?1:-1;sy=y0<y1?1:-1;error=dx-dy;
 tile_row=(unsigned)y0>>3;id=tile_row*cols+((unsigned)x0>>3);
 subx=x0&7;suby=(y0&7)<<3;
 for(;;){
  /* Track tile crossings incrementally. A run of pixels in one tile needs
   * neither a multiply, a map lookup, nor a far-pointer rebuild per pixel. */
  if(id!=last_id){p=get_tile(id,tile_row);mask=p?current_mask+map[id]-1:0;last_id=id;last_suby=-1;}
  if(p){p[suby+subx]=color;if(suby!=last_suby){*mask|=row_bits[suby>>3];last_suby=suby;}}
  if(x0==x1&&y0==y1)break;
  e2=error+error;
  if(e2>-dy){
   error-=dy;x0+=sx;subx+=sx;
   if(subx==8){subx=0;++id;}else if(subx<0){subx=7;--id;}
  }
  if(e2<dx){
   error+=dx;y0+=sy;suby+=sy*8;
   if(suby==64){suby=0;id+=cols;++tile_row;}
   else if(suby<0){suby=56;id-=cols;--tile_row;}
  }
 }
}

static void commit_dirty(int complete){
 unsigned tr,c,base,i;
 if(!complete){
  /* A failed upload may have replaced part of its run. Retain the union of
   * old/new dirty tiles so a caller can retry or begin another frame safely. */
  for(i=0;i<used;i++)previous[active_ids[i]]|=current_mask[i];
  for(tr=0;tr<rows;tr++)previous_rows[tr]|=current_rows[tr];
  return;
 }
 for(tr=0;tr<rows;tr++)if(current_rows[tr]||previous_rows[tr]){
  base=tr*cols;for(c=0;c<cols;c++)previous[base+c]=map[base+c]?current_mask[map[base+c]-1]:0;
  previous_rows[tr]=current_rows[tr];
 }
}
/* The source lives in a far heap, destination in DGROUP. Full eight-pixel
 * rows are unrolled: one far read per pixel, preserving transparent zero and
 * the static foreground indices. Partial right-edge tiles use the short loop.
 * ES is restored; DS never changes and interrupts stay enabled. */
static void blend_row(unsigned char *dst,unsigned off,unsigned seg,unsigned n);
#pragma aux blend_row = \
 "push es" "mov es,ax" "cmp cx,8" "je brfull" \
 "brpart: mov al,es:[si]" "or al,al" "jz brskip" \
 "cmp byte ptr [di],192" "jae brskip" "mov [di],al" \
 "brskip: inc si" "inc di" "loop brpart" "jmp brdone" \
 "brfull: mov al,es:[si]" "or al,al" "jz br1" "cmp byte ptr [di],192" "jae br1" "mov [di],al" \
 "br1: mov al,es:[si+1]" "or al,al" "jz br2" "cmp byte ptr [di+1],192" "jae br2" "mov [di+1],al" \
 "br2: mov al,es:[si+2]" "or al,al" "jz br3" "cmp byte ptr [di+2],192" "jae br3" "mov [di+2],al" \
 "br3: mov al,es:[si+3]" "or al,al" "jz br4" "cmp byte ptr [di+3],192" "jae br4" "mov [di+3],al" \
 "br4: mov al,es:[si+4]" "or al,al" "jz br5" "cmp byte ptr [di+4],192" "jae br5" "mov [di+4],al" \
 "br5: mov al,es:[si+5]" "or al,al" "jz br6" "cmp byte ptr [di+5],192" "jae br6" "mov [di+5],al" \
 "br6: mov al,es:[si+6]" "or al,al" "jz br7" "cmp byte ptr [di+6],192" "jae br7" "mov [di+6],al" \
 "br7: mov al,es:[si+7]" "or al,al" "jz brdone" "cmp byte ptr [di+7],192" "jae brdone" "mov [di+7],al" \
 "brdone: pop es" \
 parm [di] [si] [ax] [cx] modify exact [ax si di cx];

int tiles_present(void (*decode_bg)(unsigned y,unsigned char *row)){
 unsigned tr,base,c,start,runs,r,y,last_y,n,x,off,count,bit;
 unsigned char changed[COL_LIMIT],mask;
 unsigned char __far *p;
 last_bytes=0;if(!opened||!decode_bg)return 0;
 if(fast)return fast320_present();
 for(tr=0;tr<rows;tr++)if(current_rows[tr]||previous_rows[tr]){
  base=tr*cols;mask=0;
  for(c=0;c<cols;c++){
   row_tiles[c]=map[base+c]?tile_pointer(map[base+c]-1):0;
   changed[c]=previous[base+c]|(map[base+c]?current_mask[map[base+c]-1]:0);
   mask|=changed[c];
  }
  last_y=(tr+1)<<3;if(last_y>screen_h)last_y=screen_h;
  for(y=tr<<3;y<last_y;y++){
   bit=row_bits[y&7];if(!(mask&bit))continue;
   decode_bg(y,row_buffer);off=(y&7)<<3;runs=0;c=0;
   while(c<cols){
    if(!(changed[c]&bit)){++c;continue;}
    start=c;do{++c;}while(c<cols&&(changed[c]&bit));
    run_first[runs]=start;run_last[runs]=c;++runs;
   }
   for(r=0;r<runs;r++){
    for(c=run_first[r];c<run_last[r];c++)if(row_tiles[c]){
     x=c<<3;n=screen_w-x;if(n>8)n=8;p=row_tiles[c]+off;
     blend_row(row_buffer+x,FP_OFF(p),FP_SEG(p),n);
    }
    x=run_first[r]<<3;count=run_last[r]<<3;if(count>screen_w)count=screen_w;
    count-=x;
    if(!video_write_span(x,y,row_buffer+x,count)){commit_dirty(0);return 0;}
    last_bytes+=planar?((count+7U)>>3)*4U:count;
   }
  }
 }
 commit_dirty(1);return 1;
}
