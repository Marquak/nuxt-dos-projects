#include <stdio.h>
#include <string.h>
#include "../FPU.H"

static void get_control(unsigned *p);
#pragma aux get_control = 0xd9 0x3c parm [si] modify exact [];
static void set_control(const unsigned *p);
#pragma aux set_control = 0x9b 0xd9 0x2c parm [si] modify exact [];

int main(void)
{
    unsigned char cpu[64], fpu[64];
    int found,i,errors=0;
    unsigned before,after,downward=0x077f;
    FILE *log=fopen("FPUTEST.TXT","w");
    if(!log)return 2;
    found=matrix_fpu_detect();
    fprintf(log,"x87 detected: %d\n",found);
    matrix_glow_table(cpu,0);
    for(i=0;i<64;i++){
        if(cpu[i]>8)errors++;
        if(i&&cpu[i]!=cpu[64-i])errors++;
        if(i<32&&cpu[i]>cpu[i+1])errors++;
    }
    if(cpu[0]!=0||cpu[32]!=8)errors++;
    if(found){
        get_control(&before);set_control(&downward);
        matrix_glow_table(fpu,1);
        get_control(&after);
        if(after!=downward)errors++;
        set_control(&before);
        if(memcmp(cpu,fpu,64))errors++;
        fprintf(log,"Integer/x87 identical: %s\n",memcmp(cpu,fpu,64)?"NO":"YES");
        fprintf(log,"Control-word restored: %s\n",after==downward?"YES":"NO");
    }
    for(i=0;i<64;i++)fprintf(log,"%u%s",cpu[i],i==63?"\n":",");
    fprintf(log,"Errors: %d\n",errors);
    fclose(log);
    printf("FPU test: detected=%d errors=%d\n",found,errors);
    return errors?1:0;
}
