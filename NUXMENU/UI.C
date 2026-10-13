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
#include <malloc.h>

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
#define BACK_CLICK 0x1001
#define FAV_VIEW 6
#define DUPLICATE 4
#define SAVE 19
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
static unsigned short __far *shown;
static int screen_valid;
static unsigned last_writes;
typedef struct {unsigned char x,y,w;int action;} Button;
static Button buttons[32];
static int button_count;
static int favourites;
static int menu_valid,menu_selection,menu_top,menu_count,menu_current,menu_favourites;
static char menu_status[79];
static int rows=25,vga=0,mono=0,original_rows=25,mouse_ok=0,mouse_visible=0,mx,my;
static int click_guard=0,guard_x,guard_y;
static clock_t guard_time;
static int current=ROOT,selected=0,top=0,members[ITEMS+1],count=0,dirty=0;
static char home[PATHLEN],status[79]="";
static const char *theme_names[11]={"Classic blue","Amber terminal","Matrix green","Neon coast","Slate","Paper","Phosphor","Ice Station","Plum CRT","Red Alert","Custom"};
static const unsigned char themes[10][8]={
 {0x17,0x1f,0x70,0x1b,0x3f,0x17,0x0f,0x19},
 {0x06,0x0e,0x60,0x0f,0x60,0x06,0x0e,0x06},
 {0x02,0x0a,0x20,0x0f,0x20,0x02,0x0a,0x02},
 {0x10,0x1b,0x5f,0x1d,0x5f,0x13,0x0b,0x15},
 {0x08,0x07,0x3f,0x0b,0x70,0x08,0x0f,0x08},
 {0x70,0x70,0x1f,0x71,0x1f,0x78,0x0f,0x78},
 {0x00,0x02,0x20,0x0a,0x02,0x08,0x0a,0x02},
 {0x10,0x1b,0x3f,0x1f,0x30,0x13,0x0f,0x1b},
 {0x50,0x5e,0x70,0x5f,0x70,0x57,0x0e,0x5d},
 {0x00,0x07,0x4f,0x0c,0x4f,0x08,0x0f,0x04}
};
static const char *roles[8]={"Desktop","Panel text","Selection","Accent","Title bar","Muted text","Input field","Borders"};
typedef struct {char name[13];unsigned char dir;unsigned long size;} DirItem;
static DirItem __far files[MAXFILES];
static int file_order[MAXFILES];
static int visible[MAXFILES],file_count,visible_count,limited,filter_dirty=1;

static int saver_active,allow_idle,joy_context,wake_mouse;
static unsigned long activity_tick,ui_tick;
static int clock_hour=-1,clock_minute=-1,clock_format=-1;
static int clock_draw(int force);
static void idle(void);
#pragma aux idle = "sti" "hlt";
static int __far hard(unsigned a,unsigned b,unsigned __far *c){(void)a;(void)b;(void)c;_hardresume(_HARDERR_FAIL);return 0;}
static void mouseshow(int yes){
 union REGS r;if(saver_active)yes=0;if(!mouse_ok||yes==mouse_visible)return;
 r.x.ax=yes?1:2;int86(0x33,&r,&r);mouse_visible=yes;
}
static void cursor(int yes,int x,int y){
 union REGS r;r.h.ah=1;r.x.cx=yes?0x0607:0x2000;int86(0x10,&r,&r);
 if(yes){r.h.ah=2;r.h.bh=0;r.h.dl=(unsigned char)x;r.h.dh=(unsigned char)y;int86(0x10,&r,&r);}
}
static void mode(int wanted){
 union REGS r;unsigned char actual;
 mouseshow(0);screen_valid=menu_valid=0;if(!vga)wanted=0;
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
 char b[80];button_count=0;menu_valid=0;fill(0,0,80,rows,176,DESK);fill(0,0,80,2,' ',TITLE);
 text(2,0,"NuXT Menu",20,TITLE);text(17,0,title,49,TITLE);clock_draw(1);
 sprintf(b,"80 x %d  |  %s  |  %s",rows,theme_names[cfg.theme],mouse_ok?"Keyboard + mouse":"Keyboard");text(2,1,b,76,TITLE);
 fill(0,rows-2,80,2,' ',TITLE);
}
static void button(int x,int y,const char *label,int action){
 int w=(int)strlen(label);text(x,y,label,w,TITLE);
 if(button_count<32){Button *b=&buttons[button_count++];b->x=(unsigned char)x;b->y=(unsigned char)y;b->w=(unsigned char)w;b->action=action;}
}
static int hit_button(int x,int y){
 int i;for(i=0;i<button_count;i++)if(y==buttons[i].y&&x>=buttons[i].x&&x<buttons[i].x+buttons[i].w)return buttons[i].action;
 return CLICK;
}
static void flush(void){
 unsigned i,end,n=(unsigned)rows*80;unsigned short __far *video=MK_FP(mono?0xb000:0xb800,0);
 last_writes=0;
 /* Compare ordinary RAM, never read slow video RAM or the mouse cursor.
    The shadow belongs to NUI only and is released before a child launches. */
 if(!screen_valid||!shown){
  mouseshow(0);_fmemcpy(video,frame,n*2);if(shown)_fmemcpy(shown,frame,n*2);
  last_writes=n;screen_valid=1;mouseshow(1);return;
 }
 for(i=0;i<n;){
  if(shown[i]==frame[i]){i++;continue;}
  end=i+1;while(end<n&&shown[end]!=frame[end])end++;
  if(!last_writes)mouseshow(0);
  _fmemcpy(video+i,frame+i,(end-i)*2);_fmemcpy(shown+i,frame+i,(end-i)*2);
  last_writes+=end-i;i=end;
 }
 if(last_writes)mouseshow(1);
}
/* Swallow the second half of a double-click after opening a folder so it
   cannot activate a child underneath the same pointer position. */
static void guard_click(void){click_guard=1;guard_x=mx;guard_y=my;guard_time=clock();}
#include "IDLE.H"
static void notice(const char *s){strncpy(status,s,sizeof(status)-1);status[sizeof(status)-1]=0;}
static void message(const char *title,const char *s){
 desktop("Program organiser");box(7,7,66,10,title);wrapped(10,10,s,59,4,PANEL);button(10,14,"[ OK ]",27);flush();key();
}
static int yesno(const char *title,const char *s){
 int k;desktop("Program organiser");box(7,7,66,10,title);wrapped(10,10,s,59,3,PANEL);button(10,14,"[ Yes ]",'y');button(24,14,"[ Cancel ]",27);flush();
 do{k=key();}while(k!='y'&&k!='Y'&&k!='n'&&k!='N'&&k!=27&&k!=13&&k!=8&&k!=BACK_CLICK);return k=='y'||k=='Y';
}
static int input(const char *title,const char *prompt,char *s,unsigned cap){
 unsigned pos=(unsigned)strlen(s),n,offset;int k;char b[82];
 for(;;){
  n=(unsigned)strlen(s);offset=pos>=58?pos-57:0;
  desktop("Edit item");box(6,6,68,12,title);text(9,8,prompt,61,PANEL);fill(9,10,61,1,' ',FIELD);text(9,10,s+offset,60,FIELD);
  sprintf(b,"%u/%u  Enter: accept   Esc: cancel   Ctrl-U: clear",n,cap-1);text(9,13,b,61,DIM);
  text(9,15,"Home/End, arrows, Backspace and Delete edit the field.",61,DIM);button(9,16,"[ Save ]",13);button(22,16,"[ Cancel ]",27);button(37,16,"[ Clear ]",21);flush();cursor(1,9+(int)(pos-offset),10);k=key();cursor(0,0,0);
  if(k==13)return 1;if(k==27||k==BACK_CLICK)return 0;
  if(k==LEFT&&pos)pos--;else if(k==RIGHT&&pos<n)pos++;else if(k==HOME)pos=0;else if(k==END)pos=n;
  else if(k==21){s[0]=0;pos=0;}else if(k==8&&pos){memmove(s+pos-1,s+pos,n-pos+1);pos--;}
  else if(k==DEL&&pos<n)memmove(s+pos,s+pos+1,n-pos);
  else if(k>=32&&k<=126&&n+1<cap){memmove(s+pos+1,s+pos,n-pos+1);s[pos++]=(char)k;}
 }
}
static int place_index(void){return favourites?129:current==ROOT?128:current;}
static void remember_place(void){int n=place_index();cfg.opt.selection[n]=count?(unsigned char)members[selected]:ROOT;cfg.opt.scroll[n]=(unsigned char)top;}
static int commit(void){
 remember_place();cfg.opt.lastfav=(unsigned char)favourites;menu_valid=0;dirty=1;cfg.current=(unsigned char)current;cfg.selected=count&&members[selected]!=FAV_ID?(unsigned char)members[selected]:ROOT;
 if(save_config(home)){dirty=0;notice("Saved.");return 1;}
 message("Could not save","Check free space and write protection. Changes remain in memory. The previous configuration has been retained.");notice("Unsaved changes - check disk space/write protection.");return 0;
}
static int cmp_members(const void *a,const void *b){
 int x=*(const int *)a,y=*(const int *)b;
 if(favourites){int n=stricmp(cfg.item[x].name,cfg.item[y].name);return n?n:x-y;}
 return cfg.item[x].rank==cfg.item[y].rank?x-y:(cfg.item[x].rank<cfg.item[y].rank?-1:1);
}
static void list_menu(int wanted){
 int i;menu_valid=0;count=0;for(i=0;i<ITEMS;i++)if(favourites?(cfg.item[i].kind==PROGRAM&&(cfg.item[i].pause&FAVOURITE)):(cfg.item[i].kind&&cfg.item[i].parent==current))members[count++]=i;
 qsort(members,count,sizeof(int),cmp_members);if(!favourites&&current==ROOT){memmove(members+1,members,count*sizeof(int));members[0]=FAV_ID;count++;}selected=0;
 for(i=0;i<count;i++)if(members[i]==wanted)selected=i;
 if(top>selected)top=selected;
}
static void enter_menu(int parent,int fav){
 int n;remember_place();joy_settle=1;current=parent;favourites=fav;n=place_index();top=cfg.opt.scroll[n];list_menu(cfg.opt.selection[n]);
}
static void scroll(int *sel,int *start,int n,int page,int k){
 if(k==UP&&*sel>0)(*sel)--;if(k==DOWN&&*sel<n-1)(*sel)++;
 if(k==HOME)*sel=0;if(k==END)*sel=n?n-1:0;
 if(k==PGUP)*sel-=page;if(k==PGDN)*sel+=page;
 if(*sel<0)*sel=0;if(*sel>=n)*sel=n?n-1:0;
 if(*start>*sel)*start=*sel;if(*sel>=*start+page)*start=*sel-page+1;
}
static void menu_row(int row){
 int id=members[top+row],role=top+row==selected?SELECT:PANEL;Item *e;
 fill(4,5+row,34,1,' ',role);
 if(id==FAV_ID){text(5,5+row,"> Favourites",32,role);return;}e=&cfg.item[id];
 text(5,5+row,e->kind==FOLDER?"> ":(e->pause&FAVOURITE)?"* ":"  ",2,role);
 text(7,5+row,e->name,30,role);
}
static void menu_details(void){
 Item *e;fill(43,4,34,rows-9,' ',PANEL);if(!count)return;
 if(members[selected]==FAV_ID){text(45,5,"SUBMENU",30,ACCENT);text(45,7,"Favourites",29,PANEL);text(45,11,"Your starred programs.",29,PANEL);return;}
 e=&cfg.item[members[selected]];text(45,5,e->kind==FOLDER?"SUBMENU":"PROGRAM",30,ACCENT);wrapped(45,7,e->name,29,2,PANEL);
 if(e->kind==PROGRAM){
  text(45,10,"File",29,DIM);wrapped(45,11,e->program,29,3,PANEL);
  text(45,14,"Working folder",29,DIM);wrapped(45,15,e->cwd,29,2,PANEL);
  if(rows>25){text(45,18,"Arguments",29,DIM);wrapped(45,19,e->args[0]?e->args:"(none)",29,3,PANEL);}
 }else{text(45,11,"Click or Enter to open.",29,PANEL);text(45,13,"Backspace returns to parent.",29,PANEL);}
}
static void draw_menu(void){
 int i,page=rows-10;char b[110];
 scroll(&selected,&top,count,page,0);
 if(menu_valid&&menu_top==top&&menu_count==count&&menu_current==current&&menu_favourites==favourites&&!strcmp(menu_status,status)){
  if(menu_selection!=selected){menu_row(menu_selection-top);menu_row(selected-top);menu_details();}
 }else{
  desktop(favourites?"Favourites":current==ROOT?"Home":cfg.item[current].name);
  box(2,3,38,rows-7,favourites?"Favourites":current==ROOT?"Your menus":cfg.item[current].name);box(42,3,36,rows-7,"Selected item");
  if(!count){text(5,6,favourites?"No favourites yet.":"This menu is empty.",32,DIM);text(5,8,favourites?"Use * to star a program.":"F2 adds a program.",32,PANEL);if(!favourites)text(5,9,"F3 creates a submenu.",32,PANEL);}
  for(i=0;i<page&&top+i<count;i++)menu_row(i);
  menu_details();
  fill(0,rows-4,80,1,' ',DESK);sprintf(b," %d item%s | %s",count,count==1?"":"s",status);text(1,rows-4,b,78,PANEL);
  fill(0,rows-3,80,1,' ',TITLE);
  button(1,rows-3,"[Back]",BACK_CLICK);button(8,rows-3,"[^]",UP);button(12,rows-3,"[v]",DOWN);
  button(16,rows-3,"[PgUp]",PGUP);button(23,rows-3,"[PgDn]",PGDN);
  button(1,rows-1,"F1 Help",F1);button(16,rows-1,"F5 Settings",F5);
  button(35,rows-1,"F9 DOS prompt",F9);button(57,rows-1,"F10 Exit",F10);
 }
 menu_valid=1;menu_selection=selected;menu_top=top;menu_count=count;menu_current=current;menu_favourites=favourites;strcpy(menu_status,status);flush();
}
static int cmp_files(const void *a,const void *b){
 DirItem x,y;_fmemcpy(&x,&files[*(const int *)a],sizeof(x));_fmemcpy(&y,&files[*(const int *)b],sizeof(y));if(x.dir!=y.dir)return x.dir?-1:1;return stricmp(x.name,y.name);
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
   _fmemcpy(files[file_count].name,f.name,13);file_order[file_count]=file_count;files[file_count].name[12]=0;
   files[file_count].dir=(f.attrib&_A_SUBDIR)!=0;files[file_count].size=f.size;file_count++;
  }
  error=_dos_findnext(&f);
 }
 qsort(file_order,file_count,sizeof(int),cmp_files);return 1;
}
static void filter_files(int all,const char *filter){
 static int last_all;static char last_filter[32];int i,j;DirItem f;
 if(!filter_dirty&&all==last_all&&!strcmp(filter,last_filter))return;
 filter_dirty=0;last_all=all;strcpy(last_filter,filter);
 visible_count=0;for(i=0;i<file_count;i++){j=file_order[i];_fmemcpy(&f,&files[j],sizeof(f));if(f.dir||((all||runnable(f.name))&&contains(f.name,filter)))visible[visible_count++]=j;}
}
static void parent_path(char *path){char *p;unsigned n=(unsigned)strlen(path);while(n>3&&path[n-1]=='\\')path[--n]=0;p=strrchr(path,'\\');if(p&&p>path+2)*p=0;else path[3]=0;}
static int browser(char *result){
 char path[PATHLEN],next[PATHLEN],filter[32]="",drive[3],b[100];int sel=0,start=0,k,i,all=0,page,previous=-1;clock_t last=0;DirItem entry,*f=&entry;
 strcpy(path,cfg.browse);if(!scan(path)){strcpy(path,home);if(!scan(path)){message("Drive unavailable","The menu folder cannot be read.");return 0;}}
 for(;;){
  filter_files(all,filter);page=rows-10;scroll(&sel,&start,visible_count,page,0);desktop("Find a program");box(2,3,76,rows-6,"Drive explorer");
  text(5,4,path,70,ACCENT);sprintf(b,"%-13s  %-11s  %s", "Name","Size",all?"All files":"EXE / COM / BAT");text(5,5,b,70,DIM);
  for(i=0;i<page&&start+i<visible_count;i++){
   _fmemcpy(f,&files[visible[start+i]],sizeof(*f));fill(4,6+i,72,1,' ',sel==start+i?SELECT:PANEL);
   if(f->dir)sprintf(b,"> %-12s  <DIR>",f->name);else sprintf(b,"  %-12s  %10lu bytes",f->name,f->size);
   text(5,6+i,b,69,sel==start+i?SELECT:PANEL);
  }
  sprintf(b,"%d shown%s | Filter: %s",visible_count,limited?" (384-entry limit)":"",filter[0]?filter:"none");text(4,rows-4,b,72,DIM);
  fill(0,rows-3,80,1,' ',TITLE);
  button(1,rows-3,"[Back]",8);button(10,rows-3,"[^]",UP);button(15,rows-3,"[v]",DOWN);button(20,rows-3,"[PgUp]",PGUP);button(29,rows-3,"[PgDn]",PGDN);button(65,rows-3,"[Cancel]",27);
  button(1,rows-2,"[Open/add]",13);button(13,rows-2,"F3 Filter",F3);button(25,rows-2,"F4 All/programs",F4);button(44,rows-2,"F5 Refresh",F5);button(58,rows-2,"F6 Drive",F6);
  text(1,rows-1,"Backspace Parent folder   Home/End/PgUp/PgDn Scroll   Esc Cancel",78,TITLE);flush();k=key();if(k!=CLICK)previous=-1;
  if(k==27)return 0;if(k==BACK_CLICK)k=8;
  if(k==CLICK){
   i=my-6;if(mx>=4&&mx<76&&i>=0&&i<page&&start+i<visible_count){sel=start+i;if(files[visible[sel]].dir){k=13;guard_click();}else if(previous==sel&&clock()-last<CLOCKS_PER_SEC)k=13;previous=sel;last=clock();}else previous=-1;
  }
  if(k==13&&visible_count){
   _fmemcpy(f,&files[visible[sel]],sizeof(*f));if(strlen(path)+strlen(f->name)+2>sizeof(next)){message("Path too long","This version supports DOS paths up to 79 characters.");continue;}
   sprintf(next,"%s%s%s",path,path[strlen(path)-1]=='\\'?"":"\\",f->name);
   if(f->dir){if(scan(next)){strcpy(path,next);sel=start=0;previous=-1;}else{scan(path);message("Cannot open folder","The folder or drive is unavailable.");}}
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

static void add_program(void){
 Item e;int id=free_item();char *p;if(id<0)return;memset(&e,0,sizeof(e));if(!browser(e.program))return;
 strcpy(e.cwd,e.program);parent_path(e.cwd);p=strrchr(e.program,'\\');strncpy(e.name,p?p+1:e.program,31);p=strrchr(e.name,'.');if(p)*p=0;
 if(!input("Add program","Menu name:",e.name,sizeof(e.name))||!e.name[0])return;
 if(!input("Add program","Command-line arguments (optional):",e.args,sizeof(e.args)))return;
 e.kind=PROGRAM;e.parent=(unsigned char)current;if(favourites)e.pause|=FAVOURITE;cfg.item[id]=e;append_rank(id,current);list_menu(id);commit();
}
static void add_folder(void){
 Item e;int id=free_item();if(id<0)return;memset(&e,0,sizeof(e));
 if(!input("New submenu","Submenu name:",e.name,sizeof(e.name))||!e.name[0])return;
 e.kind=FOLDER;e.parent=(unsigned char)current;cfg.item[id]=e;append_rank(id,current);list_menu(id);commit();
}
static void edit_item(void){
 Item e;char pause[2];int id;if(!count)return;id=members[selected];if(id==FAV_ID)return;e=cfg.item[id];
 if(!input("Edit item","Menu name:",e.name,sizeof(e.name))||!e.name[0])return;
 if(e.kind==PROGRAM){
  if(!input("Edit program","Full DOS program path:",e.program,sizeof(e.program)))return;
  if(!input("Edit program","Command-line arguments:",e.args,sizeof(e.args)))return;
  if(!input("Edit program","Working directory:",e.cwd,sizeof(e.cwd)))return;
  pause[0]=(e.pause&PAUSE_EXIT)?'Y':'N';pause[1]=0;if(!input("Edit program","Pause after exit? Y or N:",pause,sizeof(pause)))return;e.pause=(unsigned char)((e.pause&FAVOURITE)|(toupper(pause[0])=='Y'?PAUSE_EXIT:0));
  if(!absolute_path(e.program)||!runnable(e.program)||!absolute_path(e.cwd)){message("Check the paths","Use full DOS 8.3 paths, e.g. C:\\GAMES\\GAME.EXE and C:\\GAMES.");return;}
 }
 cfg.item[id]=e;list_menu(id);commit();
}
static void delete_item(void){
 int i,id;if(!count)return;id=members[selected];if(id==FAV_ID)return;for(i=0;i<ITEMS;i++)if(cfg.item[i].kind&&cfg.item[i].parent==id){message("Submenu is not empty","Move or remove its entries first. No program files will be deleted.");return;}
 if(!yesno("Remove menu item?","This removes only the menu entry. The program and its files stay on disk."))return;
 cfg.item[id].kind=0;list_menu(ROOT);commit();
}
static void duplicate_item(void){
 int id,source;Item e;char name[32];if(!count)return;source=members[selected];if(source==FAV_ID)return;
 if(cfg.item[source].kind!=PROGRAM){notice("Select a program to duplicate.");return;}
 id=free_item();if(id<0)return;e=cfg.item[source];
 sprintf(name,"%.24s (copy)",e.name);
 if(!input("Duplicate program","Name for the copy:",name,sizeof(name))||!name[0])return;
 strcpy(e.name,name);cfg.item[id]=e;append_rank(id,e.parent);list_menu(id);commit();
}
static void toggle_favourites(void){
 if(favourites)enter_menu(ROOT,0);else enter_menu(ROOT,1);
}
static void star_item(void){
 int id;if(!count)return;id=members[selected];if(id==FAV_ID)return;
 if(cfg.item[id].kind!=PROGRAM){notice("Select a program to add to favourites.");return;}
 cfg.item[id].pause^=FAVOURITE;list_menu(id);commit();
}
static void move_item(void){
 int choices[ITEMS+1],n=1,i,id,sel=0,start=0,k,page;char b[100];if(!count)return;id=members[selected];if(id==FAV_ID)return;choices[0]=ROOT;
 for(i=0;i<ITEMS;i++)if(cfg.item[i].kind==FOLDER&&!child_of(i,id))choices[n++]=i;
 for(i=0;i<n;i++)if(choices[i]==cfg.item[id].parent)sel=i;
 for(;;){
  page=rows-10;scroll(&sel,&start,n,page,0);desktop("Move entry to a submenu");box(6,3,68,rows-6,"Destination");
  for(i=0;i<page&&start+i<n;i++){
   int f=choices[start+i];if(f==ROOT)strcpy(b,"Home");else sprintf(b,"%s / %s",cfg.item[f].parent==ROOT?"Home":cfg.item[cfg.item[f].parent].name,cfg.item[f].name);
   fill(8,5+i,64,1,' ',sel==start+i?SELECT:PANEL);text(9,5+i,b,61,sel==start+i?SELECT:PANEL);
  }
  button(2,rows-2,"[Save]",13);button(12,rows-2,"[Cancel]",27);button(24,rows-2,"[^]",UP);button(30,rows-2,"[v]",DOWN);button(36,rows-2,"[PgUp]",PGUP);button(45,rows-2,"[PgDn]",PGDN);
  flush();k=key();if(k==27||k==BACK_CLICK)return;
  if(k==CLICK){i=my-5;if(mx>=8&&mx<72&&i>=0&&i<page&&start+i<n)sel=start+i;}
  if(k==13){move_entry(id,choices[sel]);list_menu(id);commit();return;}
  scroll(&sel,&start,n,page,k);
 }
}
#include "SETTINGS.H"
static int launch(int shell){
 /* Fixed wire layout shared with the tiny assembly supervisor. */
 struct {char magic[4];unsigned char pause,reserved;char program[80],cwd[80],args[128];} job;
 Item *e=count&&members[selected]!=FAV_ID?&cfg.item[members[selected]]:NULL;
 struct find_t f;char batch[100],control[100],wd[PATHLEN],*command;FILE *out;int ok,isbat=0;
 memset(&job,0,sizeof(job));memcpy(job.magic,"NM14",4);
 if(!shell){
  if(!e||e->kind!=PROGRAM)return 0;
  isbat=!stricmp(strrchr(e->program,'.'),".BAT");
  if(strlen(e->program)+strlen(e->args)+(e->args[0]?1:0)+(isbat?5:0)>125){message("Command line too long","Shorten the program path or arguments.");return 0;}
  if(_dos_findfirst(e->program,_A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR|_A_ARCH,&f)||(f.attrib&_A_SUBDIR)){message("Program not found","Edit this entry with F4, or browse to the correct file with F2.");return 0;}
  if(!getcwd(wd,sizeof(wd))){message("Directory error","Cannot read the working directory.");return 0;}
  ok=!_chdrive(toupper(e->cwd[0])-'A'+1)&&!chdir(e->cwd);_chdrive(toupper(wd[0])-'A'+1);chdir(wd);
  if(!ok){message("Working folder unavailable","Check the working directory and verify that its disk is inserted.");return 0;}
  strcpy(job.cwd,e->cwd);job.pause=e->pause&PAUSE_EXIT;
 }else strcpy(job.cwd,home);
 sprintf(batch,"%sNM-RUN.BAT",home);sprintf(control,"%sNM-RUN.DAT",home);
 if(shell||isbat){
  command=getenv("COMSPEC");if(!command||strlen(command)>=sizeof(job.program)||!absolute_path(command)){message("COMSPEC error","COMSPEC must contain a full DOS path to COMMAND.COM.");return 0;}
  strcpy(job.program,command);
  if(isbat){
   out=fopen(batch,"wt");if(!out){message("Launch failed","Cannot create NM-RUN.BAT in the menu folder.");return 0;}
   fprintf(out,"@echo off\n%s%s%s\n",e->program,e->args[0]?" ":"",e->args);
   ok=!ferror(out);if(fclose(out))ok=0;if(!ok){remove(batch);message("Launch failed","Could not write the batch job. Check disk space.");return 0;}
   sprintf(job.args,"/C %s",batch);
  }
 }else{strcpy(job.program,e->program);strcpy(job.args,e->args);}
 if(!commit()){remove(batch);return 0;}
 out=fopen(control,"wb");if(!out){remove(batch);message("Launch failed","Cannot create NM-RUN.DAT in the menu folder.");return 0;}
 ok=fwrite(&job,1,sizeof(job),out)==sizeof(job);if(fclose(out))ok=0;
 if(!ok){remove(control);remove(batch);message("Launch failed","Could not write the launch job. Check disk space.");return 0;}return 1;
}
static int help(void){
 int k;
 desktop("Help");box(3,3,74,19,"NuXT Menu 1.4");
 text(5,5,"Arrows/Home/End/PgUp/PgDn: select   Enter: open / launch",70,PANEL);
 text(5,6,"Esc/Backspace/Left: back   F10: exit (Esc stays at Home)",70,PANEL);
 text(5,7,"Mouse: click folders, double-click programs; right-click back",70,PANEL);
 text(5,8,"Joystick: navigate; button 1 opens, button 2 goes back",70,PANEL);
 text(5,10,"F2 Add program   F3 New submenu   F4 Edit entry",70,ACCENT);
 text(5,11,"F5 Settings   F6 Move entry   F7/F8 Earlier/later",70,PANEL);
 text(5,12,"Delete: remove entry only (keeps program files)",70,PANEL);
 text(5,13,"* Favourite   Ctrl-F Favourites   Ctrl-D Duplicate",70,PANEL);
 text(5,14,"Ctrl-S Save   Letters: jump to an entry by name",70,PANEL);
 text(5,16,"F9 DOS prompt (EXIT returns)   F10 Save and exit",70,ACCENT);
 text(5,17,"Changes save automatically. Keep NUXMENU.DAT and .BAK.",70,DIM);
 text(5,18,"MENU /SAFE restores readable colours and disables joystick.",70,DIM);
 button(5,20,"[Add]",F2);button(12,20,"[Submenu]",F3);button(23,20,"[Edit]",F4);
 button(31,20,"[Move]",F6);button(39,20,"[Remove]",DEL);button(49,20,"[*]",'*');button(54,20,"[Duplicate]",DUPLICATE);
 button(2,rows-2,"[Back]",27);button(13,rows-2,"[Save]",SAVE);flush();k=key();
 if(k==F2||k==F3||k==F4||k==F5||k==F6||k==F7||k==F8||k==F9||k==F10||k==DEL||k=='*'||k==DUPLICATE||k==SAVE||k==FAV_VIEW)return k;return 0;
}
int main(int argc,char **argv){
 union REGS r;char exe[260],*p;int i,k,rc=0,driver=0,safe=0,returning=0,override=-1,loaded,previous=-1;clock_t last=0;
 for(i=1;i<argc;i++){
  if(!stricmp(argv[i],"/DRIVER"))driver=1;else if(!stricmp(argv[i],"/RETURN"))returning=1;else if(!stricmp(argv[i],"/SAFE"))safe=1;
  else if(!stricmp(argv[i],"/25"))override=0;else if(!stricmp(argv[i],"/43"))override=1;else if(!stricmp(argv[i],"/50"))override=2;
  else{puts("NuXT Menu: MENU [/25|/43|/50] [/SAFE]\nKeep MENU.EXE and NUI.EXE together. VGA enables 43/50 rows.");return 0;}
 }
 if(!driver){puts("Please run MENU.EXE; it releases the interface memory before launching games.");return 1;}
 if(!_cmdname(exe)||!(p=strrchr(exe,'\\')))return 1;p[1]=0;
 if(strlen(exe)>68||!absolute_path(exe)){puts("Install in a short DOS folder, e.g. C:\\NUXMENU.");return 1;}strcpy(home,exe);
 _harderr(hard);loaded=load_config(home);
 if(loaded<0){puts("NUXMENU.DAT and its backup are unreadable. Restore a backup, or rename\nNUXMENU.DAT to start a new menu. Existing files were not changed.");return 1;}
 if(!loaded)defaults(home);if(loaded==2)notice("Recovered configuration from NUXMENU.BAK.");
 if(safe){cfg.opt.joy=0;cfg.opt.saver=0;cfg.mode=0;cfg.theme=0;memcpy(cfg.colors,themes[0],8);}if(override>=0)cfg.mode=(unsigned char)override;
 current=(returning||cfg.opt.start_last)?cfg.current:ROOT;
 favourites=(returning||cfg.opt.start_last)?cfg.opt.lastfav:0;
 top=cfg.opt.scroll[place_index()];list_menu(cfg.opt.selection[place_index()]);
 activity_tick=ui_tick=now_ticks();joy_settle=1;
 r.h.ah=15;int86(0x10,&r,&r);mono=(r.h.al&127)==7;
 r.x.ax=0x1a00;int86(0x10,&r,&r);vga=r.h.al==0x1a;
 if(vga){original_rows=*(unsigned char __far *)MK_FP(0x40,0x84)+1;if(original_rows!=43&&original_rows!=50)original_rows=25;}
 {unsigned char __far *vector=(unsigned char __far *)_dos_getvect(0x33);
  if(vector&&*vector!=0xcf){r.x.ax=0;int86(0x33,&r,&r);mouse_ok=r.x.ax==0xffff;}
 }shown=_fmalloc(80U*50U*2U);mode(cfg.mode);
 for(;;){
  draw_menu();allow_idle=joy_context=1;k=key();allow_idle=joy_context=0;if(k==F1)k=help();if(k!=CLICK)previous=-1;
  if(k==CLICK){
   i=my-5;if(mx>=4&&mx<38&&i>=0&&i<rows-10&&top+i<count){selected=top+i;if(members[selected]==FAV_ID||cfg.item[members[selected]].kind==FOLDER){k=13;guard_click();}else if(previous==selected&&clock()-last<CLOCKS_PER_SEC)k=13;previous=selected;last=clock();}
   else previous=-1;
  }
  if(k==F10){
   if(cfg.opt.confirm_exit&&!yesno("Exit to DOS?","Leave NuXT Menu?"))continue;
   if(commit()||yesno("Exit without saving?","Your latest edits could not be saved. Discard those unsaved edits and exit?"))break;
  }else if(k==27||k==8||k==LEFT||k==BACK_CLICK){if(favourites)toggle_favourites();else if(current!=ROOT){i=cfg.item[current].parent;enter_menu(i,0);}}
  else if((k==13||k==RIGHT)&&count){i=members[selected];if(i==FAV_ID){enter_menu(ROOT,1);previous=-1;}else if(cfg.item[i].kind==FOLDER){enter_menu(i,0);previous=-1;}else if(k==13&&launch(0)){rc=10;break;}}
  else if(k==FAV_VIEW)toggle_favourites();else if(k=='*')star_item();else if(k==DUPLICATE)duplicate_item();else if(k==SAVE)commit();
  else if(k==F2)add_program();else if(k==F3){if(favourites)toggle_favourites();add_folder();}else if(k==F4)edit_item();
  else if(k==F5)settings();else if(k==F6)move_item();else if(k==DEL)delete_item();else if(k==F9&&launch(1)){rc=10;break;}
  else if((k==F7||k==F8)&&favourites)notice("Reorder programs in their submenu; favourites sort by name.");
  else if((k==F7||k==F8)&&count&&members[selected]!=FAV_ID){i=selected+(k==F7?-1:1);if(i>=0&&i<count&&members[i]!=FAV_ID){unsigned short rank=cfg.item[members[i]].rank;cfg.item[members[i]].rank=cfg.item[members[selected]].rank;cfg.item[members[selected]].rank=rank;i=members[selected];list_menu(i);commit();}}
  else if(k>=32&&k<=126&&count){for(i=1;i<=count;i++){int n=(selected+i)%count;if(toupper(members[n]==FAV_ID?'F':cfg.item[members[n]].name[0])==toupper(k)){selected=n;break;}}}
  else scroll(&selected,&top,count,rows-10,k);
 }
 mouseshow(0);mode(original_rows==43?1:original_rows==50?2:0);if(vga){r.x.ax=0x1003;r.x.bx=1;int86(0x10,&r,&r);}cursor(1,0,0);if(shown)_ffree(shown);return rc;
}
