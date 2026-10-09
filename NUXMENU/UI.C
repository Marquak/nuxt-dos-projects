/* NuXT Menu: direct text video, BIOS input, DOS 8.3 browsing. No TSR. */
#include "MENU.H"
#include <dos.h>
#include <bios.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <direct.h>
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#include <time.h>

#define UP 0x4800
#define DOWN 0x5000
#define LEFT 0x4b00
#define RIGHT 0x4d00
#define HOME 0x4700
#define END 0x4f00
#define PGUP 0x4900
#define PGDN 0x5100
#define DEL 0x5300
#define F1 0x3b00
#define F2 0x3c00
#define F3 0x3d00
#define F4 0x3e00
#define F5 0x3f00
#define F6 0x4000
#define F7 0x4100
#define F8 0x4200
#define F9 0x4300
#define F10 0x4400
#define CLICK 0x1000
#define MAXFILES 384
#define DESK 0
#define PANEL 1
#define SELECT 2
#define ACCENT 3
#define TITLE 4
#define DIM 5
#define FIELD 6
#define BORDER 7

static unsigned short frame[80*50];
static int rows=25,vga=0,mono=0,original_rows=25,mouse_ok=0,mouse_visible=0,mx,my;
static int click_guard=0,guard_x,guard_y;
static clock_t guard_time;
static int current=ROOT,selected=0,top=0,members[ITEMS],count=0,dirty=0;
static char home[PATHLEN],status[79]="F2: browse your drive and add a program. F1: help.";
static const char *theme_names[7]={"Classic blue","Amber terminal","Matrix green","Neon coast","Slate","Paper","Custom"};
static const unsigned char themes[6][8]={
 {0x17,0x1f,0x70,0x1b,0x3f,0x17,0x0f,0x19},
 {0x06,0x0e,0x60,0x0f,0x60,0x06,0x0e,0x06},
 {0x02,0x0a,0x20,0x0f,0x20,0x02,0x0a,0x02},
 {0x10,0x1b,0x5f,0x1d,0x5f,0x13,0x0b,0x15},
 {0x08,0x07,0x3f,0x0b,0x70,0x08,0x0f,0x08},
 {0x70,0x70,0x1f,0x71,0x1f,0x78,0x0f,0x78}
};
static const char *roles[8]={"Desktop","Panel text","Selection","Accent","Title bar","Muted text","Input field","Borders"};
typedef struct {char name[13];unsigned char dir;unsigned long size;} DirItem;
static DirItem files[MAXFILES];
static int visible[MAXFILES],file_count,visible_count,limited,filter_dirty=1;

static void idle(void);
#pragma aux idle = "sti" "hlt";
static int __far hard(unsigned a,unsigned b,unsigned __far *c){(void)a;(void)b;(void)c;_hardresume(_HARDERR_FAIL);return 0;}
static void mouseshow(int yes){
 union REGS r;if(!mouse_ok||yes==mouse_visible)return;
 r.x.ax=yes?1:2;int86(0x33,&r,&r);mouse_visible=yes;
}
static void cursor(int yes,int x,int y){
 union REGS r;r.h.ah=1;r.x.cx=yes?0x0607:0x2000;int86(0x10,&r,&r);
 if(yes){r.h.ah=2;r.h.bh=0;r.h.dl=(unsigned char)x;r.h.dh=(unsigned char)y;int86(0x10,&r,&r);}
}
static void mode(int wanted){
 union REGS r;unsigned char actual;
 mouseshow(0);if(!vga)wanted=0;
 if(vga){r.x.ax=(wanted==1)?0x1201:0x1202;r.x.bx=0x0030;int86(0x10,&r,&r);}
 r.x.ax=mono?7:3;int86(0x10,&r,&r);
 if(wanted){r.x.ax=0x1112;r.x.bx=0;int86(0x10,&r,&r);}
 rows=wanted==1?43:wanted==2?50:25;
 if(vga){actual=*(unsigned char __far *)MK_FP(0x40,0x84);if(actual>=24&&actual<50)rows=actual+1;}
 if(vga){r.x.ax=0x1003;r.x.bx=0;int86(0x10,&r,&r);}
 if(mouse_ok){
  /* Reset AFTER the video/font change: refresh geometry and cursor hide count. */
  r.x.ax=0;int86(0x33,&r,&r);mouse_ok=r.x.ax==0xffff;mouse_visible=0;
 }
 if(mouse_ok){r.x.ax=7;r.x.cx=0;r.x.dx=639;int86(0x33,&r,&r);r.x.ax=8;r.x.cx=0;r.x.dx=rows*8-1;int86(0x33,&r,&r);}
 cursor(0,0,0);
}
static void put(int x,int y,unsigned ch,int role){if(x>=0&&x<80&&y>=0&&y<rows)frame[y*80+x]=(unsigned short)(ch|((unsigned)cfg.colors[role]<<8));}
static void fill_words(unsigned short *p,unsigned value,unsigned count);
#pragma aux fill_words = "push es" "push ds" "pop es" "cld" "rep stosw" "pop es" \
 parm [di] [ax] [cx] modify exact [di cx];
static void fill(int x,int y,int w,int h,unsigned ch,int role){
 unsigned short *p,value=(unsigned short)(ch|((unsigned)cfg.colors[role]<<8));
 /* Clip once, then fill contiguous words without a function call per cell. */
 if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
 if(x+w>80)w=80-x;if(y+h>rows)h=rows-y;
 if(w<=0||h<=0)return;p=frame+y*80+x;
 if(w==80){fill_words(p,value,(unsigned)(w*h));return;}
 while(h--){fill_words(p,value,(unsigned)w);p+=80;}
}
static void text(int x,int y,const char *s,int width,int role){
 unsigned short *p,attr=(unsigned)cfg.colors[role]<<8;
 if(y<0||y>=rows||x>=80||width<=0)return;
 while(x<0&&width&&*s){x++;s++;width--;}
 if(x<0)return;if(x+width>80)width=80-x;p=frame+y*80+x;
 while(width--&&*s)*p++=(unsigned char)*s++|attr;
}
static void wrapped(int x,int y,const char *s,int width,int lines,int role){
 int n;while(*s&&lines--){n=(int)strlen(s);if(n>width)n=width;text(x,y++,s,n,role);s+=n;}
}
static void box(int x,int y,int w,int h,const char *title){
 int i;fill(x+1,y+1,w,h,176,DESK);fill(x,y,w,h,' ',PANEL);
 for(i=1;i<w-1;i++){put(x+i,y,196,BORDER);put(x+i,y+h-1,196,BORDER);}
 for(i=1;i<h-1;i++){put(x,y+i,179,BORDER);put(x+w-1,y+i,179,BORDER);}
 put(x,y,218,BORDER);put(x+w-1,y,191,BORDER);put(x,y+h-1,192,BORDER);put(x+w-1,y+h-1,217,BORDER);
 if(title){put(x+2,y,' ',ACCENT);text(x+3,y,title,w-7,ACCENT);put(x+3+(int)strlen(title),y,' ',ACCENT);}
}
static void desktop(const char *title){
 char b[80];fill(0,0,80,rows,176,DESK);fill(0,0,80,2,' ',TITLE);
 text(2,0,"NuXT Menu",20,TITLE);text(17,0,title,60,TITLE);
 sprintf(b,"80 x %d  |  %s  |  %s",rows,theme_names[cfg.theme],mouse_ok?"Keyboard + mouse":"Keyboard");text(2,1,b,76,TITLE);
 fill(0,rows-2,80,2,' ',TITLE);
}
static void flush(void){
 mouseshow(0);_fmemcpy(MK_FP(mono?0xb000:0xb800,0),frame,rows*160);mouseshow(1);
}
/* Swallow the second half of a double-click after opening a folder so it
   cannot activate a child underneath the same pointer position. */
static void guard_click(void){click_guard=1;guard_x=mx;guard_y=my;guard_time=clock();}
static int key(void){
 union REGS r;unsigned k;
 for(;;){
  if(_bios_keybrd(_KEYBRD_READY)){click_guard=0;k=_bios_keybrd(_KEYBRD_READ);return (k&255)&&(k&255)!=224?(int)(k&255):(int)(k&0xff00);}
  if(mouse_ok){
   /* Driver release counters retain clicks that occur between polls/redraws.
      Act on release, so a held button never repeats through nested menus. */
   r.x.ax=6;r.x.bx=1;int86(0x33,&r,&r);
   if(r.x.bx){click_guard=0;return 8;}
   r.x.ax=6;r.x.bx=0;int86(0x33,&r,&r);
   if(r.x.bx){
    mx=r.x.cx/8;my=r.x.dx/8;
    if(click_guard&&mx==guard_x&&my==guard_y&&clock()-guard_time<CLOCKS_PER_SEC/2)continue;
    click_guard=0;return CLICK;
   }
  }
  idle();
 }
}
static void notice(const char *s){strncpy(status,s,sizeof(status)-1);status[sizeof(status)-1]=0;}
static void message(const char *title,const char *s){
 desktop("Program organiser");box(7,7,66,10,title);wrapped(10,10,s,59,4,PANEL);text(10,14,"Press any key",50,ACCENT);flush();key();
}
static int yesno(const char *title,const char *s){
 int k;desktop("Program organiser");box(7,7,66,10,title);wrapped(10,10,s,59,3,PANEL);text(10,14,"Y = yes    N / Esc = cancel",50,ACCENT);flush();
 do{k=key();}while(k!='y'&&k!='Y'&&k!='n'&&k!='N'&&k!=27&&k!=13&&k!=8);return k=='y'||k=='Y';
}
static int input(const char *title,const char *prompt,char *s,unsigned cap){
 unsigned pos=(unsigned)strlen(s),n,offset;int k;char b[82];
 for(;;){
  n=(unsigned)strlen(s);offset=pos>=58?pos-57:0;
  desktop("Edit item");box(6,6,68,12,title);text(9,8,prompt,61,PANEL);fill(9,10,61,1,' ',FIELD);text(9,10,s+offset,60,FIELD);
  sprintf(b,"%u/%u  Enter: accept   Esc: cancel   Ctrl-U: clear",n,cap-1);text(9,13,b,61,DIM);
  text(9,15,"Home/End, arrows, Backspace and Delete edit the field.",61,DIM);flush();cursor(1,9+(int)(pos-offset),10);k=key();cursor(0,0,0);
  if(k==13)return 1;if(k==27)return 0;
  if(k==LEFT&&pos)pos--;else if(k==RIGHT&&pos<n)pos++;else if(k==HOME)pos=0;else if(k==END)pos=n;
  else if(k==21){s[0]=0;pos=0;}else if(k==8&&pos){memmove(s+pos-1,s+pos,n-pos+1);pos--;}
  else if(k==DEL&&pos<n)memmove(s+pos,s+pos+1,n-pos);
  else if(k>=32&&k<=126&&n+1<cap){memmove(s+pos+1,s+pos,n-pos+1);s[pos++]=(char)k;}
 }
}
static int commit(void){
 dirty=1;cfg.current=(unsigned char)current;cfg.selected=count?(unsigned char)members[selected]:ROOT;
 if(save_config(home)){dirty=0;notice("Saved.");return 1;}
 message("Could not save","Check free space and write protection. Changes remain in memory. The previous configuration has been retained.");notice("Unsaved changes - check disk space/write protection.");return 0;
}
static int cmp_members(const void *a,const void *b){
 int x=*(const int *)a,y=*(const int *)b;return cfg.item[x].rank==cfg.item[y].rank?x-y:(cfg.item[x].rank<cfg.item[y].rank?-1:1);
}
static void list_menu(int wanted){
 int i;count=0;for(i=0;i<ITEMS;i++)if(cfg.item[i].kind&&cfg.item[i].parent==current)members[count++]=i;
 qsort(members,count,sizeof(int),cmp_members);selected=0;
 for(i=0;i<count;i++)if(members[i]==wanted)selected=i;
 if(top>selected)top=selected;
}
static void scroll(int *sel,int *start,int n,int page,int k){
 if(k==UP&&*sel>0)(*sel)--;if(k==DOWN&&*sel<n-1)(*sel)++;
 if(k==HOME)*sel=0;if(k==END)*sel=n?n-1:0;
 if(k==PGUP)*sel-=page;if(k==PGDN)*sel+=page;
 if(*sel<0)*sel=0;if(*sel>=n)*sel=n?n-1:0;
 if(*start>*sel)*start=*sel;if(*sel>=*start+page)*start=*sel-page+1;
}
static void draw_menu(void){
 int i,page=rows-10,id;char b[110];Item *e;
 scroll(&selected,&top,count,page,0);desktop(current==ROOT?"Home":cfg.item[current].name);
 box(2,3,38,rows-7,current==ROOT?"Your menus":cfg.item[current].name);box(42,3,36,rows-7,"Selected item");
 if(!count){text(5,6,"This menu is empty.",32,DIM);text(5,8,"F2 adds a program.",32,PANEL);text(5,9,"F3 creates a submenu.",32,PANEL);}
 for(i=0;i<page&&top+i<count;i++){
  id=members[top+i];e=&cfg.item[id];fill(4,5+i,34,1,' ',top+i==selected?SELECT:PANEL);
  text(5,5+i,e->kind==FOLDER?"> ":"  ",2,top+i==selected?SELECT:ACCENT);text(7,5+i,e->name,30,top+i==selected?SELECT:PANEL);
 }
 if(count){
  e=&cfg.item[members[selected]];text(45,5,e->kind==FOLDER?"SUBMENU":"PROGRAM",30,ACCENT);wrapped(45,7,e->name,29,2,PANEL);
  if(e->kind==PROGRAM){text(45,10,"File",29,DIM);wrapped(45,11,e->program,29,3,PANEL);text(45,14,"Working folder",29,DIM);wrapped(45,15,e->cwd,29,2,PANEL);
   if(rows>25){text(45,18,"Arguments",29,DIM);wrapped(45,19,e->args[0]?e->args:"(none)",29,3,PANEL);}
  }else{text(45,11,"Click or Enter to open.",29,PANEL);text(45,13,"Backspace returns to parent.",29,PANEL);}
 }
 fill(0,rows-4,80,1,' ',DESK);sprintf(b," %d item%s | %s",count,count==1?"":"s",status);text(1,rows-4,b,78,PANEL);
 text(1,rows-2,"F1 Help  F2 Add  F3 Submenu  F4 Edit  F5 Theme/screen  F6 Move",78,TITLE);
 text(1,rows-1,"F7/F8 Reorder   Del Remove   F9 DOS prompt   F10 Exit   Esc/Back: up",78,TITLE);flush();
}
static int cmp_files(const void *a,const void *b){
 const DirItem *x=a,*y=b;if(x->dir!=y->dir)return x->dir?-1:1;return stricmp(x->name,y->name);
}
static int contains(const char *s,const char *part){
 unsigned i,j;if(!*part)return 1;for(i=0;s[i];i++){for(j=0;part[j]&&s[i+j]&&toupper(s[i+j])==toupper(part[j]);j++);if(!part[j])return 1;}return 0;
}
static int scan(const char *path){
 struct find_t f;char mask[PATHLEN+5];unsigned error;file_count=0;limited=0;filter_dirty=1;
 sprintf(mask,"%s%s*.*",path,path[strlen(path)-1]=='\\'?"":"\\");
 error=_dos_findfirst(mask,_A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR|_A_ARCH,&f);
 if(error&&error!=2&&error!=18)return 0;
 while(!error){
  if(strcmp(f.name,".")&&strcmp(f.name,"..")&&!(f.attrib&_A_VOLID)){
   if(file_count==MAXFILES){limited=1;break;}
   strncpy(files[file_count].name,f.name,12);files[file_count].name[12]=0;
   files[file_count].dir=(f.attrib&_A_SUBDIR)!=0;files[file_count].size=f.size;file_count++;
  }
  error=_dos_findnext(&f);
 }
 qsort(files,file_count,sizeof(DirItem),cmp_files);return 1;
}
static void filter_files(int all,const char *filter){
 static int last_all;static char last_filter[32];int i;
 if(!filter_dirty&&all==last_all&&!strcmp(filter,last_filter))return;
 filter_dirty=0;last_all=all;strcpy(last_filter,filter);
 visible_count=0;for(i=0;i<file_count;i++)if(files[i].dir||((all||runnable(files[i].name))&&contains(files[i].name,filter)))visible[visible_count++]=i;
}
static void parent_path(char *path){char *p;unsigned n=(unsigned)strlen(path);while(n>3&&path[n-1]=='\\')path[--n]=0;p=strrchr(path,'\\');if(p&&p>path+2)*p=0;else path[3]=0;}
static int browser(char *result){
 char path[PATHLEN],next[PATHLEN],filter[32]="",drive[3],b[100];int sel=0,start=0,k,i,all=0,page,previous=-1;clock_t last=0;DirItem *f;
 strcpy(path,cfg.browse);if(!scan(path)){strcpy(path,home);if(!scan(path)){message("Drive unavailable","The menu folder cannot be read.");return 0;}}
 for(;;){
  filter_files(all,filter);page=rows-10;scroll(&sel,&start,visible_count,page,0);desktop("Find a program");box(2,3,76,rows-6,"Drive explorer");
  text(5,4,path,70,ACCENT);sprintf(b,"%-13s  %-11s  %s", "Name","Size",all?"All files":"EXE / COM / BAT");text(5,5,b,70,DIM);
  for(i=0;i<page&&start+i<visible_count;i++){
   f=&files[visible[start+i]];fill(4,6+i,72,1,' ',sel==start+i?SELECT:PANEL);
   if(f->dir)sprintf(b,"> %-12s  <DIR>",f->name);else sprintf(b,"  %-12s  %10lu bytes",f->name,f->size);
   text(5,6+i,b,69,sel==start+i?SELECT:PANEL);
  }
  sprintf(b,"%d shown%s | Filter: %s",visible_count,limited?" (384-entry limit)":"",filter[0]?filter:"none");text(4,rows-4,b,72,DIM);
  text(1,rows-2,"Enter Open/add   F3 Filter   F4 All/programs   F5 Refresh   F6 Drive",78,TITLE);
  text(1,rows-1,"Backspace Parent folder   Home/End/PgUp/PgDn Scroll   Esc Cancel",78,TITLE);flush();k=key();if(k!=CLICK)previous=-1;
  if(k==27)return 0;
  if(k==CLICK){
   i=my-6;if(mx>=4&&mx<76&&i>=0&&i<page&&start+i<visible_count){sel=start+i;if(files[visible[sel]].dir){k=13;guard_click();}else if(previous==sel&&clock()-last<CLOCKS_PER_SEC)k=13;previous=sel;last=clock();}
  }
  if(k==13&&visible_count){
   f=&files[visible[sel]];if(strlen(path)+strlen(f->name)+2>sizeof(next)){message("Path too long","This version supports DOS paths up to 79 characters.");continue;}
   sprintf(next,"%s%s%s",path,path[strlen(path)-1]=='\\'?"":"\\",f->name);
   if(f->dir){if(scan(next)){strcpy(path,next);strcpy(cfg.browse,path);sel=start=0;previous=-1;}else{scan(path);message("Cannot open folder","The folder or drive is unavailable.");}}
   else if(!runnable(next))message("Choose a program","Select an EXE, COM or BAT file to add to the menu.");
   else if(!absolute_path(next))message("Unsupported path","Use a DOS 8.3 path without spaces, percent signs or shell control characters.");
   else{strcpy(result,next);strcpy(cfg.browse,path);return 1;}
  }else if(k==8){strcpy(next,path);parent_path(next);if(scan(next)){strcpy(path,next);sel=start=0;}else scan(path);}
  else if(k==F3){char temp[32];strcpy(temp,filter);if(input("Filter","Show program names containing:",temp,sizeof(temp)))strcpy(filter,temp);sel=start=0;previous=-1;}
  else if(k==F4){all=!all;sel=start=0;}
  else if(k==F5){if(!scan(path))message("Drive unavailable","Cannot read the directory. Try another drive with F6.");sel=start=0;}
  else if(k==F6){
   drive[0]=path[0];drive[1]=0;
   if(input("Select drive","Drive letter (A through Z):",drive,sizeof(drive))&&strlen(drive)==1&&isalpha(drive[0])){
    sprintf(next,"%c:\\",toupper(drive[0]));if(scan(next)){strcpy(path,next);sel=start=0;previous=-1;}else{scan(path);message("Drive unavailable","No readable DOS drive at that letter, or no disk is inserted.");}
   }
  }else scroll(&sel,&start,visible_count,page,k);
 }
}
static int free_item(void){int i;for(i=0;i<ITEMS;i++)if(!cfg.item[i].kind)return i;message("Menu full","This version holds 128 programs and submenus in total.");return -1;}
static void rank_new(Item *e){int i;for(i=0;i<count;i++)cfg.item[members[i]].rank=(unsigned short)i;e->rank=(unsigned short)count;}
static void add_program(void){
 Item e;int id=free_item();char *p;if(id<0)return;memset(&e,0,sizeof(e));if(!browser(e.program))return;
 strcpy(e.cwd,e.program);parent_path(e.cwd);p=strrchr(e.program,'\\');strncpy(e.name,p?p+1:e.program,31);p=strrchr(e.name,'.');if(p)*p=0;
 if(!input("Add program","Menu name:",e.name,sizeof(e.name))||!e.name[0])return;
 if(!input("Add program","Command-line arguments (optional):",e.args,sizeof(e.args)))return;
 e.kind=PROGRAM;e.parent=(unsigned char)current;rank_new(&e);cfg.item[id]=e;list_menu(id);commit();
}
static void add_folder(void){
 Item e;int id=free_item();if(id<0)return;memset(&e,0,sizeof(e));
 if(!input("New submenu","Submenu name:",e.name,sizeof(e.name))||!e.name[0])return;
 e.kind=FOLDER;e.parent=(unsigned char)current;rank_new(&e);cfg.item[id]=e;list_menu(id);commit();
}
static void edit_item(void){
 Item e;char pause[2];int id;if(!count)return;id=members[selected];e=cfg.item[id];
 if(!input("Edit item","Menu name:",e.name,sizeof(e.name))||!e.name[0])return;
 if(e.kind==PROGRAM){
  if(!input("Edit program","Full DOS program path:",e.program,sizeof(e.program)))return;
  if(!input("Edit program","Command-line arguments:",e.args,sizeof(e.args)))return;
  if(!input("Edit program","Working directory:",e.cwd,sizeof(e.cwd)))return;
  pause[0]=e.pause?'Y':'N';pause[1]=0;if(!input("Edit program","Pause after exit? Y or N:",pause,sizeof(pause)))return;e.pause=toupper(pause[0])=='Y';
  if(!absolute_path(e.program)||!runnable(e.program)||!absolute_path(e.cwd)){message("Check the paths","Use full DOS 8.3 paths, e.g. C:\\GAMES\\GAME.EXE and C:\\GAMES.");return;}
 }
 cfg.item[id]=e;commit();
}
static void delete_item(void){
 int i,id;if(!count)return;id=members[selected];for(i=0;i<ITEMS;i++)if(cfg.item[i].kind&&cfg.item[i].parent==id){message("Submenu is not empty","Move or remove its entries first. No program files will be deleted.");return;}
 if(!yesno("Remove menu item?","This removes only the menu entry. The program and its files stay on disk."))return;
 cfg.item[id].kind=0;list_menu(ROOT);commit();
}
static void move_item(void){
 int choices[ITEMS+1],n=1,i,id,sel=0,start=0,k,page;char b[100];if(!count)return;id=members[selected];choices[0]=ROOT;
 for(i=0;i<ITEMS;i++)if(cfg.item[i].kind==FOLDER&&!child_of(i,id))choices[n++]=i;
 for(i=0;i<n;i++)if(choices[i]==cfg.item[id].parent)sel=i;
 for(;;){
  page=rows-10;scroll(&sel,&start,n,page,0);desktop("Move entry to a submenu");box(6,3,68,rows-6,"Destination");
  for(i=0;i<page&&start+i<n;i++){
   int f=choices[start+i];if(f==ROOT)strcpy(b,"Home");else sprintf(b,"%s / %s",cfg.item[f].parent==ROOT?"Home":cfg.item[cfg.item[f].parent].name,cfg.item[f].name);
   fill(8,5+i,64,1,' ',sel==start+i?SELECT:PANEL);text(9,5+i,b,61,sel==start+i?SELECT:PANEL);
  }
  text(2,rows-2,"Enter: move here     Esc: cancel",76,TITLE);flush();k=key();if(k==27)return;
  if(k==13){int parent=choices[sel];unsigned rank=0;for(i=0;i<ITEMS;i++)if(cfg.item[i].kind&&cfg.item[i].parent==parent)cfg.item[i].rank=(unsigned short)rank++;
   cfg.item[id].parent=(unsigned char)parent;cfg.item[id].rank=(unsigned short)rank;list_menu(ROOT);commit();return;
  }
  scroll(&sel,&start,n,page,k);
 }
}
static void settings(void){
 unsigned char oldmode=cfg.mode,oldtheme=cfg.theme,oldcolors[8];int sel=0,k,i,change;char b[90];memcpy(oldcolors,cfg.colors,8);
 for(;;){
  desktop("Appearance");box(5,3,70,17,"Screen and colours");
  for(i=0;i<10;i++){
   if(i==0)sprintf(b,"Screen layout       80 x %d%s",cfg.mode==1?43:cfg.mode==2?50:25,vga?"":" (VGA needed for 43/50)");
   else if(i==1)sprintf(b,"Theme               %s",theme_names[cfg.theme]);
   else sprintf(b,"%-18s  Ink %2u  Paper %2u    Sample text",roles[i-2],cfg.colors[i-2]&15,cfg.colors[i-2]>>4);
   fill(7,5+i,66,1,' ',i==sel?SELECT:PANEL);text(8,5+i,b,62,i==sel?SELECT:PANEL);
   if(i>=2)text(48,5+i," Sample text ",14,i-2);
  }
  text(8,17,"Left/right: choice or ink   PgUp/PgDn: paper colour",62,DIM);text(8,18,"S or F10: save    Esc: undo appearance changes",62,ACCENT);
  text(2,rows-2,"Arrow keys select settings. Changes preview immediately.",76,TITLE);flush();k=key();
  if(k==27){cfg.mode=oldmode;cfg.theme=oldtheme;memcpy(cfg.colors,oldcolors,8);mode(cfg.mode);return;}
  if(k=='s'||k=='S'||k==F10){commit();return;}
  if(k==UP&&sel)sel--;if(k==DOWN&&sel<9)sel++;
  if(k==CLICK&&mx>=7&&mx<73&&my>=5&&my<15)sel=my-5;
  change=(k==RIGHT||k==13)?1:k==LEFT?-1:0;
  if(sel==0&&change){if(vga){cfg.mode=(unsigned char)((cfg.mode+change+3)%3);mode(cfg.mode);}}
  else if(sel==1&&change){cfg.theme=(unsigned char)((cfg.theme+change+7)%7);if(cfg.theme<6)memcpy(cfg.colors,themes[cfg.theme],8);}
  else if(sel>=2){
   if(change){cfg.colors[sel-2]=(unsigned char)((cfg.colors[sel-2]&0xf0)|((cfg.colors[sel-2]+change)&15));cfg.theme=6;}
   if(k==PGUP||k==PGDN){cfg.colors[sel-2]=(unsigned char)(cfg.colors[sel-2]+(k==PGUP?16:-16));cfg.theme=6;}
  }
 }
}
static int launch(int shell){
 Item *e=count?&cfg.item[members[selected]]:NULL;struct find_t f;char job[100],wd[PATHLEN],*command;FILE *out;int ok;
 if(!shell){
  if(!e||e->kind!=PROGRAM)return 0;
  if(strlen(e->program)+strlen(e->args)+(e->args[0]?1:0)+(!stricmp(strrchr(e->program,'.'),".BAT")?5:0)>125){message("Command line too long","Shorten the program path or arguments. DOS COMMAND.COM has a limited batch command-line length.");return 0;}
  if(_dos_findfirst(e->program,_A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR|_A_ARCH,&f)||(f.attrib&_A_SUBDIR)){message("Program not found","Edit this entry with F4, or browse to the correct file with F2.");return 0;}
  if(!getcwd(wd,sizeof(wd))){message("Directory error","Cannot read the working directory.");return 0;}
  {unsigned n=(unsigned)strlen(e->cwd);while(n>3&&e->cwd[n-1]=='\\')e->cwd[--n]=0;}
  ok=!_chdrive(toupper(e->cwd[0])-'A'+1)&&!chdir(e->cwd);_chdrive(toupper(wd[0])-'A'+1);chdir(wd);
  if(!ok){message("Working folder unavailable","Check the working directory with F4 and verify that its disk is inserted.");return 0;}
 }
 if(!commit())return 0;
 sprintf(job,"%sNM-RUN.BAT",home);out=fopen(job,"wt");if(!out){message("Launch failed","Cannot create NM-RUN.BAT in the menu folder.");return 0;}
 fprintf(out,"@echo off\n");
 if(shell){command=getenv("COMSPEC");if(!command||!safe_path(command)){fclose(out);remove(job);message("COMSPEC error","COMSPEC must contain a DOS path to COMMAND.COM.");return 0;}fprintf(out,"%s\n",command);}
 else{
  fprintf(out,"%c:\ncd %s\n",toupper(e->cwd[0]),e->cwd);
  fprintf(out,"%s%s%s%s\n",!stricmp(strrchr(e->program,'.'),".BAT")?"call ":"",e->program,e->args[0]?" ":"",e->args);
  if(e->pause)fprintf(out,"pause\n");
 }
 ok=!ferror(out);if(fclose(out))ok=0;if(!ok){remove(job);message("Launch failed","Could not write the launch job. Check disk space.");return 0;}return 1;
}
static void help(void){
 int k;desktop("Quick guide");box(4,3,72,18,"NuXT Menu 1.2");
 text(7,5,"Enter opens a submenu or launches a selected program.",65,PANEL);
 text(7,6,"Click folders; double-click programs; right-click goes back.",65,PANEL);
 text(7,8,"F2  Browse drives and add EXE / COM / BAT files.",65,ACCENT);
 text(7,9,"F3  Create a submenu inside the current menu.",65,PANEL);
 text(7,10,"F4  Edit name, file, arguments, working folder and pause.",65,PANEL);
 text(7,11,"F5  Choose 80x25 / 43 / 50, theme and custom colours.",65,PANEL);
 text(7,12,"F6  Move entry.  F7 / F8 reorder it within this menu.",65,PANEL);
 text(7,13,"Del removes the entry only; no program files are deleted.",65,PANEL);
 text(7,15,"F9 opens DOS. Type EXIT to return. F10 exits the menu.",65,ACCENT);
 text(7,16,"Typed letters jump to an entry. Ctrl-U clears edit fields.",65,PANEL);
 text(7,18,"Configuration saves next to MENU.EXE, with a .BAK copy.",65,DIM);
 text(7,19,"Run MENU /SAFE to recover readable default colours/layout.",65,DIM);
 text(2,rows-2,"Press any key to return.",76,TITLE);flush();k=key();(void)k;
}
int main(int argc,char **argv){
 union REGS r;char exe[260],*p;int i,k,rc=0,driver=0,safe=0,override=-1,loaded,previous=-1;clock_t last=0;
 for(i=1;i<argc;i++){
  if(!stricmp(argv[i],"/DRIVER"))driver=1;else if(!stricmp(argv[i],"/SAFE"))safe=1;
  else if(!stricmp(argv[i],"/25"))override=0;else if(!stricmp(argv[i],"/43"))override=1;else if(!stricmp(argv[i],"/50"))override=2;
  else{puts("NuXT Menu: MENU [/25|/43|/50] [/SAFE]\nKeep MENU.EXE and NUI.EXE together. VGA enables 43/50 rows.");return 0;}
 }
 if(!driver){puts("Please run MENU.EXE; it releases the interface memory before launching games.");return 1;}
 if(!_cmdname(exe)||!(p=strrchr(exe,'\\')))return 1;p[1]=0;
 if(strlen(exe)>68||!absolute_path(exe)){puts("Install in a short DOS folder, e.g. C:\\NUXMENU.");return 1;}strcpy(home,exe);
 _harderr(hard);loaded=load_config(home);
 if(loaded<0){puts("NUXMENU.DAT and its backup are unreadable. Restore a backup, or rename\nNUXMENU.DAT to start a new menu. Existing files were not changed.");return 1;}
 if(!loaded)defaults(home);if(loaded==2)notice("Recovered configuration from NUXMENU.BAK.");
 if(safe){cfg.mode=0;cfg.theme=0;memcpy(cfg.colors,themes[0],8);}if(override>=0)cfg.mode=(unsigned char)override;
 current=cfg.current;list_menu(cfg.selected);
 r.h.ah=15;int86(0x10,&r,&r);mono=(r.h.al&127)==7;
 r.x.ax=0x1a00;int86(0x10,&r,&r);vga=r.h.al==0x1a;
 if(vga){original_rows=*(unsigned char __far *)MK_FP(0x40,0x84)+1;if(original_rows!=43&&original_rows!=50)original_rows=25;}
 {unsigned char __far *vector=(unsigned char __far *)_dos_getvect(0x33);
  if(vector&&*vector!=0xcf){r.x.ax=0;int86(0x33,&r,&r);mouse_ok=r.x.ax==0xffff;}
 }mode(cfg.mode);
 for(;;){
  draw_menu();k=key();if(k!=CLICK)previous=-1;
  if(k==CLICK){
   i=my-5;if(mx>=4&&mx<38&&i>=0&&i<rows-10&&top+i<count){selected=top+i;if(cfg.item[members[selected]].kind==FOLDER){k=13;guard_click();}else if(previous==selected&&clock()-last<CLOCKS_PER_SEC)k=13;previous=selected;last=clock();}
   else if(my==rows-2){if(mx<9)k=F1;else if(mx<17)k=F2;else if(mx<29)k=F3;else if(mx<38)k=F4;else if(mx<56)k=F5;else k=F6;}
  }
  if(k==F10||(k==27&&current==ROOT)){
   if(commit()||yesno("Exit without saving?","Your latest edits could not be saved. Discard those unsaved edits and exit?"))break;
  }else if(k==27||k==8||k==LEFT){if(current!=ROOT){i=current;current=cfg.item[current].parent;top=0;list_menu(i);}}
  else if((k==13||k==RIGHT)&&count){i=members[selected];if(cfg.item[i].kind==FOLDER){current=i;top=0;list_menu(ROOT);previous=-1;}else if(k==13&&launch(0)){rc=10;break;}}
  else if(k==F1)help();else if(k==F2)add_program();else if(k==F3)add_folder();else if(k==F4)edit_item();
  else if(k==F5)settings();else if(k==F6)move_item();else if(k==DEL)delete_item();else if(k==F9&&launch(1)){rc=10;break;}
  else if((k==F7||k==F8)&&count){i=selected+(k==F7?-1:1);if(i>=0&&i<count){unsigned short rank=cfg.item[members[i]].rank;cfg.item[members[i]].rank=cfg.item[members[selected]].rank;cfg.item[members[selected]].rank=rank;i=members[selected];list_menu(i);commit();}}
  else if(k>=32&&k<=126&&count){for(i=1;i<=count;i++){int n=(selected+i)%count;if(toupper(cfg.item[members[n]].name[0])==toupper(k)){selected=n;break;}}}
  else scroll(&selected,&top,count,rows-10,k);
 }
 mouseshow(0);mode(original_rows==43?1:original_rows==50?2:0);if(vga){r.x.ax=0x1003;r.x.bx=1;int86(0x10,&r,&r);}cursor(1,0,0);return rc;
}
