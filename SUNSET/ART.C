/* Embedded original indexed artwork. Each scanline is independently RLE
 * encoded as little-endian 16-bit pixel count followed by one colour byte.
 * Count bit 15 marks an alternating-pair run with two colour bytes instead.
 * The far ROM-like streams stay outside the DOS small-model data group.
 */
#include "SUNSET.H"
#include <string.h>
#ifdef SUNSET_ART_HOST
#define __far
#endif
#include "ART.H"

static const unsigned char __far *art_data;
static const unsigned __far *art_rows;
static unsigned art_width,art_height;
static unsigned char initial_row[800];

#ifndef SUNSET_ART_HOST
/* Source never crosses its own <64K segment. Set ES to the caller's near
 * data segment and DS to the selected artwork segment for the complete
 * row. Solid runs use REP STOSB; alternating dither pairs use REP STOSW.
 * REP STOSW preserves the odd-count carry for a final optional STOSB.
 * Preserve segment registers; interrupts remain enabled throughout.
 */
static void decode_row(const unsigned char __far *source,unsigned char *dest,unsigned width);
#pragma aux decode_row = \
    "push ds" "push es" "push ds" "pop es" "mov ds,dx" "cld" \
    "next_run: lodsw" "mov cx,ax" "and cx,7fffh" "sub bx,cx" \
    "test ah,80h" "jz solid_run" \
    "lodsw" "shr cx,1" "rep stosw" "jnc advance_run" "stosb" \
    "jmp advance_run" "solid_run: lodsb" "rep stosb" \
    "advance_run: or bx,bx" "jnz next_run" "pop es" "pop ds" \
    parm [dx si] [di] [bx] modify exact [ax bx cx si di];
#else
static void decode_row(const unsigned char *source,unsigned char *dest,unsigned width)
{
    while(width){
        unsigned tag=source[0]+((unsigned)source[1]<<8),count=tag&0x7fff,i;
        if(tag&0x8000){
            for(i=0;i<count;i++)dest[i]=source[2+(i&1)];
            source+=4;
        }else{memset(dest,source[2],count);source+=3;}
        dest+=count;width-=count;
    }
}
#endif

void art_open(int resolution)
{
    if(resolution==2){
        art_data=bg320;art_rows=bgrow320;art_width=320;art_height=200;
    }else if(resolution==1){
        art_data=bg800;art_rows=bgrow800;art_width=800;art_height=600;
    }else{
        art_data=bg640;art_rows=bgrow640;art_width=640;art_height=480;
    }
}
void art_row(unsigned y,unsigned char *dest)
{
    if(y>=art_height){memset(dest,0,art_width);return;}
    decode_row(art_data+art_rows[y],dest,art_width);
}
const unsigned char *art_palette(void) { return palette; }
int art_show(void)
{
    unsigned y;
    if(!art_width || video_width()!=(int)art_width || video_height()!=(int)art_height)return 0;
    for(y=0;y<art_height;y++){
        art_row(y,initial_row);
        if(!video_write_span(0,(int)y,initial_row,art_width))return 0;
    }
    return 1;
}
