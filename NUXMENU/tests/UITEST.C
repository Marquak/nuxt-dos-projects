/* Runs only in TEST.SH's private DOS folder. Includes real UI routines. */
#define main menu_main
#include "UI.C"
#undef main
static FILE *report;
static int checks,failures;
static void check(int ok,const char *s){checks++;if(!ok){fprintf(report,"FAIL: %s\n",s);failures++;}}
static void queue(unsigned k){union REGS r;r.h.ah=5;r.x.cx=k;int86(0x16,&r,&r);}
static void program(int id,int parent,const char *name){
 Item *e=&cfg.item[id];memset(e,0,sizeof(*e));e->kind=PROGRAM;e->parent=(unsigned char)parent;
 strcpy(e->name,name);strcpy(e->program,"C:\\GO.BAT");strcpy(e->cwd,"C:\\");strcpy(e->args,"/A");append_rank(id,parent);
}
static void reset(void){defaults("C:\\");current=ROOT;favourites=0;top=selected=0;list_menu(FAV_ID);}
int main(void){
 int i,j,layout;unsigned writes;char result[PATHLEN],saved[PATHLEN];FILE *f;
 report=fopen("UIRESULT.TXT","w");if(!report)return 2;setvbuf(report,NULL,_IONBF,0);
 strcpy(home,"C:\\");shown=_fmalloc(8000);check(shown!=NULL,"screen shadow allocation");vga=1;mode(0);reset();
 cfg.item[0].rank=65535;cfg.item[1].rank=100;cfg.item[2].parent=0;
 check(move_entry(2,ROOT),"move to root");list_menu(2);
 check(members[0]==FAV_ID&&members[1]==1&&members[2]==0&&members[3]==2,"destination order preserved and appended");
 check(cfg.item[2].rank==2,"maximum rank normalized without overflow");
 check(move_entry(1,ROOT)&&cfg.item[1].rank==0,"same parent leaves order unchanged");
 check(!move_entry(0,0),"reject self move");cfg.item[2].parent=0;
 check(!move_entry(0,2),"reject descendant move");
 reset();mkdir("C:\\BROWSE");mkdir("C:\\BROWSE\\A&B");strcpy(cfg.browse,"C:\\BROWSE");strcpy(saved,cfg.browse);
 queue(13);queue(27);check(!browser(result),"cancel after entering shell-unsafe folder");
 check(!strcmp(saved,cfg.browse)&&save_config(home),"cancel preserves valid browse path and save");
 reset();program(3,0,"Alpha");program(4,1,"Zulu");cfg.item[3].pause=PAUSE_EXIT|FAVOURITE;cfg.item[4].pause=FAVOURITE;
 current=0;list_menu(3);toggle_favourites();check(count==2&&members[0]==3&&members[1]==4,"favourites spans submenus and sorts names");
 toggle_favourites();check(current==ROOT&&members[0]==FAV_ID,"favourites back returns Home");enter_menu(0,0);check(members[selected]==3,"submenu selection restored");
 queue(13);duplicate_item();check(count==2&&members[selected]==5,"duplicate appends independent entry");
 check(!strcmp(cfg.item[5].program,cfg.item[3].program)&&!strcmp(cfg.item[5].args,"/A")&&cfg.item[5].pause==3,"duplicate retains launch fields and flags");
 strcpy(cfg.item[5].args,"/B");check(!strcmp(cfg.item[3].args,"/A"),"duplicate edits independent");
 queue(27);duplicate_item();check(cfg.item[6].kind==0,"cancel duplicate creates nothing");
 star_item();check(cfg.item[5].pause==PAUSE_EXIT,"unstar preserves pause");
 f=fopen("GO.BAT","w");fputs("@echo off\n",f);fclose(f);
 cfg.item[5].pause=FAVOURITE;check(launch(0),"favourite launch job created");
 f=fopen("NM-RUN.BAT","r");i=0;while(fgets(result,sizeof(result),f))if(strstr(result,"pause"))i=1;fclose(f);
 f=fopen("NM-RUN.DAT","rb");fseek(f,4,SEEK_SET);i=fgetc(f);fclose(f);check(!i,"favourite flag does not enable pause");
 cfg.item[5].pause|=PAUSE_EXIT;check(launch(0),"pause launch job created");
 f=fopen("NM-RUN.BAT","r");i=0;while(fgets(result,sizeof(result),f))if(strstr(result,"pause"))i=1;fclose(f);f=fopen("NM-RUN.DAT","rb");fseek(f,4,SEEK_SET);i=fgetc(f);fclose(f);check(i,"pause flag reaches supervisor");
 for(layout=0;layout<3;layout++){
  mode(layout);reset();draw_menu();check(last_writes==(unsigned)rows*80,"mode change fully initializes display");
  selected=1;draw_menu();writes=last_writes;
  check(writes>0&&writes<200,"selection writes only changed cells");
  check((frame[5*80+4]>>8)==cfg.colors[PANEL]&&(frame[6*80+4]>>8)==cfg.colors[SELECT],"old selection erased and new highlighted");
  check(!_fmemcmp(frame,shown,rows*160)&&!_fmemcmp(frame,MK_FP(0xb800,0),rows*160),"shadow and VGA match final frame");
  fprintf(report,"%d rows: selection writes %u/%u cells\n",rows,writes,rows*80);
  draw_menu();check(last_writes==0,"unchanged screen writes nothing");
  check(hit_button(2,rows-3)==BACK_CLICK&&hit_button(9,rows-3)==UP&&hit_button(13,rows-3)==DOWN,"back and scroll mouse targets");
  check(hit_button(17,rows-1)==F5&&hit_button(36,rows-1)==F9&&hit_button(58,rows-1)==F10,"uncluttered settings prompt exit footer");
  i=1;for(j=0;j<button_count;j++){int k;if(buttons[j].x+buttons[j].w>80)i=0;for(k=j+1;k<button_count;k++)if(buttons[j].y==buttons[k].y&&buttons[j].x<buttons[k].x+buttons[k].w&&buttons[k].x<buttons[j].x+buttons[j].w)i=0;}
  check(i,"toolbar buttons fit without overlap");
 }
 strcpy(result,"Keep");queue(27);check(!input("Test","Name:",result,sizeof(result))&&!strcmp(result,"Keep"),"cancel preserves input text");
 check(hit_button(10,16)==13&&hit_button(23,16)==27,"input save cancel mouse targets");
 reset();i=cfg.theme;queue(DOWN);queue(RIGHT);queue(27);settings();check(cfg.theme==i,"appearance cancel restores original theme");
 queue(DOWN);queue(RIGHT);queue(SAVE);settings();check(cfg.theme==i+1,"appearance save accepts theme");
 reset();check(count==4&&members[0]==FAV_ID,"Home always has virtual favourites without using a slot");
 star_item();check(cfg.item[0].pause==0,"virtual submenu cannot be starred");
 edit_item();delete_item();duplicate_item();move_item();check(count==4,"virtual submenu protected from mutation");
 reset();for(i=3;i<40;i++)program(i,0,"Many");enter_menu(0,0);selected=25;top=20;remember_place();enter_menu(ROOT,0);enter_menu(0,0);
 check(selected==25&&top==20,"submenu selection and scroll restored");
 {char s[12];time_string(s,0,7,0);check(!strcmp(s,"12:07 AM"),"12h midnight");time_string(s,12,0,0);check(!strcmp(s,"12:00 PM"),"12h noon");time_string(s,23,9,1);check(!strcmp(s,"23:09"),"24h format");}
 check(since(5,0x1800adUL)==8,"idle timer handles midnight rollover");
 reset();draw_menu();clock_draw(1);flush();clock_draw(0);flush();check(last_writes==0,"unchanged clock causes no video writes");
 for(i=1;i<=4;i++){unsigned short __far *copy=_fmalloc(rows*160U);_fmemcpy(copy,frame,rows*160U);queue(13);screensaver(i);check(!_fmemcmp(copy,frame,rows*160U),"saver wake restores screen");check(!_bios_keybrd(_KEYBRD_READY),"saver consumes wake key");_ffree(copy);}
 {unsigned r[3]={100,100,0};cfg.opt.xmin=cfg.opt.ymin=70;cfg.opt.xmax=cfg.opt.ymax=130;joy_settle=1;joy_old=0;
 check(!joy_action(r,10)&&!joy_settle,"neutral joystick settles");r[1]=50;check(joy_action(r,11)==UP,"joystick up");check(!joy_action(r,12),"joystick repeat delay");check(joy_action(r,16)==UP,"joystick repeat fires");r[1]=100;r[2]=1;check(joy_action(r,17)==13,"button one opens");check(!joy_action(r,18),"held button does not reopen");r[2]=2;check(joy_action(r,19)==BACK_CLICK,"button two goes back");joy_settle=1;check(!joy_action(r,20),"wake held button suppressed");}
 reset();queue(DOWN);queue(DOWN);queue(RIGHT);queue(DOWN);queue(RIGHT);queue(27);settings();check(cfg.opt.clock_on&&!cfg.opt.clock24,"settings cancel restores clock options");
 _ffree(shown);shown=NULL;mode(0);fprintf(report,"UI: %d checks, %d failures\n",checks,failures);fclose(report);return failures!=0;
}
