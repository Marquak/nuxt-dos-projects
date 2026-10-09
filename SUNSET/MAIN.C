/* NEON COAST -- an endless original V20 / 8087 / AdLib sunset drive.
 * Compose moving regions in a cached 320 frame or high-resolution sparse
 * tiles before replacing them in VGA; the soundtrack runs independently. */
#include "SUNSET.H"
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FRONDS.H"

#define MAXVERT 448
#define MAXEDGE 520
#define TREES 8
typedef struct {unsigned a,b;unsigned char c;} Edge;
static Vec3 world[MAXVERT];
static Vec2 view[MAXVERT];
static Edge edges[MAXEDGE];
static unsigned nv,ne,grid_at[10],dash_at[8][2],tree_z[TREES],tree_order[TREES];
static unsigned w,h,cx,horizon,scale,detail=2;
static unsigned colors=256;
static int resolution=2,automatic,silent,no87,paused,muted,report;
static unsigned long tick_limit,frame_limit,frames,bytes_total,render_ticks,overflow_frames;
static unsigned max_tiles,slow_frames,last_pulse=999;
static volatile int running=1;
static void (__interrupt __far *old_break)(void);
static const char *sound_name="HORIZON RUNNER";

static void idle(void);
#pragma aux idle = "sti" "hlt";
static void __interrupt __far stop_signal(void){running=0;}
static int xs(int x){return resolution==2?x/2:resolution?(x*5)/4:x;}
static int ys(int y){return resolution==2?(y*5)/12:resolution?(y*5)/4:y;}
static int screen_y(int y){return (int)horizon-(resolution==2?(y*5)/6:y);}
static unsigned vertex(int x,int y,int z){
 unsigned n=nv;if(n>=MAXVERT)return 0;
 world[n].x=x;world[n].y=y;world[n].z=z;nv++;return n;
}
static void edge(unsigned a,unsigned b,unsigned char c){
 if(ne>=MAXEDGE)return;edges[ne].a=a;edges[ne].b=b;edges[ne].c=c;ne++;
}
static void palm(int x,int z,unsigned id,unsigned long t){
 unsigned b=nv,j,at,fronds=detail?8:6,lod;
 if(z>1000)fronds=4;else if(z>600&&fronds>6)fronds=6;
 lod=fronds==4?0:fronds==6?1:2;
 int lean=x<0?18:-18,wind=math_sin((unsigned)(t/3)+id*31)/45;
 unsigned char c=(unsigned char)(176+(1700-z)*7/1500);
 if(c<176)c=176;if(c>183)c=183;
 /* The curved trunk narrows into the crown; all points have real depth. */
 vertex(x-5,-70,z);vertex(x+5,-70,z);
 vertex(x+lean/2-4,40,z+3);vertex(x+lean/2+4,40,z+3);
 vertex(x+lean-3+wind,148,z+6);vertex(x+lean+3+wind,148,z+6);
 vertex(x+lean+wind,153,z+6);
 edge(b,b+2,c);edge(b+2,b+4,c);edge(b+1,b+3,c);edge(b+3,b+5,c);
 edge(b,b+1,c);edge(b+2,b+3,c);edge(b+4,b+5,c);
 for(j=0;j<fronds;j++){
  const int __far *f=frond_shape[id][lod][j];
  int dx=f[0],dz=f[1],px=f[2],pz=f[3];
  int tx=x+lean+wind,ty=153,tz=z+6;
  at=nv;
  vertex(tx+dx/2,ty+22,tz+dz/2);
  vertex(tx+dx,ty-40+(int)(j&1)*8,tz+dz);
  vertex(tx+dx/2+px,ty+8,tz+dz/2-pz);
  vertex(tx+dx/2-px,ty+8,tz+dz/2+pz);
  edge(b+6,at,c);edge(at,at+1,c);
  edge(b+6,at+2,c);edge(at+2,at+1,c);
  edge(b+6,at+3,(unsigned char)(c+8));edge(at+3,at+1,c);
 }
}
static void scene(unsigned long t){
 unsigned i,j,k,travel=(unsigned)(((t%15200UL)*6UL)%30400UL),road=travel%1600;
 unsigned trees=detail==2?8:6;
 nv=ne=0;
 for(i=0;i<10;i++)grid_at[i]=vertex(0,-70,110+(i*160+1600-road)%1600);
 for(i=0;i<8;i++){
  int z=130+(i*200+1600-road)%1600;
  for(j=0;j<2;j++){
   int x=j?25:-25;
   dash_at[i][j]=vertex(x,-70,z);vertex(x,-70,z+88);
  }
 }
 for(i=0;i<trees;i++){
  unsigned pair=i/2;
  /* At z<=144 every frond/trunk has already left the viewport. Recycle
   * at 120 so even a slow frame never shows a nearby palm disappear. */
  tree_z[i]=120+(pair*(detail==2?380:506)+(i&1?170:0)+1520-travel%1520)%1520;
  tree_order[i]=i;
 }
 /* Far-to-near drawing preserves the nearer palm at line intersections. */
 for(i=1;i<trees;i++){k=tree_order[i];j=i;while(j&&tree_z[tree_order[j-1]]<tree_z[k]){tree_order[j]=tree_order[j-1];j--;}tree_order[j]=k;}
 for(i=0;i<trees;i++){
  k=tree_order[i];palm((k&1?1:-1)*(218+(int)(k/2%2)*16),tree_z[k],k,t);
 }
 /* Depth coefficients were prepared once; project the batch and cache
  * screen coordinates once per vertex rather than again for every edge. */
 math_project(world,view,nv,scale,0);
 for(i=0;i<nv;i++)if(view[i].valid){view[i].x+=cx;view[i].y=screen_y(view[i].y);}
 for(i=0;i<10;i++){
  unsigned v=grid_at[i];int y,left,right;unsigned char c;
  if(!view[v].valid)continue;y=view[v].y;
  if(y<=(int)horizon+2||y>=(int)h)continue;
  left=(int)cx-xs(14)-(int)((long)(y-horizon)*xs(179)/ys(250));
  right=(int)w-left;
  c=(unsigned char)(160+(unsigned long)(y-horizon)*13/(h-horizon));
  tiles_line(0,y,left-xs(4),y,c);tiles_line(right+xs(4),y,w-1,y,c);
 }
 for(i=0;i<8;i++)for(j=0;j<2;j++){
  unsigned a=dash_at[i][j],b=a+1;int y0,y1,x0,x1;
  if(!view[a].valid||!view[b].valid)continue;
  x0=view[a].x;x1=view[b].x;y0=view[a].y;y1=view[b].y;
  tiles_line(x0,y0,x1,y1,165);
  if(y0>(int)horizon+ys(75)){tiles_line(x0-1,y0,x1,y1,166);tiles_line(x0+1,y0,x1,y1,166);}
 }
 for(i=0;i<ne;i++){
  unsigned a=edges[i].a,b=edges[i].b;
  if(view[a].valid&&view[b].valid)
   tiles_line(view[a].x,view[a].y,view[b].x,view[b].y,edges[i].c);
 }
}
static void glow(void){
 unsigned p=music_pulse(),i,gain=52+p;unsigned char rgb[24*3];
 const unsigned char *base=art_palette();
 if(colors==16||p==last_pulse)return;last_pulse=p;
 for(i=0;i<24*3;i++)rgb[i]=(unsigned char)((unsigned)base[224*3+i]*gain/64);
 video_palette_range(224,24,rgb);
}
static void arguments(int argc,char **argv){
 int i;
 for(i=1;i<argc;i++){
  char *a=argv[i];if(*a=='/'||*a=='-')a++;
  if(!stricmp(a,"320"))resolution=2;
  else if(!stricmp(a,"640"))resolution=0;
  else if(!stricmp(a,"800"))resolution=1;
  else if(!stricmp(a,"16"))colors=16;
  else if(!stricmp(a,"256"))colors=256;
  else if(!stricmp(a,"auto"))automatic=1;
  else if(!stricmp(a,"silent"))silent=1;
  else if(!stricmp(a,"no87"))no87=1;
  else if(!stricmp(a,"report"))report=1;
  else if(!strnicmp(a,"ticks:",6)){tick_limit=strtoul(a+6,0,10);if(!tick_limit||tick_limit>65000UL){puts("Ticks must be 1..65000.");exit(1);}}
  else if(!strnicmp(a,"frames:",7)){frame_limit=strtoul(a+7,0,10);if(!frame_limit||frame_limit>65000UL){puts("Frames must be 1..65000.");exit(1);}}
  else if(!strnicmp(a,"detail:",7)){int n=atoi(a+7);if(n<1||n>3){puts("Detail must be 1..3.");exit(1);}detail=n-1;}
  else {
   puts("NEON COAST 3.1 - endless V20 / 8087 / AdLib sunset drive\n");
   puts("SUNSET [/320|/640|/800] [/16|/256] [/AUTO] [/SILENT] [/NO87] [/DETAIL:1..3]");
   puts("       [/REPORT] [/TICKS:1..65000] [/FRAMES:1..65000]");
   puts("V20/V30 or 80186+ CPU required. Default: 320x200, 256 colours.");
   puts("ESC/Q exits; SPACE pauses; M mutes. No on-screen text or ending.");
   puts("/REPORT writes SUNSET.LOG; /TICKS and /FRAMES are test limits.");
   exit(stricmp(a,"?")&&stricmp(a,"help")?1:0);
  }
 }
}
int main(int argc,char **argv){
 unsigned long now,last,before,elapsed;unsigned notes,tiles;int have87,ch,ok=1;
 static unsigned char black[768];char driver[40],renderer[40];
 arguments(argc,argv);have87=!no87&&math_detect();
 puts("\n NEON COAST\n An endless drive into the sunset\n");
 printf(" V20 / 80186 build. Maths: %s\n",have87?"8087":"integer fallback");
 printf(" Music: %s / 104 BPM AdLib synthwave\n",sound_name);
 puts(" Video: 320x200, 640x480, 800x600 / 16 or 256 colours\n");
 if(!automatic){
  puts(" 1 - 320x200 (fast)   2 - 640x480   3 - 800x600");
  puts(" C - 16/256 colours   S - sound   ENTER - start   ESC - quit");
  printf(" Selected: %s / %u colours\n",resolution==2?"320x200":resolution?"800x600":"640x480",colors);
  while(1){
   ch=getch();if(ch==27)return 0;if(ch==13)break;
   if(ch=='s'||ch=='S'){silent=!silent;printf(" Music %s\n",silent?"off":"on");}
   else if(ch=='c'||ch=='C')colors=colors==16?256:16;
   else if(ch>='1'&&ch<='3')resolution=ch=='1'?2:ch=='2'?0:1;
   else continue;
   printf(" Selected: %s / %u colours\n",resolution==2?"320x200":resolution?"800x600":"640x480",colors);
  }
 }
 math_init(have87);video_depth(colors);
 if(!video_open(resolution)){math_end();puts("This video mode is unavailable or failed its memory test.");puts("Try /320 /256 for standard VGA. High SVGA modes need the Trident or a compatible VBE BIOS.");return 1;}
 w=video_width();h=video_height();cx=w/2;horizon=ys(230);scale=xs(430);
 strcpy(driver,video_driver());video_palette(black);
 if(!tiles_open(w,h)){video_close();math_end();puts("Not enough conventional memory for the tile compositor.");return 1;}
 strcpy(renderer,tiles_driver());
 art_open(resolution);
 if(!tiles_cache_background(art_row)){tiles_close();video_close();math_end();puts("Background cache failed.");return 1;}
 if(!art_show()){tiles_close();video_close();math_end();puts("Video upload failed.");return 1;}
 math_matrix(0,0,0);math_prepare(scale);
 old_break=_dos_getvect(0x23);_dos_setvect(0x23,stop_signal);
 music_start(silent);video_palette(colors==16?video_palette16():art_palette());last=music_ticks();
 while(running){
  while(kbhit()){
   ch=getch();if(ch==0||ch==224){getch();continue;}
   if(ch==27||ch=='q'||ch=='Q')running=0;
   else if(ch==' '){paused=!paused;music_pause(paused);}
   else if(ch=='m'||ch=='M'){muted=!muted;music_mute(muted);}
  }
  if(!running)break;now=music_ticks();
  if(paused||now==last){idle();continue;}
  if(tick_limit&&now>=tick_limit)break;
  last=before=now;tiles_begin();scene(now);glow();
  if(!tiles_present(art_row)){ok=0;break;}
  frames++;tiles=tiles_count();if(tiles>max_tiles)max_tiles=tiles;
  bytes_total+=tiles_bytes();
  if(tiles_overflow()){overflow_frames++;if(detail)detail--;}
  now=music_ticks();render_ticks+=now-before;
  if(now-before>4){if(++slow_frames>=8&&detail){detail--;slow_frames=0;}}else if(slow_frames)slow_frames--;
  if(frame_limit&&frames>=frame_limit)break;
 }
 notes=music_notes();elapsed=music_ticks();music_stop();_dos_setvect(0x23,old_break);
 tiles_close();video_close();math_end();
 if(report){FILE *f=fopen("SUNSET.LOG","w");if(f){fprintf(f,"NEON COAST 3.1\nvideo=%s\nrenderer=%s\nresolution=%ux%u\ncolours=%u\n8087=%d\nticks=%lu\nframes=%lu\nrender_ticks=%lu\nbytes_uploaded=%lu\nmax_tiles=%u\noverflow_frames=%lu\nfinal_detail=%u\nnotes=%u\n",driver,renderer,w,h,colors,have87,elapsed,frames,render_ticks,bytes_total,max_tiles,overflow_frames,detail+1,notes);fclose(f);}else{puts("Could not write SUNSET.LOG.");ok=0;}}
 if(!ok)puts("A video transfer failed; returned safely to DOS.");
 else printf("\nNeon Coast stopped. %lu frames. Video and timer restored.\n",frames);
 return ok?0:1;
}
