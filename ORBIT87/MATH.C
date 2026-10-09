/* ORBIT 87 maths. C is compiled -0 -ms -fpc: NO implicit x87 operations.
 * MATH87.ASM supplies original-8087 kernels, entered only after detection.
 * /NO87 must bypass math_detect and call math_init(0).
 * State is small; no allocation, DOS calls or coprocessor work in an ISR.
 */
#include "ORBIT.H"
#include <string.h>
#include <stddef.h>
#ifdef ORBIT_MATH_HOST
#include <math.h>
#include <stdint.h>
typedef int16_t S16;
typedef int32_t S32;
#else
typedef int S16;
typedef long S32;
#endif

#define PARTICLES 48
#define Q14 16384L
#define GM 500L

/* Fixed layouts shared with MATH87.ASM; test compilation checks offsets. */
#pragma pack(push,2)
typedef struct {
    S16 m[9],scale,distance,one;
    S32 distance_q14;
    double denominator;
    S16 depth;
} Project87;
typedef struct {
    S32 minus_gm_q12;
    S16 soft,pad;
    double r2,factor;
} Gravity87;
typedef struct {
    double x,y,z,sigma_dt,rho,dt,dx,dy,beta,dz;
    S16 scale,zcenter;
} Lorenz87;
#pragma pack(pop)
typedef char project_layout[(offsetof(Project87,denominator)==28 && offsetof(Project87,depth)==36)?1:-1];
typedef char gravity_layout[(offsetof(Gravity87,r2)==8 && offsetof(Gravity87,factor)==16)?1:-1];
typedef char lorenz_layout[(offsetof(Lorenz87,scale)==80)?1:-1];
static Project87 projection;
static Gravity87 gravity={-GM*4096L,400,0,0.0,0.0};
static const Lorenz87 lorenz_initial={1.0,1.0,1.0,0.06,28.0,0.006,0.0,0.0,8.0/3.0,0.0,5,24};
static Lorenz87 lorenz;
static S32 lx,ly,lz;
static S32 px[PARTICLES],py[PARTICLES],pz[PARTICLES];
static S32 vx[PARTICLES],vy[PARTICLES],vz[PARTICLES];
static int active87;
#ifndef ORBIT_MATH_HOST
static unsigned saved_control;
#endif

#ifndef ORBIT_MATH_HOST
extern int probe87(unsigned *p);
#pragma aux probe87 parm [si] value [ax] modify exact [ax];
extern void control87(unsigned *p);
#pragma aux control87 parm [si] modify exact [];
extern void setcontrol87(const unsigned *p);
#pragma aux setcontrol87 parm [si] modify exact [];
extern void finish87(const unsigned *p);
#pragma aux finish87 parm [si] modify exact [];
extern void project87(const Vec3 *in,Vec2 *out,Project87 *p);
#pragma aux project87 parm [si] [di] [bx] modify exact [];
extern void gravity87(const Vec3 *p,Vec3 *a,Gravity87 *g);
#pragma aux gravity87 parm [si] [di] [bx] modify exact [];
extern void attractor87(Lorenz87 *l,Vec3 *out);
#pragma aux attractor87 parm [si] [di] modify exact [];
#else
/* Host-only independent execution of the same equations for long-run tests. */
static void project87(const Vec3 *v,Vec2 *out,Project87 *p)
{
    double x=(double)v->x*p->m[0]+(double)v->y*p->m[1]+(double)v->z*p->m[2];
    double y=(double)v->x*p->m[3]+(double)v->y*p->m[4]+(double)v->z*p->m[5];
    double z=(double)v->x*p->m[6]+(double)v->y*p->m[7]+(double)v->z*p->m[8]+p->distance_q14;
    out->valid=(lrint(z/16384.0)>16);
    out->x=out->valid?(int)lrint(x*p->scale/z):0;
    out->y=out->valid?(int)lrint(y*p->scale/z):0;
}
static void gravity87(const Vec3 *p,Vec3 *a,Gravity87 *g)
{
    double r2=(double)p->x*p->x+(double)p->y*p->y+(double)p->z*p->z+g->soft;
    double factor=g->minus_gm_q12/(r2*sqrt(r2));
    a->x=(int)lrint(p->x*factor);a->y=(int)lrint(p->y*factor);a->z=(int)lrint(p->z*factor);
}
static void attractor87(Lorenz87 *l,Vec3 *out)
{
    l->dx=(l->y-l->x)*l->sigma_dt;
    l->dy=(l->x*(l->rho-l->z)-l->y)*l->dt;
    l->dz=(l->x*l->y-l->z*l->beta)*l->dt;
    l->x+=l->dx;l->y+=l->dy;l->z+=l->dz;
    out->x=(int)lrint(l->x*l->scale);out->y=(int)lrint(l->y*l->scale);
    out->z=(int)lrint((l->z-l->zcenter)*l->scale);
}
#endif

/* Quarter sine, amplitude 16384. No run-time trig or expensive table build.
 * Angle tables are shared by both paths; the 8087 performs live numerical
 * integration and projection, where its precision has visible value. */
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

/* These integer hot paths use one native 16x16 IMUL instead of Watcom's
 * general 32x32 multiply helper. Rounded power-of-two divides combine the
 * product's two words, avoiding software long division on an 8088/V20. */
#ifndef ORBIT_MATH_HOST
static long mul16(int a,int b);
#pragma aux mul16 = "imul bx" parm [ax] [bx] value [dx ax] modify exact [ax dx];
static int mul14(int a,int b);
#pragma aux mul14 = \
    "imul bx" "or dx,dx" "jns positive" \
    "add ax,8191" "adc dx,0" "jmp combine" \
    "positive: add ax,8192" "adc dx,0" \
    "combine: mov cl,14" "shr ax,cl" "shl dx,1" "shl dx,1" "or ax,dx" \
    parm [ax] [bx] value [ax] modify exact [ax cx dx];
static int round14(long n);
#pragma aux round14 = \
    "or dx,dx" "jns positive" \
    "add ax,8191" "adc dx,0" "jmp combine" \
    "positive: add ax,8192" "adc dx,0" \
    "combine: mov cl,14" "shr ax,cl" "shl dx,1" "shl dx,1" "or ax,dx" \
    parm [dx ax] value [ax] modify exact [ax cx dx];
static int round12(long n);
#pragma aux round12 = \
    "or dx,dx" "jns positive" \
    "add ax,2047" "adc dx,0" "jmp combine" \
    "positive: add ax,2048" "adc dx,0" \
    "combine: mov cl,12" "shr ax,cl" \
    "shl dx,1" "shl dx,1" "shl dx,1" "shl dx,1" "or ax,dx" \
    parm [dx ax] value [ax] modify exact [ax cx dx];
#else
static long mul16(int a,int b) { return (long)a*b; }
static int round14(long n) { return (int)(n<0?-((-n+8192L)>>14):((n+8192L)>>14)); }
static int round12(long n) { return (int)(n<0?-((-n+2048L)>>12):((n+2048L)>>12)); }
static int mul14(int a,int b) { return round14((long)a*b); }
#endif
/* Positive denominator throughout. Shifting d avoids a second long divide. */
static long round_div(long n,long d)
{
    return n<0?-((-n+(d>>1))/d):(n+(d>>1))/d;
}
static unsigned long root32(unsigned long n)
{
    unsigned long root=0,bit=0x40000000UL;
    while(bit>n)bit>>=2;
    while(bit){
        if(n>=root+bit){n-=root+bit;root=(root>>1)+bit;}
        else root>>=1;
        bit>>=2;
    }
    return root;
}
int math_detect(void)
{
#ifndef ORBIT_MATH_HOST
    unsigned scratch[2]={0x5a5a,0x037f};
    return probe87(scratch);
#else
    return 1;
#endif
}
void math_init(int use87)
{
#ifndef ORBIT_MATH_HOST
    unsigned nearest=0x037f;
    if(active87)math_end();
    if(use87){control87(&saved_control);setcontrol87(&nearest);}
#endif
    active87=use87!=0;
    math_matrix(0,0,0);
    math_gravity_reset();math_attractor_reset();
}
void math_end(void)
{
#ifndef ORBIT_MATH_HOST
    if(active87)finish87(&saved_control);
#endif
    active87=0;
}
int math_uses87(void) { return active87; }

void math_matrix(unsigned yaw,unsigned pitch,unsigned roll)
{
    int cy=sin14(yaw+64),sy=sin14(yaw),cp=sin14(pitch+64),sp=sin14(pitch);
    int cr=sin14(roll+64),sr=sin14(roll),a,b;
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
    projection.scale=scale;projection.distance=distance;projection.one=16384;
    projection.distance_q14=(long)distance*16384L;
    while(count--){
        if(active87)project87(in,out,&projection);
        else{
            x=round14(mul16(in->x,projection.m[0])+mul16(in->y,projection.m[1])+mul16(in->z,projection.m[2]));
            y=round14(mul16(in->x,projection.m[3])+mul16(in->y,projection.m[4])+mul16(in->z,projection.m[5]));
            z=distance+round14(mul16(in->x,projection.m[6])+mul16(in->y,projection.m[7])+mul16(in->z,projection.m[8]));
            out->valid=z>16;
            out->x=out->valid?(int)round_div(x*scale,z):0;
            out->y=out->valid?(int)round_div(y*scale,z):0;
        }
        ++in;++out;
    }
}
void math_gravity_reset(void)
{
    unsigned i,phase;int r,s,c,si,ci;long v;
    for(i=0;i<PARTICLES;i++){
        phase=i*97U;r=56+i*3;
        s=math_sin(phase);c=math_sin(phase+64);
        si=math_sin(i*31U);ci=math_sin(i*31U+64);
        px[i]=(long)r*c*16L;py[i]=(long)r*s*ci/16L;pz[i]=(long)r*s*si/16L;
        v=(long)root32((GM*65536UL)/r)*16L;
        vx[i]=-v*s/256L;vy[i]=v*c*ci/65536L;vz[i]=v*c*si/65536L;
    }
}
void math_gravity_step(Vec3 *out,unsigned count)
{
    unsigned i;Vec3 p,a;long r2,den;
    if(count>PARTICLES)count=PARTICLES;
    for(i=0;i<count;i++){
        p.x=(int)round12(px[i]);p.y=(int)round12(py[i]);p.z=(int)round12(pz[i]);
        if(active87)gravity87(&p,&a,&gravity);
        else{
            r2=mul16(p.x,p.x)+mul16(p.y,p.y)+mul16(p.z,p.z)+400L;
            den=(long)root32((unsigned long)r2)*r2;
            a.x=(int)round_div(-GM*4096L*p.x,den);
            a.y=(int)round_div(-GM*4096L*p.y,den);
            a.z=(int)round_div(-GM*4096L*p.z,den);
        }
        /* Symplectic Euler: kick then drift bounds orbital energy better
         * than advancing position from the old velocity. Shared Q12 state
         * keeps the per-particle memory footprint modest on an XT. */
        vx[i]+=a.x;vy[i]+=a.y;vz[i]+=a.z;
        px[i]+=vx[i];py[i]+=vy[i];pz[i]+=vz[i];
        out[i].x=(int)round12(px[i]);
        out[i].y=(int)round12(py[i]);
        out[i].z=(int)round12(pz[i]);
    }
}
void math_attractor_reset(void)
{
    memcpy(&lorenz,&lorenz_initial,sizeof(lorenz));lx=ly=lz=256L;
}
void math_attractor_step(Vec3 *out,unsigned count)
{
    long dx,dy,dz;
    while(count--){
        if(active87)attractor87(&lorenz,out);
        else{
            dx=(ly-lx)*3L/50L;
            dy=(lx*(28L*256L-lz)/256L-ly)*3L/500L;
            dz=(lx*ly/256L-lz*8L/3L)*3L/500L;
            lx+=dx;ly+=dy;lz+=dz;
            out->x=(int)round_div(lx*5L,256L);
            out->y=(int)round_div(ly*5L,256L);
            out->z=(int)round_div((lz-24L*256L)*5L,256L);
        }
        ++out;
    }
}
