/* Verify real page memory isolation and retained geometry against full redraw.
 * Intended DOSBox S3 integration test: mode12 single page and VBE102 two pages.
 * It includes the actual scene compositor; it never calls its interactive main.
 */
#define main orbit_demo_main
#include "../MAIN.C"
#undef main

static FILE *test_log;
static unsigned test_passed,test_failed;

static void expect(int good,const char *name)
{
 if(good)++test_passed;else ++test_failed;
 fprintf(test_log,"%s %s\n",good?"PASS":"FAIL",name);fflush(test_log);
}
static void test_pattern(unsigned n)
{
 unsigned j;
 for(j=0;j<16;j++){
  video_line(0,j*screen_h/16,screen_w-1,screen_h-1-j*screen_h/16,(j+n)%15+1);
  video_line(j*screen_w/16,0,screen_w-1-j*screen_w/16,screen_h-1,(j+n*3)%15+1);
 }
 video_text(13,screen_h/2,15,n?"SECOND PAGE":"FIRST PAGE",2);
 video_pixel(0,0,n+1);video_pixel(screen_w-1,screen_h-1,n+2);
}
static void isolate_pages(void)
{
 unsigned long crc0,crc1,blank;
 video_select_page(0);video_clear();blank=video_checksum();test_pattern(0);crc0=video_checksum();
 if(!buffered){
  video_select_page(1);expect(video_checksum()==crc0,"single-page selection safely aliases page zero");
  video_clear();expect(video_checksum()==blank,"single-page clear exact");return;
 }
 video_select_page(1);video_clear();expect(video_checksum()==blank,"page1 clear equals empty page0");
 test_pattern(1);crc1=video_checksum();expect(crc1!=crc0,"page1 contains its distinct pattern");
 video_select_page(0);expect(video_checksum()==crc0,"drawing page1 preserves page0");
 video_clear();expect(video_checksum()==blank,"page0 clear exact");
 video_select_page(1);expect(video_checksum()==crc1,"clearing page0 preserves page1");
 video_select_page(0);test_pattern(0);expect(video_checksum()==crc0,"page0 redraw exact");
 video_select_page(1);expect(video_checksum()==crc1,"redrawing page0 preserves page1");
 video_clear();expect(video_checksum()==blank,"page1 clear exact");
 video_select_page(0);expect(video_checksum()==crc0,"clearing page1 preserves page0");
 video_clear();
}
/* Independent list traversal: compare the retained erase/draw compositor with
 * a completely cleared page. Colour order matches intentional overlap order,
 * but this reference does not use the compositor's bucket links or old lists. */
static unsigned long full_redraw(unsigned list,unsigned long local)
{
 unsigned c,n;Line *l;
 frame();
 for(c=1;c<16;c++)for(n=0;n<line_count[list];n++){
  l=&lines[list][n];if(l->color==c)video_line(l->x0,l->y0,l->x1,l->y1,c);
 }
 if(section==4)credits(local);
 return video_checksum();
}
static void compose_frames(void)
{
 unsigned act,j,page,list;unsigned long local,actual,expected,other_before,other_after;
 char label[100];
 for(act=0;act<SCENES;act++){
  enter_scene(act);
  for(j=0;j<4;j++){
   page=draw_page;other_before=0;
   if(buffered){video_select_page(page^1);other_before=video_checksum();video_select_page(page);}
   local=37UL*j+24;
   begin_lines();
   switch(act){case 0:gravity(local,2);break;case 1:crystal(local);break;case 2:ocean(local);break;case 3:attractor(local);break;default:gravity(local,2);break;}
   /* Guaranteed old-pixel exposure as well as the actual scene geometry. */
   add_line(xs[60+j*100],ys[112],xs[75+j*100],ys[132],15);
   present();list=page_lines[page];if(act==4)credits(local);
   actual=video_checksum();expected=full_redraw(list,local);
   sprintf(label,"act%u frame%u page%u retained/full CRC %08lx/%08lx",act+1,j+1,page,actual,expected);
   expect(actual==expected,label);
   if(buffered){
    video_select_page(page^1);other_after=video_checksum();video_select_page(page);
    sprintf(label,"act%u frame%u preserves displayed page",act+1,j+1);expect(other_before==other_after,label);
   }
   video_show_page(page);
   if(buffered){draw_page^=1;video_select_page(draw_page);}
  }
 }
}
int main(void)
{
 unsigned res,i;int opened=0,have87=math_detect();
 test_log=fopen("PAGETEST.TXT","w");if(!test_log)return 2;
 fprintf(test_log,"ORBIT87 page-memory and retained-scene integration (DOSBox S3)\n");
 math_init(have87);music_start(1);music_pause(1);
 for(res=0;res<2;res++){
  opened=video_open(res);expect(opened,"video mode opens");if(!opened)continue;
  buffered=video_buffering(1);
  fprintf(test_log,"mode=%u %s buffering=%d %s\n",res,video_driver(),buffered,video_buffer_name());
  expect(buffered==(res?1:0),res?"VBE800 two-page capability detected":"VGA640 single-page fallback detected");
  if(buffered<0){video_close();continue;}
  screen_w=video_width();screen_h=video_height();cx=screen_w/2;
  for(i=0;i<=640;i++)xs[i]=(int)((unsigned long)i*screen_w/640);
  for(i=0;i<=480;i++)ys[i]=(int)((unsigned long)i*screen_h/480);
  cy=ys[257];seed=1987;detail=2;hud=0;paused=0;muted=0;
  for(i=0;i<28;i++){stars[i][0]=xs[35+random16()%570];stars[i][1]=ys[102+random16()%306];star_ink[i]=i%4?1:2;}
  isolate_pages();compose_frames();video_close();
 }
 music_stop();math_end();fprintf(test_log,"TOTAL %u passed, %u failed\n",test_passed,test_failed);fclose(test_log);return test_failed?1:0;
}
