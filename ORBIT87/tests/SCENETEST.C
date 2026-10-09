#define main orbit_demo_main
#include "../MAIN.C"
#undef main
static unsigned clock16(void){return *(volatile unsigned __far*)MK_FP(0x40,0x6c);}
int main(void){
 unsigned i,j,k,before,ticks,have=math_detect();FILE *f=fopen("SCENES.TXT","w");unsigned long crc;
 if(!f)return 1;
 if(!video_open(0)){fclose(f);return 2;}
 screen_w=640;screen_h=480;cx=320;cy=257;
 for(i=0;i<=640;i++)xs[i]=i;for(i=0;i<=480;i++)ys[i]=i;
 math_init(have);music_start(1);
 for(k=0;k<2;k++){
  section=k?2:1;video_clear();line_count[0]=line_count[1]=line_count[2]=0;which=0;draw_page=0;page_lines[0]=1;page_lines[1]=2;
  before=clock16();
  for(j=0;j<100;j++){begin_lines();if(k)ocean(j);else crystal(j);present();}
  ticks=clock16()-before;crc=video_checksum();
  fprintf(f,"%s ticks=%u crc=%08lx\n",k?"ocean":"crystal",ticks,crc);
 }
 music_stop();math_end();video_close();fclose(f);return 0;
}
