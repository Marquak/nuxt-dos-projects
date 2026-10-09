/* SUNSET DRIVE: V20 cached road projection and original-8087 setup/fallback.
 * Compile C with -1 -ms -fpc. /NO87 must bypass math_detect entirely.
 * No allocation, DOS calls, or coprocessor work occurs in an interrupt.
 */
#include "SUNSET.H"
#include <stddef.h>
#include <string.h>
#ifndef SUNSET_MATH_HOST
#include <malloc.h>
#include <dos.h>
#endif
#ifdef SUNSET_MATH_HOST
#include <math.h>
#include <stdint.h>
typedef int16_t S16;
typedef int32_t S32;
#else
typedef int S16;
typedef long S32;
#endif

#define NEAR_DEPTH 56

/* The assembler shares these byte offsets; do not change independently.
 * Vec2 is six bytes in the DOS build (one tail-padding byte).
 */
#pragma pack(push,2)
typedef struct {
    S16 m[9],scale;
    S32 distance_q14;
    double near_q14;
    unsigned short status;
} Project87;
#pragma pack(pop)
typedef char project_layout[(offsetof(Project87,distance_q14)==20 && offsetof(Project87,near_q14)==24 && offsetof(Project87,status)==32)?1:-1];
#ifndef SUNSET_MATH_HOST
typedef char vertex_layout[(sizeof(Vec3)==6 && sizeof(Vec2)==6 && offsetof(Vec2,valid)==4)?1:-1];
#endif
static Project87 projection={{0},0,0,917504.0,0};
static int active87,identity;

#ifndef SUNSET_MATH_HOST
static unsigned saved_control;
static unsigned __far *depth_table;
static int depth_scale;
static struct {long numerator;short depth;} table_args;
extern void depth_table87(void *p,unsigned seg,unsigned off);
#pragma aux depth_table87 parm [si] [dx] [di] modify exact [cx di];
extern void road_lookup(const Vec3 *,Vec2 *,unsigned,unsigned,unsigned);
#pragma aux road_lookup parm [si] [di] [cx] [dx] [bx] modify exact [ax bx cx dx si di];
extern int probe87(unsigned *p);
#pragma aux probe87 parm [si] value [ax] modify exact [ax];
extern void control87(unsigned *p);
#pragma aux control87 parm [si] modify exact [];
extern void setcontrol87(const unsigned *p);
#pragma aux setcontrol87 parm [si] modify exact [];
extern void finish87(const unsigned *p);
#pragma aux finish87 parm [si] modify exact [];
extern void project87(const Vec3 *in,Vec2 *out,unsigned count,Project87 *p);
#pragma aux project87 parm [si] [di] [cx] [bx] modify exact [];
extern void direct87(const Vec3 *in,Vec2 *out,unsigned count,Project87 *p);
#pragma aux direct87 parm [si] [di] [cx] [bx] modify exact [];
#else
/* Independent host equation model; DOS tests execute the actual assembly. */
static void project87(const Vec3 *in,Vec2 *out,unsigned count,Project87 *p)
{
    while(count--){
        double x=(double)in->x*p->m[0]+(double)in->y*p->m[1]+(double)in->z*p->m[2];
        double y=(double)in->x*p->m[3]+(double)in->y*p->m[4]+(double)in->z*p->m[5];
        double z=(double)in->x*p->m[6]+(double)in->y*p->m[7]+(double)in->z*p->m[8]+p->distance_q14;
        out->valid=z>p->near_q14;
        out->x=out->valid?(int)lrint(x*p->scale/z):0;
        out->y=out->valid?(int)lrint(y*p->scale/z):0;
        ++in;++out;
    }
}
#endif

#ifdef SUNSET_MATH_HOST
#define direct87 project87
#endif

/* Quarter sine: 256 phases per turn, amplitude 16384. Original 8087 has
 * no FSIN/FCOS. Use a tiny fixed table for camera setup. The bounded road
 * uses cached depth factors; general cameras retain full matrix projection.
 */
static const S16 sine_q14[65]={
0,402,804,1205,1606,2006,2404,2801,3196,3590,3981,4370,4756,
5140,5520,5897,6270,6639,7005,7366,7723,8076,8423,8765,9102,9434,
9760,10080,10394,10702,11003,11297,11585,11866,12140,12406,12665,
12916,13160,13395,13623,13842,14053,14256,14449,14635,14811,14978,
15137,15286,15426,15557,15679,15791,15893,15986,16069,16143,16207,
16261,16305,16340,16364,16379,16384};
static int sin14(unsigned phase)
{
    unsigned q=(phase>>6)&3,t=phase&63;
    int s=(q&1)?sine_q14[64-t]:sine_q14[t];
    return (q&2)?-s:s;
}
int math_sin(unsigned phase) { return sin14(phase)/64; }

#ifndef SUNSET_MATH_HOST
static long mul16(int a,int b);
#pragma aux mul16 = "imul bx" parm [ax] [bx] value [dx ax] modify exact [ax dx];
static int mul14(int a,int b);
#pragma aux mul14 = \
    "imul bx" "or dx,dx" "jns positive" \
    "add ax,8191" "adc dx,0" "jmp combine" \
    "positive: add ax,8192" "adc dx,0" \
    "combine: mov cl,14" "shr ax,cl" "shl dx,1" "shl dx,1" "or ax,dx" \
    parm [ax] [bx] value [ax] modify exact [ax cx dx];
#else
static long mul16(int a,int b) { return (long)a*b; }
static int round14(long n) { return (int)(n<0?-((-n+8192L)>>14):((n+8192L)>>14)); }
static int mul14(int a,int b) { return round14((long)a*b); }
#endif
/* Keep eight fractional bits in the integer projection too. Rounding to
 * whole world units before division makes nearby palm tips wobble. Q8
 * leaves safe 32-bit products for scene coordinates (abs <= 4096, scale
 * <= 1024) and stays within one pixel of the 8087 path in scene tests.
 */
static long q14_to_q8(long n)
{
    return n<0?-((-n+32L)>>6):((n+32L)>>6);
}
static long round_div(long n,long d)
{
    return n<0?-((-n+(d>>1))/d):(n+(d>>1))/d;
}

int math_detect(void)
{
#ifndef SUNSET_MATH_HOST
    unsigned scratch[2]={0x5a5a,0x037f};
    return probe87(scratch);
#else
    return 1;
#endif
}
void math_init(int use87)
{
#ifndef SUNSET_MATH_HOST
    unsigned nearest=0x037f;
    if(active87)math_end();
    if(use87){control87(&saved_control);setcontrol87(&nearest);}
#endif
    active87=use87!=0;
    math_matrix(0,0,0);
}
void math_prepare(int scale)
{
#ifndef SUNSET_MATH_HOST
    unsigned z;
    if(scale<1||scale>900)return;
    if(depth_table&&depth_scale==scale)return;
    if(!depth_table)depth_table=_fmalloc(4096U);
    if(!depth_table)return;
    table_args.numerator=(long)scale*2048L;
    _fmemset(depth_table,0,4096U);
    if(active87)depth_table87(&table_args,FP_SEG(depth_table),FP_OFF(depth_table)+114);
    else for(z=57;z<2048;z++)depth_table[z]=(unsigned)((table_args.numerator+z/2)/z);
    depth_scale=scale;_fheapshrink();
#else
    (void)scale;
#endif
}
int math_cached(void)
{
#ifndef SUNSET_MATH_HOST
    return depth_table!=0;
#else
    return 0;
#endif
}
void math_end(void)
{
#ifndef SUNSET_MATH_HOST
    if(active87)finish87(&saved_control);
    if(depth_table){_ffree(depth_table);depth_table=0;depth_scale=0;_fheapshrink();}
#endif
    active87=0;
}
int math_uses87(void) { return active87; }

void math_matrix(unsigned yaw,unsigned pitch,unsigned roll)
{
    int cy=sin14(yaw+64),sy=sin14(yaw),cp=sin14(pitch+64),sp=sin14(pitch);
    int cr=sin14(roll+64),sr=sin14(roll),a,b;
    identity=((yaw|pitch|roll)&255)==0;
    /* Rz(roll) * Rx(pitch) * Ry(yaw), coefficients in Q14. */
    a=mul14(sp,sy);b=mul14(sp,cy);
    projection.m[0]=mul14(cr,cy)-mul14(sr,a);
    projection.m[1]=-mul14(sr,cp);
    projection.m[2]=mul14(cr,sy)+mul14(sr,b);
    projection.m[3]=mul14(sr,cy)+mul14(cr,a);
    projection.m[4]=mul14(cr,cp);
    projection.m[5]=mul14(sr,sy)-mul14(cr,b);
    projection.m[6]=-mul14(cp,sy);
    projection.m[7]=sp;
    projection.m[8]=mul14(cp,cy);
}
void math_project(const Vec3 *in,Vec2 *out,unsigned count,int scale,int distance)
{
    long x,y,z;
    if(!count)return;
    projection.scale=scale;
    projection.distance_q14=(long)distance*16384L;
    /* The sunset camera faces straight down the road. Prefer cached depth
     * factors; direct projection remains available outside the cache domain. */
    if(identity&&!distance){
#ifndef SUNSET_MATH_HOST
        if(depth_table&&depth_scale==scale){
            unsigned n=count;const Vec3 *v=in;
            /* General callers retain exact projection outside the bounded
             * road domain. This scan also prevents far-table overreads. */
            while(n&&v->z<2048&&v->x>=-2047&&v->x<=2047&&v->y>=-2047&&v->y<=2047){n--;v++;}
            if(!n){road_lookup(in,out,count,FP_SEG(depth_table),FP_OFF(depth_table));return;}
        }
#endif
        if(active87){direct87(in,out,count,&projection);return;}
        while(count--){
            z=in->z;out->valid=z>NEAR_DEPTH;
            if(out->valid){
                out->x=(int)round_div(mul16(in->x,scale),z);
                out->y=(int)round_div(mul16(in->y,scale),z);
            }else{out->x=0;out->y=0;}
            ++in;++out;
        }
        return;
    }
    if(active87){project87(in,out,count,&projection);return;}
    while(count--){
        z=mul16(in->x,projection.m[6])+mul16(in->y,projection.m[7])+mul16(in->z,projection.m[8])+projection.distance_q14;
        out->valid=z>NEAR_DEPTH*16384L;
        if(out->valid){
            x=q14_to_q8(mul16(in->x,projection.m[0])+mul16(in->y,projection.m[1])+mul16(in->z,projection.m[2]));
            y=q14_to_q8(mul16(in->x,projection.m[3])+mul16(in->y,projection.m[4])+mul16(in->z,projection.m[5]));
            z=q14_to_q8(z);
            out->x=(int)round_div(x*scale,z);
            out->y=(int)round_div(y*scale,z);
        }else{out->x=0;out->y=0;}
        ++in;++out;
    }
}
