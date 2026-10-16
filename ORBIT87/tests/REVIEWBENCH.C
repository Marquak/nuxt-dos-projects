/* Identical 60-frame scene workloads, with retained geometry and palette.
 * Timings are emulator workloads, not a physical V20 FPS measurement. */
#define main orbit_demo_main
#include "../MAIN.C"
#undef main
static unsigned clock16(void){return *(volatile unsigned __far*)MK_FP(0x40,0x6c);}
int main(void){
 unsigned i,j,k,start,duration;FILE *f=fopen("BENCH.TXT","w");
 if(!f||!video_open(0))return 1;
 screen_w=640;screen_h=480;cx=320;cy=257;
 for(i=0;i<=640;i++)xs[i]=i;for(i=0;i<=480;i++)ys[i]=i;
 math_init(math_detect());music_start(1);
 for(k=0;k<5;k++){
  enter_scene(k);start=clock16();
  for(j=0;j<60;j++){
   begin_lines();
   switch(k){case 0:gravity(j,1);break;case 1:crystal(j);break;case 2:ocean(j);break;case 3:attractor(j);break;default:gravity(j,1);break;}
   present();if(k==4)credits(j);scene_palette(j);
  }
  duration=clock16()-start;
  fprintf(f,"scene%u ticks=%u crc=%08lx\n",k+1,duration,video_checksum());fflush(f);
 }
 music_stop();math_end();video_close();fclose(f);return 0;
}
