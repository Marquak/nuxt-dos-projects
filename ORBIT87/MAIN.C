/* ORBIT 87 -- original VGA / 8087 / AdLib demonstration for the NuXT.
 * Geometry is prepared before old lines are erased. Sparse retained line
 * lists avoid full-frame copies across the XT's slow video bus. */
#include "ORBIT.H"
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINES 512
#define TRAILS 8
#define NONE 0xffffU
#define SCENES 5
typedef struct { int x0,y0,x1,y1; unsigned next; unsigned char color; } Line;
/* One retained list for each VRAM page, plus one list being built. */
static Line lines[3][MAXLINES];
static unsigned line_count[3],page_lines[2],heads[16],tails[16],which,draw_page;
static Vec3 world[180];
static Vec2 projected[180],trail[TRAILS][48],nebula[320];
static Vec3 circles[84],grid[130];
static unsigned circle_count,grid_cols;
static unsigned char wave_a[130],wave_b[130];
static int xs[641],ys[481],screen_w,screen_h,cx,cy;
static unsigned stars[28][2],star_ink[28],seed=1987;
static unsigned detail=2,slow_frames,frame_count,scene_frames[SCENES];
static unsigned total_notes,pulse,trail_slot,nebula_head,nebula_count,nebula_capacity;
static unsigned long dropped_lines,frames_total,render_ticks;
static unsigned long scene_at,demo_at,scene_duration,limit_ticks;
static unsigned section,start_section,resolution;
static int silent,no87,looping,quick,automatic,reference,report,hud,muted,paused;
static int single_page,buffered,musical_timeline;
static unsigned hud_dirty[2];
static char hud_text[70];
static char footer_cache[2][70];
static unsigned palette_section=NONE,palette_gain,palette_pulse;
static const unsigned scene_ends[5]={384,768,1024,1536,1792};
static const unsigned scene_lengths[5]={749,749,499,999,499};
static volatile int running=1;
#ifdef V20_BUILD
static const char cpu_target[]="V20 / 80186+";
#else
static const char cpu_target[]="8086 / 8088+";
#endif
static void (__interrupt __far *old_break)(void);
static const char *titles[SCENES]={"GRAVITY GARDEN","CRYSTAL OBSERVATORY","NEON OCEAN","MATHEMATICAL NEBULA","WE ARE STILL HERE"};
static const char *subtitles[SCENES]={
 "ORBITAL MECHANICS / A FIELD OF LIGHT",
 "GEOMETRY IN THE QUIET BETWEEN STARS",
 "A TIDE THAT FOLLOWS THE MUSIC",
 "SMALL DIFFERENCES / INFINITE PATHS",
 "A LITTLE COMPUTER / A VERY LARGE UNIVERSE"};
static const unsigned char base_palette[16][3]={
 {0,0,0},{2,5,12},{5,12,22},{8,25,33},
 {3,43,54},{28,57,63},{58,61,63},{33,7,36},
 {54,20,56},{63,37,53},{63,37,10},{63,55,31},
 {5,24,45},{10,39,63},{17,7,29},{36,45,59}};

static void idle(void);
#pragma aux idle = "sti" "hlt";
static void __interrupt __far stop_signal(void){running=0;}
static unsigned random16(void){seed=(unsigned)(seed*25173U+13849U);return seed;}
static int sine(unsigned p){return math_sin(p&255);}
static int cosine(unsigned p){return math_sin((p+64)&255);}
/* Signed 16x16 multiply with division by 256, truncated toward zero.
 * This byte shuffle replaces a generic 32-bit signed arithmetic helper
 * in every wave vertex; it uses only original 8086 instructions. */
static int mul256(int a,int b);
#pragma aux mul256 = \
 "imul bx" "test dx,dx" "jns positive" "add ax,255" "adc dx,0" \
 "positive: mov al,ah" "mov ah,dl" \
 parm [ax] [bx] value [ax] modify exact [ax dx];

static void begin_lines(void)
{
 unsigned i;line_count[which]=0;
 for(i=0;i<16;i++)heads[i]=tails[i]=NONE;
}

/* Reject geometry outside the decorative frame. The video module also clips
 * arbitrary caller coordinates before touching video memory. */
static void add_line(int x0,int y0,int x1,int y1,unsigned ink)
{
 Line *l;unsigned n=line_count[which];
 if(x0<xs[26]||x0>xs[614]||x1<xs[26]||x1>xs[614]||
    y0<ys[94]||y0>ys[418]||y1<ys[94]||y1>ys[418])return;
 if(section==4&&(x0>xs[178]||x1>xs[178])&&(x0<xs[468]||x1<xs[468])&&
    (y0>ys[190]||y1>ys[190])&&(y0<ys[289]||y1<ys[289]))return;
 if(section==4&&(y0>ys[392]||y1>ys[392]))return;
 if(n>=MAXLINES){dropped_lines++;return;}
 ink&=15;if(!ink)return;
 l=&lines[which][n];l->x0=x0;l->y0=y0;l->x1=x1;l->y1=y1;l->color=(unsigned char)ink;l->next=NONE;
 if(heads[ink]==NONE)heads[ink]=n;else lines[which][tails[ink]].next=n;
 tails[ink]=n;line_count[which]=n+1;
}
static void join(unsigned a,unsigned b,unsigned ink)
{
 if(projected[a].valid&&projected[b].valid)
  add_line(cx+projected[a].x,cy+projected[a].y,cx+projected[b].x,cy+projected[b].y,ink);
}
static void dot(int x,int y,unsigned ink,int large)
{
 add_line(x,y,x,y,ink);
 if(large){add_line(x-2,y,x+2,y,ink);add_line(x,y-2,x,y+2,ink);}
}
static void backdrop(void)
{
 unsigned i;for(i=0;i<28;i++){
  if(section==4&&stars[i][0]>xs[178]&&stars[i][0]<xs[468]&&stars[i][1]>ys[190]&&stars[i][1]<ys[289])continue;
  if(section==4&&stars[i][1]>ys[392])continue;
  video_pixel(stars[i][0],stars[i][1],star_ink[i]);
 }
}
static void present(void)
{
 unsigned n,c,old=page_lines[draw_page];Line *l;
 for(n=0;n<line_count[old];n++){
  l=&lines[old][n];video_line(l->x0,l->y0,l->x1,l->y1,0);
 }
 backdrop();
 /* Colour buckets minimise VGA graphics-controller programming. */
 for(c=1;c<16;c++)for(n=heads[c];n!=NONE;n=lines[which][n].next){
  l=&lines[which][n];video_line(l->x0,l->y0,l->x1,l->y1,c);
 }
 page_lines[draw_page]=which;which=old;
}
static void text(int x,int y,unsigned ink,const char *s,int scale)
{video_text(xs[x],ys[y],ink,s,scale);}

static void footer_label(char *s)
{
 if(hud)sprintf(s,"%s / %s / DETAIL %u",math_uses87()?"8087":"INTEGER",music_present()?"ADLIB":"SILENT",detail+1);
 else strcpy(s,buffered?"8087 + FM / DOUBLE BUFFERED / 140 BPM":"V20 + 8087 + VGA + FM / 140 BPM");
}
/* Each page owns its text history, just like its retained geometry. Clear
 * only changed character cells, including the tail of a shorter string. */
static void footer_write(unsigned page,const char *s)
{
 unsigned i,oldlen=strlen(footer_cache[page]),len=strlen(s);
 unsigned limit=(xs[504]-xs[28])/8;char glyph[2];
 if(limit>=sizeof(footer_cache[page]))limit=sizeof(footer_cache[page])-1;
 if(len>limit)len=limit;
 glyph[1]=0;
 for(i=0;i<oldlen||i<len;i++){
  char before=i<oldlen?footer_cache[page][i]:' ',after=i<len?s[i]:' ';
  if(before!=after){glyph[0]=after;video_text(xs[28]+i*8,ys[459],3,glyph,1);}
 }
 memcpy(footer_cache[page],s,len);footer_cache[page][len]=0;
}
static void toggle_hud(void)
{
 unsigned p;char label[70];
 hud=!hud;footer_label(label);
 for(p=0;p<(buffered?2U:1U);p++){video_select_page(p);footer_write(p,label);}
 video_select_page(draw_page);hud_text[0]=0;hud_dirty[0]=hud_dirty[1]=0;
}

static void status(void)
{
 text(512,440,11,paused?"PAUSED  ":"        ",1);
 text(504,459,3,!music_present()?"SILENT   ":muted?"MUTED    ":"ADLIB ON ",1);
}
static void status_pages(void)
{
 unsigned p;for(p=0;p<(buffered?2U:1U);p++){video_select_page(p);status();}
 video_select_page(draw_page);
}

static void credits(unsigned long t)
{
 static const char message[]="    ORBIT 87 / ORIGINAL CODE AND FM / A V20 AND AN 8087 UNDER THE STARS / GREETINGS TO THE NUXT CREW / THANK YOU FOR KEEPING OLD MACHINES ALIVE!    ";
 char visible[128],blank[128];unsigned i,offset=(unsigned)(t*2),n=(screen_w-xs[32]*2)/8-1;
 if(n>125)n=125;
 for(i=0;i<n;i++)visible[i]=message[(offset/8+i)%(sizeof(message)-1)];visible[n]=0;
 memset(blank,' ',n+2);blank[n+2]=0;
 video_text(xs[26],ys[402],0,blank,1);
 video_text(xs[32]-(offset&7),ys[402],5,visible,1);
}

static void scene_palette(unsigned long local)
{
 unsigned char rgb[48];unsigned i,j,value,gain=64;
 if(local<18)gain=(unsigned)local*64/18;
 if(local+18>scene_duration){unsigned tail=(unsigned)(scene_duration>local?scene_duration-local:0)*64/18;if(tail<gain)gain=tail;}
 if(palette_section==section&&palette_gain==gain&&palette_pulse==pulse)return;
 palette_section=section;palette_gain=gain;palette_pulse=pulse;
 for(i=0;i<16;i++)for(j=0;j<3;j++){
  unsigned k=j;
  /* Distinct colour atmospheres, retaining neutral text and navigation. */
  if(i!=0&&i!=6&&i!=15){
   if(section==1)k=(j+1)%3;
   else if(section==3)k=(j+2)%3;
  }
  value=base_palette[i][k];
  if(i==4||i==8||i==10){value+=pulse;if(value>63)value=63;}
  rgb[i*3+j]=(unsigned char)(value*gain/64);
 }
 video_palette(rgb);
}

static void frame(void)
{
 char buf[70];int i;
 video_clear();
 text(28,18,6,"ORBIT 87",2);
 text(202,25,3,"A NUXT SPACE ODYSSEY",1);
 sprintf(buf,"ACT %u / 5",section+1);text(516,25,15,buf,1);
 video_line(xs[28],ys[44],xs[611],ys[44],2);
 text(28,57,4,titles[section],1);text(28,73,15,subtitles[section],1);
 video_line(xs[24],ys[90],xs[615],ys[90],2);
 video_line(xs[24],ys[422],xs[615],ys[422],2);
 for(i=0;i<4;i++){
  int x=i&1?615:24,y=i&2?422:90,dx=i&1?-12:12,dy=i&2?-12:12;
  video_line(xs[x],ys[y],xs[x+dx],ys[y],4);video_line(xs[x],ys[y],xs[x],ys[y+dy],4);
 }
 text(28,440,15,"ESC EXIT   SPACE PAUSE   M MUSIC   N NEXT",1);
 footer_label(buf);text(28,459,3,buf,1);
 if(section==4){
  video_text(cx-64,ys[203],6,"NUXT",4);
  video_text(cx-19*4,ys[247],4,"ORIGINAL CODE AND FM",1);
  video_text(cx-23*4,ys[269],15,"FOR THE MACHINES WE KEEP",1);
 }
 status();
 backdrop();
}

static void redraw_pages(void)
{
 unsigned p;
 line_count[0]=line_count[1]=line_count[2]=0;
 page_lines[0]=1;page_lines[1]=2;which=0;
 /* Scene transitions are faded to black before these once-per-act clears.
  * Normal animation only erases the retained geometry on the hidden page. */
 for(p=0;p<(buffered?2U:1U);p++){video_select_page(p);frame();footer_label(footer_cache[p]);}
 video_show_page(0);draw_page=buffered?1:0;video_select_page(draw_page);
 hud_dirty[0]=hud_dirty[1]=0;hud_text[0]=0;
}

static void enter_scene(unsigned next)
{
 /* 24/24/16/32/16 bars at140 BPM. Automatic changes use the score position,
  * so the breakdown, drop and finale stay aligned despite slow rendering. */
 section=next;scene_at=music_ticks();scene_duration=quick?110UL:scene_lengths[section];
 trail_slot=0;nebula_head=nebula_count=nebula_capacity=0;
 memset(trail,0,sizeof(trail));memset(nebula,0,sizeof(nebula));
 frame_count=slow_frames=pulse=0;palette_section=NONE;
 math_gravity_reset();math_attractor_reset();music_section(section);scene_palette(0);redraw_pages();
}

static void gravity(unsigned long t,unsigned elapsed)
{
 unsigned i,j,n=detail==2?48:detail==1?32:20,oldest=(trail_slot+1)%TRAILS;
 int scale=screen_h*3/4;
 if(elapsed>3)elapsed=3;
 for(i=0;i<elapsed;i++)math_gravity_step(world,n);
 math_matrix((unsigned)(t/3)&255,22+(unsigned)(sine((unsigned)(t/4))/32),0);
 math_project(world,projected,n,scale,520);
 for(i=0;i<n;i++){
  Vec2 *p=&trail[trail_slot][i];
  *p=projected[i];
  for(j=0;j<TRAILS-1;j++){
   unsigned a=(oldest+j)%TRAILS,b=(a+1)%TRAILS;
   Vec2 *u=&trail[a][i],*v=&trail[b][i];
   if(u->valid&&v->valid)add_line(cx+u->x,cy+u->y,cx+v->x,cy+v->y,j<2?2:j<4?3:j<6?4:5);
  }
  if(p->valid)dot(cx+p->x,cy+p->y,i%7?5:11,i%12==0);
 }
 trail_slot=(trail_slot+1)%TRAILS;
 /* A warm beacon marks the softened gravitational centre. */
 dot(cx,cy,11,1);
 for(i=0;i<16;i++)add_line(cx+mul256(xs[9],cosine(i*16)),cy+mul256(ys[9],sine(i*16)),
                         cx+mul256(xs[9],cosine((i+1)*16)),cy+mul256(ys[9],sine((i+1)*16)),10);

}

static void crystal(unsigned long t)
{
 unsigned i,j,k,n=detail==2?28:detail==1?22:16,at=0;
 int r=142,p;
 /* Three perpendicular great circles form an armillary observatory. */
 if(circle_count!=n){
  for(j=0;j<3;j++)for(i=0;i<n;i++){
   int a=mul256(r,cosine(i*256/n)),b=mul256(r,sine(i*256/n));
   circles[at].x=j==0?0:a;circles[at].y=j==1?0:(j==0?a:b);circles[at].z=j==2?0:b;at++;
  }
  circle_count=n;
 }
 at=n*3;memcpy(world,circles,at*sizeof(Vec3));
 /* A breathing eight-vertex crystal, with a second counter-rotating core. */
 p=55+sine((unsigned)t*2)/16;
 for(i=0;i<8;i++){
  world[at+i].x=i&1?p:-p;world[at+i].y=i&2?p:-p;world[at+i].z=i&4?p:-p;
 }
 math_matrix((unsigned)t*2,(unsigned)t, (unsigned)(t/3));
 math_project(world,projected,at+8,screen_h*4/5,480);
 for(j=0;j<3;j++)for(i=0;i<n;i++)join(j*n+i,j*n+(i+1)%n,j==0?4:j==1?8:10);
 for(i=0;i<8;i++)for(k=1;k<=4;k<<=1)if(!(i&k))join(at+i,at+(i^k),5);
 for(i=0;i<8;i++)if(projected[at+i].valid)dot(cx+projected[at+i].x,cy+projected[at+i].y,6,1);
 /* An independent angular diamond at the heart. */
 for(i=0;i<6;i++){world[i].x=world[i].y=world[i].z=0;if(i<2)world[i].x=i?31:-31;else if(i<4)world[i].y=i&1?31:-31;else world[i].z=i&1?31:-31;}
 math_matrix((unsigned)(0-t)*3,(unsigned)t*2,32);math_project(world,projected,6,screen_h*4/5,480);
 for(i=0;i<6;i++)for(j=i+1;j<6;j++)if(i/2!=j/2)join(i,j,9);
}

static void ocean(unsigned long t)
{
 unsigned x,z,nx=detail==2?13:detail==1?11:9,nz=detail==2?10:detail==1?8:7,n=0;
 int px,pz,amp=20+(int)pulse;
 if(grid_cols!=nx){
  for(z=0;z<nz;z++)for(x=0;x<nx;x++){
   px=-200+(int)(400*x/(nx-1));pz=-170+(int)(340*z/(nz-1));
   grid[n].x=px;grid[n].z=pz;wave_a[n]=(unsigned char)(x*21+z*13);wave_b[n]=(unsigned char)(z*37-x*11);n++;
  }
  grid_cols=nx;
 }
 for(n=0;n<nx*nz;n++){
  world[n].x=grid[n].x;world[n].z=grid[n].z;
  world[n].y=mul256(amp,sine(wave_a[n]+(unsigned)t*3))+mul256(12,sine(wave_b[n]-(unsigned)t*2));
 }
 math_matrix((unsigned)(sine((unsigned)t/3)/24)&255,35,0);
 math_project(world,projected,n,screen_h*4/5,560);
 for(z=0;z<nz;z++)for(x=0;x<nx;x++){
  n=z*nx+x;
  if(x+1<nx)join(n,n+1,z<3?3:z<6?4:5);
  if(z+1<nz)join(n,n+nx,x==nx/2?9:12);
 }
 /* A rising electronic sun above the mathematical sea. */
 for(x=0;x<20;x++){
  int a=x*256/20,b=(x+1)*256/20;
  add_line(cx+mul256(xs[37],cosine(a)),ys[142]+mul256(ys[24],sine(a)),
           cx+mul256(xs[37],cosine(b)),ys[142]+mul256(ys[24],sine(b)),x<10?10:8);
 }
}

static void attractor(unsigned long t)
{
 unsigned i,n=detail==2?12:detail==1?9:6,cap=detail==2?320:detail==1?224:128;
 unsigned a,b,split1,split2;
 /* A detail change uses a different ring modulus. Start a fresh trail
  * rather than walking old indices with the new capacity. */
 if(nebula_capacity!=cap){nebula_head=nebula_count=0;nebula_capacity=cap;}
 math_attractor_step(world,n);
 math_matrix((unsigned)(t/8),12,0);math_project(world,projected,n,screen_h,460);
 for(i=0;i<n;i++){
  nebula[nebula_head]=projected[i];if(++nebula_head==cap)nebula_head=0;
  if(nebula_count<cap)nebula_count++;
 }
 a=nebula_head+cap-nebula_count;if(a>=cap)a-=cap;
 split1=nebula_count/3;split2=nebula_count*2/3;
 for(i=1;i<nebula_count;i++){
  Vec2 *u,*v;b=a+1;if(b==cap)b=0;u=&nebula[a];v=&nebula[b];
  if(u->valid&&v->valid)add_line(cx+u->x,cy+u->y,cx+v->x,cy+v->y,i<split1?3:i<split2?8:5);
  a=b;
 }
 if(nebula_count){Vec2 *v=&nebula[nebula_head?nebula_head-1:cap-1];if(v->valid)dot(cx+v->x,cy+v->y,6,1);}
}

static void arguments(int argc,char **argv)
{
 int i;for(i=1;i<argc;i++){
  char *a=argv[i];if(*a=='/'||*a=='-')a++;
  if(!stricmp(a,"640"))resolution=0;
  else if(!stricmp(a,"800"))resolution=1;
  else if(!stricmp(a,"1024"))resolution=2;
  else if(!stricmp(a,"silent"))silent=1;
  else if(!stricmp(a,"no87"))no87=1;
  else if(!stricmp(a,"loop"))looping=1;
  else if(!stricmp(a,"quick"))quick=1;
  else if(!stricmp(a,"auto"))automatic=1;
  else if(!stricmp(a,"ref"))reference=1;
  else if(!stricmp(a,"report"))report=1;
  else if(!stricmp(a,"hud"))hud=1;
  else if(!stricmp(a,"single"))single_page=1;
  else if(!strnicmp(a,"scene:",6)){int n=atoi(a+6);if(n<1||n>5){puts("Scene must be 1..5.");exit(1);}start_section=n-1;}
  else if(!strnicmp(a,"ticks:",6)){limit_ticks=strtoul(a+6,0,10);if(!limit_ticks||limit_ticks>65000UL){puts("Ticks must be 1..65000.");exit(1);}}
  else if(!strnicmp(a,"detail:",7)){int n=atoi(a+7);if(n<1||n>3){puts("Detail must be 1..3.");exit(1);}detail=n-1;}
  else {
   puts("ORBIT 87 - VGA / 8087 / AdLib space demo\n");
   puts("ORBIT87 [/640|/800|/1024] [/NO87] [/SILENT] [/AUTO] [/LOOP]");
   puts("        [/DETAIL:1..3] [/HUD] [/SCENE:1..5] [/QUICK]");
   puts("        [/REPORT] [/TICKS:1..65000] [/REF] [/SINGLE]");
   puts("ESC exit; SPACE pause; M mute; N next act; 1..5 choose act; F stats.");
   puts("All modes use 16 colours. /REF selects the test C line renderer.");
   puts("/REPORT writes ORBIT.LOG. /QUICK shortens each act for testing.");
   puts("/SINGLE disables automatic double buffering for comparison.");
   exit(stricmp(a,"?")&&stricmp(a,"help")?1:0);
  }
 }
}

int main(int argc,char **argv)
{
 unsigned i,elapsed;unsigned long last,now,before,local,last_hud=0;int ch,have87,result=0;
 char driver[48],buffer_name[32];
 arguments(argc,argv);have87=!no87&&math_detect();
 puts("\n ORBIT 87 2.1\n A NuXT space odyssey\n");
 printf(" Build: %s\n",cpu_target);
 printf(" Maths: %s\n",have87?"8087 coprocessor":"integer fallback");
 puts(" Video: VGA 640 x 480 / 16 colours is recommended.");
 puts("        800 x 600 and 1024 x 768 are experimental SVGA modes.");
 puts(" Music: 140 BPM AdLib techno / one evolving 192-second track.\n");
 if(!automatic){
  puts(" ENTER - start   1 - 640x480   2 - 800x600   3 - 1024x768");
  puts(" S - sound on/off   ESC - quit");
  for(;;){ch=getch();if(ch==27)return 0;if(ch=='s'||ch=='S'){silent=!silent;printf(" Music %s\n",silent?"off":"on");}else if(ch>='1'&&ch<='3'){resolution=ch-'1';break;}else if(ch==13)break;}
 }
 math_init(have87);
 if(!video_open(resolution)){math_end();puts("This VGA mode is unavailable or failed its memory check. Try /640.");return 1;}
 buffered=video_buffering(!single_page);
 if(buffered<0){video_close();math_end();puts("The VBE display-start check failed. Try /SINGLE.");return 1;}
 strcpy(buffer_name,video_buffer_name());
 strncpy(driver,video_driver(),47);driver[47]=0;
 video_reference(reference);screen_w=video_width();screen_h=video_height();
 for(i=0;i<=640;i++)xs[i]=(int)((unsigned long)i*screen_w/640);
 for(i=0;i<=480;i++)ys[i]=(int)((unsigned long)i*screen_h/480);
 cx=screen_w/2;cy=ys[257];
 for(i=0;i<28;i++){stars[i][0]=xs[35+random16()%570];stars[i][1]=ys[102+random16()%306];star_ink[i]=i%4?1:2;}
 old_break=_dos_getvect(0x23);_dos_setvect(0x23,stop_signal);
 musical_timeline=!quick&&!start_section;
 music_start(silent);demo_at=music_ticks();enter_scene(start_section);last=music_ticks();
 while(running){
  while(kbhit()){
   ch=getch();if(ch==0||ch==224){getch();continue;}
   if(ch==27||ch=='q'||ch=='Q')running=0;
   else if(ch==' '){paused=!paused;music_pause(paused);status_pages();}
   else if(ch=='m'||ch=='M'){muted=!muted;music_mute(muted);status_pages();}
   else if(ch=='f'||ch=='F')toggle_hud();
   else if(ch=='n'||ch=='N'){musical_timeline=0;enter_scene((section+1)%SCENES);last=music_ticks();}
   else if(ch>='1'&&ch<='5'){musical_timeline=0;enter_scene(ch-'1');last=music_ticks();}
  }
  if(!running)break;
  now=music_ticks();if(paused||now==last){idle();continue;}
  if(limit_ticks&&now-demo_at>=limit_ticks)break;
  local=now-scene_at;
  /* music_beat counts steps already played: the new bar has begun only
   * after its first step, and the final release has its own finished flag. */
  if(musical_timeline?(section==SCENES-1?music_finished():music_beat()>scene_ends[section]):local>=scene_duration){
   if(section+1==SCENES&&!looping)break;
   if(section+1==SCENES){music_restart();musical_timeline=!quick;}
   enter_scene((section+1)%SCENES);last=music_ticks();continue;
  }
  elapsed=(unsigned)(now-last);if(elapsed>4)elapsed=4;last=now;before=now;
  pulse=music_pulse();begin_lines();
  switch(section){case 0:gravity(local,elapsed);break;case 1:crystal(local);break;case 2:ocean(local);break;case 3:attractor(local);break;default:gravity(local,elapsed);break;}
  present();
  if(section==4)credits(local);
  scene_palette(local);frame_count++;scene_frames[section]++;frames_total++;
  now=music_ticks();
  if(hud&&(now-last_hud>=18||!hud_text[0])){
   last_hud=now;
   sprintf(hud_text,"%s / DETAIL %u / FRAMES %u / NOTES %u    ",have87?"8087":"INTEGER",detail+1,frame_count,music_notes());
   hud_dirty[0]=hud_dirty[1]=1;
  }
  if(hud&&hud_dirty[draw_page]){footer_write(draw_page,hud_text);hud_dirty[draw_page]=0;}
  video_show_page(draw_page);
  if(buffered){draw_page^=1;video_select_page(draw_page);}
  now=music_ticks();render_ticks+=now-before;
  if(now-before>2){if(++slow_frames>=4&&detail){detail--;slow_frames=0;nebula_head=nebula_count=0;}}else if(slow_frames)slow_frames--;
 }
 total_notes=music_notes();now=music_ticks()-demo_at;
 music_stop();_dos_setvect(0x23,old_break);video_close();math_end();
 if(report){FILE *f=fopen("ORBIT.LOG","w");if(f){fprintf(f,"ORBIT 87 2.1\nbuild=%s\nvideo=%s\nbuffering=%s\nresolution=%dx%d\n8087=%d\nreference=%d\nticks=%lu\nframes=%lu\nrender_ticks=%lu\nnotes=%u\ndropped_lines=%lu\nfinal_detail=%u\n",cpu_target,driver,buffer_name,screen_w,screen_h,have87,reference,now,frames_total,render_ticks,total_notes,dropped_lines,detail+1);for(i=0;i<SCENES;i++)fprintf(f,"scene%u=%u\n",i+1,scene_frames[i]);fclose(f);}else{puts("Could not write ORBIT.LOG.");result=1;}}
 printf("\nORBIT 87 finished. %lu frames, %u musical notes.\n",frames_total,total_notes);
 puts("Video and timer restored. Thank you for keeping these machines alive.");
 return result;
}
