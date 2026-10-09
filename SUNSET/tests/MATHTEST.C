/* DOS smoke/soak test: link MATH.C + MATH87.OBJ using -1 -fpc.
 * MATHTEST /NO87 bypasses even detection. Host equation model:
 * cc -x c -DSUNSET_MATH_HOST -O2 ../MATH.C MATHTEST.C -lm -o mathtest
 */
#include "../SUNSET.H"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef SUNSET_MATH_HOST
static void getword(unsigned *p);
#pragma aux getword = 0x9b 0xd9 0x3c 0x9b parm [si] modify exact [];
static void setword(const unsigned *p);
#pragma aux setword = 0x9b 0xd9 0x2c parm [si] modify exact [];
/* Original 8087 14-byte environment: CW, SW, tag, instruction/data ptrs.
 * FSTENV masks exceptions, so reload its saved CW before returning.
 */
static void getenvironment(unsigned *p);
#pragma aux getenvironment = 0x9b 0xd9 0x34 0x9b 0xd9 0x2c 0x9b parm [si] modify exact [];
#endif
#define VERTICES 384
static Vec3 input[VERTICES];
static Vec2 output[VERTICES],reference[VERTICES];
static unsigned errors;
static unsigned long checks;
static FILE *logfile;
#define CHECK(c) do { ++checks; if(!(c)) { ++errors; if(errors<25) fprintf(logfile,"FAIL line %u: %s\n",(unsigned)__LINE__,#c); } } while(0)

static void check_stack(void)
{
#ifndef SUNSET_MATH_HOST
    unsigned env[7];
    getenvironment(env);
    CHECK((env[1]&0x3841)==0); /* TOP zero; no invalid or stack fault */
    CHECK(env[2]==0xffff);   /* all eight registers tagged empty */
#endif
}
static void fill_palms(unsigned tick)
{
    unsigned i;
    for(i=0;i<VERTICES;i++){
        input[i].x=(i&1)?220:-220;
        input[i].x+=(int)(i*17U%97U)-48;
        input[i].y=(int)(i*37U%231U)-60;
        input[i].z=80+(int)((i*211U+tick*17U)%1720U);
    }
}
static void basics(int mode)
{
    unsigned i,t;
    math_init(mode);CHECK(math_uses87()==mode);
    CHECK(math_sin(0)==0);CHECK(math_sin(64)==256);
    CHECK(math_sin(128)==0);CHECK(math_sin(192)==-256);
    for(i=0;i<256;i++){
        CHECK(math_sin(i)==-math_sin(i+128));
        CHECK(abs(math_sin(i))<=256);
    }
    input[0].x=220;input[0].y=110;input[0].z=400;
    input[1].x=-220;input[1].y=-110;input[1].z=800;
    math_project(input,output,2,200,0);
    CHECK(output[0].valid && output[0].x==110 && output[0].y==55);
    CHECK(output[1].valid && output[1].x==-55);
    CHECK(output[1].y==-28); /* tie rounds to nearest-even or away: both -28 */
    for(i=0;i<6;i++){
        input[i].x=57;input[i].y=-57;input[i].z=53+(int)i;
    }
    math_project(input,output,6,200,0);
    for(i=0;i<4;i++)CHECK(!output[i].valid && !output[i].x && !output[i].y);
    CHECK(output[4].valid && output[4].x==200 && output[4].y==-200);
    CHECK(output[5].valid);
    if(mode)check_stack();
    output[0].x=123;output[0].y=-456;output[0].valid=91;
    math_project(input,output,0,200,0);
    CHECK(output[0].x==123 && output[0].y==-456 && output[0].valid==91);
    input[0].x=100;input[0].y=0;input[0].z=0;
    math_matrix(64,0,0);math_project(input,output,1,200,500);
    CHECK(output[0].valid && output[0].x==0 && output[0].y==0);
    /* Roadside geometry streams towards the camera; frame-to-frame camera
     * rotations cover positive and negative angles and near clipping.
     */
    for(t=0;t<256;t++){
        fill_palms(t);
        math_matrix((unsigned)(math_sin(t)/80),(unsigned)(math_sin(t*3U)/128),0);
        math_project(input,output,VERTICES,224,0);
        for(i=0;i<VERTICES;i++){
            if(output[i].valid){
                CHECK(abs(output[i].x)<1400 && abs(output[i].y)<1100);
            }else CHECK(!output[i].x && !output[i].y);
        }
        if(mode && !(t&31))check_stack();
    }
    /* General matrix exercises all coefficients, with translation keeping
     * every point ahead of the near plane through a complete revolution.
     */
    for(i=0;i<VERTICES;i++){
        input[i].x=(int)(i*71U%401U)-200;
        input[i].y=(int)(i*37U%401U)-200;
        input[i].z=(int)(i*19U%401U)-200;
    }
    for(t=0;t<128;t++){
        math_matrix(t*2U,t*6U,t*10U);
        math_project(input,output,VERTICES,224,500);
        for(i=0;i<VERTICES;i++)CHECK(output[i].valid && abs(output[i].x)<600 && abs(output[i].y)<600);
    }
    if(mode)check_stack();
    math_end();CHECK(!math_uses87());
    fprintf(logfile,"%s: 256 streamed palm batches + 128 general rotation batches passed\n",mode?"8087":"integer");
}
static void compare_projection(void)
{
    unsigned i,t;int max_error=0;
    for(t=0;t<128;t+=3){
        fill_palms(t);
        math_init(0);math_matrix((unsigned)(math_sin(t)/80),(unsigned)(math_sin(t*3U)/128),0);
        math_project(input,reference,VERTICES,224,0);math_end();
        math_init(1);math_matrix((unsigned)(math_sin(t)/80),(unsigned)(math_sin(t*3U)/128),0);
        math_project(input,output,VERTICES,224,0);
        check_stack();math_end();
        for(i=0;i<VERTICES;i++){
            int e=abs(output[i].x-reference[i].x)+abs(output[i].y-reference[i].y);
            if(e>max_error)max_error=e;
            CHECK(output[i].valid==reference[i].valid);CHECK(e<=2);
        }
    }
    fprintf(logfile,"Integer/8087 maximum projection Manhattan difference: %d pixels\n",max_error);
}
static void compare_identity(void)
{
    unsigned i,t;
    /* Exercise the exact no-rotation path used in every sunset frame, with
     * a full batch and negative/near-plane depths as well as live palms. */
    for(t=0;t<64;t++){
        fill_palms(t);
        input[0].z=-32768;input[1].z=-1;input[2].z=0;
        input[3].z=56;input[4].z=57;
        math_init(0);math_project(input,reference,VERTICES,537,0);math_end();
        math_init(1);math_project(input,output,VERTICES,537,0);
        check_stack();math_end();
        for(i=0;i<VERTICES;i++){
            CHECK(output[i].valid==reference[i].valid);
            CHECK(abs(output[i].x-reference[i].x)<=1);
            CHECK(abs(output[i].y-reference[i].y)<=1);
        }
    }
    fprintf(logfile,"Identity-camera fast path: 64 full batches passed\n");
}
static long rounded(long n,long z){return n<0?-((-n+z/2)/z):(n+z/2)/z;}
static void cached_road(int mode)
{
#ifndef SUNSET_MATH_HOST
    unsigned scales[3]={215,430,537},s,start,i,n;int ex,ey;
    for(s=0;s<3;s++){
        math_init(mode);math_prepare(scales[s]);CHECK(math_cached());
        for(start=53;start<2048;start+=VERTICES){
            n=2048-start;if(n>VERTICES)n=VERTICES;
            for(i=0;i<n;i++){
                unsigned z=start+i;
                input[i].x=(int)(z*17U%4095U)-2047;
                input[i].y=(int)(z*31U%4095U)-2047;
                input[i].z=z;
            }
            math_project(input,output,n,scales[s],0);
            for(i=0;i<n;i++){
                CHECK(output[i].valid==(input[i].z>56));
                if(input[i].z<=56){CHECK(!output[i].x&&!output[i].y);continue;}
                ex=(int)rounded((long)input[i].x*scales[s],input[i].z);
                ey=(int)rounded((long)input[i].y*scales[s],input[i].z);
                CHECK(abs(output[i].x-ex)<=1);CHECK(abs(output[i].y-ey)<=1);
            }
        }
        input[0].x=3000;input[0].y=-2000;input[0].z=2048;
        math_project(input,output,1,scales[s],0);
        CHECK(abs(output[0].x-(int)rounded(3000L*scales[s],2048))<=1);
        if(mode)check_stack();math_end();CHECK(!math_cached());
    }
    fprintf(logfile,"%s depth cache: every depth, all three scales within one pixel; bounds fallback passed\n",mode?"8087":"integer");
#else
    (void)mode;
#endif
}
int main(int argc,char **argv)
{
    int found=0,skip=argc>1 && !strcmp(argv[1],"/NO87");
    logfile=fopen("MATHTEST.TXT","w");if(!logfile)return 2;
    if(!skip)found=math_detect();
    fprintf(logfile,"8087 detected=%d, forced integer=%d\n",found,skip);
    basics(0);cached_road(0);
    if(found){
#ifndef SUNSET_MATH_HOST
        unsigned before,after,down=0x077f;
        getword(&before);setword(&down);
        CHECK(math_detect());getword(&after);CHECK(after==down);
        math_init(1);fill_palms(0);math_project(input,output,VERTICES,224,0);
        check_stack();math_end();getword(&after);
        CHECK(after==down);setword(&before);
#endif
        basics(1);compare_projection();compare_identity();cached_road(1);
    }
    fprintf(logfile,"Checks=%lu errors=%u\n",checks,errors);
    fclose(logfile);printf("Math test: detected=%d errors=%u (MATHTEST.TXT)\n",found,errors);
    return errors?1:0;
}
