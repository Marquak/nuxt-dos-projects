/* Exact extracted MAIN.C helper; standalone test of real DOS machine code. */
#include <stdio.h>
static int mul256(int a,int b);
#pragma aux mul256 = \
 "imul bx" "test dx,dx" "jns positive" "add ax,255" "adc dx,0" \
 "positive: mov al,ah" "mov ah,dl" \
 parm [ax] [bx] value [ax] modify exact [ax dx];
static unsigned errors;
static unsigned long checks;
static FILE *f;
static void test(int a,int b)
{
 int got=mul256(a,b),want=(int)(((long)a*b)/256L);
 ++checks;
 if(got!=want){++errors;if(errors<20)fprintf(f,"FAIL %d*%d /256: asm=%d C=%d\n",a,b,got,want);}
}
int main(void)
{
 static const int av[]={-1024,-1023,-769,-768,-767,-513,-512,-511,-257,-256,-255,-129,-128,-127,-65,-64,-63,-2,-1,0,1,2,63,64,65,127,128,129,255,256,257,511,512,513,767,768,769,1023,1024};
 static const int bv[]={-256,-255,-254,-129,-128,-127,-65,-64,-63,-2,-1,0,1,2,63,64,65,127,128,129,254,255,256};
 unsigned i,j,seed=0x8087;int a,b;
 f=fopen("MATMUL.TXT","w");if(!f)return 2;
 for(i=0;i<sizeof(av)/sizeof(av[0]);i++)for(j=0;j<sizeof(bv)/sizeof(bv[0]);j++)test(av[i],bv[j]);
 for(a=-1024;a<=1024;a+=31)for(b=-256;b<=256;b+=11)test(a,b);
 for(i=0;i<2048;i++){
  seed=seed*25173U+13849U;a=(int)(seed%2049U)-1024;
  seed=seed*25173U+13849U;b=(int)(seed%513U)-256;
  test(a,b);
 }
 fprintf(f,"Exact MAIN.C mul256 vs C signed long multiply/divide\nChecks=%lu errors=%u\n",checks,errors);
 fclose(f);return errors?1:0;
}
