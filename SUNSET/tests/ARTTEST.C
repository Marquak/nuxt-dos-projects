/* Decode every pixel to raw indexed images for independent PNG comparison.
 * Build with ART.C only; video stubs verify initial-image span submission.
 */
#include "../SUNSET.H"
#include <stdio.h>
#include <string.h>

static unsigned char row[802],shown[800];
static int width,height,fail_at=-1;
static unsigned next_y,errors;
static unsigned long checks,pixels;
static FILE *logfile;
#define CHECK(c) do { ++checks; if(!(c)) { ++errors; if(errors<25) fprintf(logfile,"FAIL line %u: %s\n",(unsigned)__LINE__,#c); } } while(0)
int video_width(void){return width;}
int video_height(void){return height;}
int video_write_span(int x,int y,const unsigned char *data,unsigned count)
{
    CHECK(x==0 && y==(int)next_y && count==(unsigned)width);
    if(y==fail_at)return 0;
    art_row((unsigned)y,shown);
    CHECK(!memcmp(shown,data,count));
    pixels+=count;++next_y;
    return 1;
}
static void image_test(int res)
{
    FILE *raw;unsigned y,i;unsigned long dither_pairs=0;
    width=res==2?320:res?800:640;height=res==2?200:res?600:480;
    art_open(res);
    raw=fopen(res==2?"BG320.RAW":res?"BG800.RAW":"BG640.RAW","wb");
    CHECK(raw!=0);if(!raw)return;
    for(y=0;y<(unsigned)height;y++){
        memset(row,0xa5,sizeof(row));art_row(y,row+1);
        CHECK(row[0]==0xa5 && row[width+1]==0xa5);
        CHECK(fwrite(row+1,1,(unsigned)width,raw)==(unsigned)width);
        for(i=1;i<(unsigned)width-2;i++){
            if(row[i]>=1 && row[i]<112 && row[i+1]>=1 && row[i+1]<112 &&
               (row[i]+1==row[i+1] || row[i+1]+1==row[i]) &&
               row[i]==row[i+2] && row[i+1]==row[i+3])++dither_pairs;
        }
    }
    CHECK(dither_pairs>1000);
    CHECK(fclose(raw)==0);
    memset(row,0xa5,sizeof(row));art_row((unsigned)height,row+1);
    CHECK(row[0]==0xa5 && row[width+1]==0xa5);
    for(i=0;i<(unsigned)width;i++)CHECK(row[i+1]==0);
    next_y=0;pixels=0;fail_at=-1;
    CHECK(art_show());CHECK(next_y==(unsigned)height);
    CHECK(pixels==(unsigned long)width*height);
    next_y=0;fail_at=17;CHECK(!art_show());CHECK(next_y==17);
    fail_at=-1;--width;CHECK(!art_show());++width;
    fprintf(logfile,"%ux%u: exact-width rows, all pixels exported, initial-image spans and failure propagation passed; %lu dither pairs\n",width,height,dither_pairs);
}
int main(void)
{
    FILE *raw;unsigned i;const unsigned char *p;
    logfile=fopen("ARTTEST.TXT","w");if(!logfile)return 2;
    CHECK(!art_show());image_test(0);image_test(1);image_test(2);
    p=art_palette();for(i=0;i<768;i++)CHECK(p[i]<=63);
    raw=fopen("DAC.BIN","wb");CHECK(raw!=0);
    if(raw){CHECK(fwrite(p,1,768,raw)==768);CHECK(fclose(raw)==0);}
    fprintf(logfile,"Checks=%lu errors=%u\n",checks,errors);fclose(logfile);
    printf("Art test: errors=%u (ARTTEST.TXT)\n",errors);
    return errors?1:0;
}
