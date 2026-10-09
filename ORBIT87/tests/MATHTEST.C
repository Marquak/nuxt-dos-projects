/* Standalone numerical/instruction smoke test. No graphics or sound.
 * Build with MATH.C and MATH87.OBJ, using the main demo's 8086 flags.
 * MATHTEST /NO87 never probes or calls a coprocessor instruction.
 * Host: cc -x c -DORBIT_MATH_HOST -O2 ../MATH.C MATHTEST.C -lm -o mathtest
 */
#include "../ORBIT.H"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef ORBIT_MATH_HOST
static void getword(unsigned *p);
#pragma aux getword = 0x9b 0xd9 0x3c 0x9b parm [si] modify exact [];
static void setword(const unsigned *p);
#pragma aux setword = 0x9b 0xd9 0x2c parm [si] modify exact [];
static void statusword(unsigned *p);
#pragma aux statusword = 0x9b 0xdd 0x3c 0x9b parm [si] modify exact [];
#endif
static Vec3 input[48],particles[48],trail[48];
static Vec2 output[48],reference[48];
static unsigned errors;
static unsigned long checks;
static FILE *logfile;
#define CHECK(c) do { ++checks; if(!(c)) { ++errors; if(errors<25) fprintf(logfile,"FAIL line %u: %s\n",(unsigned)__LINE__,#c); } } while(0)

static void basics(int mode)
{
    unsigned i,t;int radius_max=0,orbit_moved=0,attractor_max=0,left=0,right=0;
    Vec3 start;
    math_init(mode);CHECK(math_uses87()==mode);
    CHECK(math_sin(0)==0);CHECK(math_sin(64)==256);
    CHECK(math_sin(128)==0);CHECK(math_sin(192)==-256);
    for(i=0;i<256;i++){
        CHECK(math_sin(i)==-math_sin(i+128));
        CHECK(abs(math_sin(i))<=256);
    }
    input[0].x=100;input[0].y=-50;input[0].z=0;
    math_matrix(0,0,0);math_project(input,output,1,360,500);
    CHECK(output[0].valid);CHECK(output[0].x==72);CHECK(output[0].y==-36);
    input[0].x=0;input[0].y=0;input[0].z=-500;
    math_project(input,output,1,360,500);CHECK(!output[0].valid);
    CHECK(output[0].x==0 && output[0].y==0);
    input[0].z=-600;math_project(input,output,1,360,500);CHECK(!output[0].valid);
    input[0].x=100;input[0].z=0;
    math_matrix(64,0,0);math_project(input,output,1,360,500);
    CHECK(output[0].valid);CHECK(output[0].x==0);CHECK(output[0].y==0);
    for(i=0;i<48;i++){
        input[i].x=(int)(i*71U%401U)-200;
        input[i].y=(int)(i*37U%401U)-200;
        input[i].z=(int)(i*19U%401U)-200;
    }
    /* Repeated rotations test the FPU stack, divide and range together. */
    for(t=0;t<256;t++){
        math_matrix(t,t*3U,t*5U);math_project(input,output,48,360,500);
        for(i=0;i<48;i++){
            CHECK(output[i].valid);
            CHECK(abs(output[i].x)<1000 && abs(output[i].y)<1000);
        }
    }
    math_gravity_reset();math_gravity_step(particles,48);start=particles[0];
    for(t=0;t<2000;t++){
        math_gravity_step(particles,48);
        for(i=0;i<48;i++){
            int m=abs(particles[i].x)+abs(particles[i].y)+abs(particles[i].z);
            if(m>radius_max)radius_max=m;
            CHECK(m<550);
        }
    }
    orbit_moved=abs(start.x-particles[0].x)+abs(start.y-particles[0].y)+abs(start.z-particles[0].z);
    CHECK(orbit_moved>5);
    math_attractor_reset();
    for(t=0;t<1000;t++){
        math_attractor_step(trail,48);
        for(i=0;i<48;i++){
            int m=abs(trail[i].x)+abs(trail[i].y)+abs(trail[i].z);
            if(m>attractor_max)attractor_max=m;
            if(trail[i].x < -20)left=1;
            if(trail[i].x > 20)right=1;
            CHECK(abs(trail[i].x)<150 && abs(trail[i].y)<200 && abs(trail[i].z)<170);
        }
    }
    CHECK(left && right);
#ifndef ORBIT_MATH_HOST
    if(mode){unsigned status;statusword(&status);CHECK((status&0x3841)==0);fprintf(logfile,"8087 status %04X (TOP=0, no invalid/stack error required)\n",status);}
#endif
    fprintf(logfile,"%s: gravity max Manhattan radius %d, Lorenz %d, orbit movement %d\n",mode?"8087":"integer",radius_max,attractor_max,orbit_moved);
    math_end();CHECK(!math_uses87());
}
static void compare_projection(void)
{
    unsigned i,t;int max_error=0;
    for(t=0;t<256;t+=7){
        math_init(0);math_matrix(t,t*3U,t*5U);math_project(input,reference,48,360,500);math_end();
        math_init(1);math_matrix(t,t*3U,t*5U);math_project(input,output,48,360,500);math_end();
        for(i=0;i<48;i++){
            int e=abs(output[i].x-reference[i].x)+abs(output[i].y-reference[i].y);
            if(e>max_error)max_error=e;
            CHECK(output[i].valid==reference[i].valid);CHECK(e<=4);
        }
    }
    fprintf(logfile,"Integer/8087 maximum projection Manhattan difference: %d pixels\n",max_error);
}
int main(int argc,char **argv)
{
    int found=0,skip=argc>1 && !strcmp(argv[1],"/NO87");
    logfile=fopen("MATHTEST.TXT","w");if(!logfile)return 2;
    if(!skip)found=math_detect();
    fprintf(logfile,"8087 detected=%d, forced integer=%d\n",found,skip);
    basics(0);
    if(found){
#ifndef ORBIT_MATH_HOST
        unsigned before,after,down=0x077f;
        getword(&before);setword(&down);
        math_init(1);math_attractor_step(trail,48);math_end();getword(&after);
        CHECK(after==down);setword(&before);
#endif
        basics(1);compare_projection();
    }
    fprintf(logfile,"Checks=%lu errors=%u\n",checks,errors);
    fclose(logfile);printf("Math test: detected=%d errors=%u (MATHTEST.TXT)\n",found,errors);
    return errors?1:0;
}
