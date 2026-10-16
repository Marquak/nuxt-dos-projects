/* Runs only in TEST.SH's private DOS folder. Includes real UI routines. */
#define main menu_main
#include "UI.C"
#undef main
static unsigned char __far *font_ptr(void);
#pragma aux font_ptr = "push bp" "mov ax,1130h" "mov bh,6" "int 10h" "mov ax,bp" "mov dx,es" "pop bp" value [dx ax] modify [ax bx cx dx es];
static int fake_x,fake_y,fake_held,fake_wheel,fake_release,fake_rx,fake_ry,fake_api;
static void __interrupt __far fake_mouse(union INTPACK r){
 if(r.w.ax==0){r.w.ax=0xffff;r.w.bx=3;}
 else if(r.w.ax==0x11){r.w.ax=fake_api?0x574d:0;r.w.cx=fake_api?1:0;}
 else if(r.w.ax==3){r.w.bx=(unsigned)fake_held|((unsigned)(unsigned char)fake_wheel<<8);fake_wheel=0;r.w.cx=fake_x;r.w.dx=fake_y;}
 else if(r.w.ax==6){r.w.cx=fake_rx;r.w.dx=fake_ry;if(r.w.bx==0){r.w.bx=fake_release;fake_release=0;}else r.w.bx=0;}
}
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

 /* New interactions use actual modal routines, not a second implementation. */
 reset();queue(13);check(!yesno("Remove?","Test"),"confirmation defaults to Cancel");
 queue(9);queue(13);check(yesno("Remove?","Test"),"Tab highlights and activates Yes");
 queue(SHIFT_TAB);queue(13);check(yesno("Remove?","Test"),"Shift-Tab highlights and activates Yes");
 strcpy(result,"Keep");queue(9);queue(9);queue(13);check(!input("Test","Name",result,sizeof(result)),"input Tab reaches Cancel");
 strcpy(result,"Clear");queue(SHIFT_TAB);queue(13);queue(13);check(input("Test","Name",result,sizeof(result))&&!result[0],"input Shift-Tab Clear then Save");
 reset();mkdir("C:\\PICK");mkdir("C:\\OTHER");
 f=fopen("C:\\PICK\\NEW.EXE","wb");fputs("test",f);fclose(f);
 queue(F2);check(browse_path(result,"C:\\OTHER",1)&&!strcmp(result,"C:\\OTHER"),"select empty working folder");
 queue(13);check(browse_path(result,"C:\\PICK",0)&&!strcmp(result,"C:\\PICK\\NEW.EXE"),"program browser starts at entry folder");
 program(3,0,"Original");
 {Item e=cfg.item[3],before=cfg.item[3];
  strcpy(e.program,"C:\\PICK\\OLD.EXE");strcpy(e.cwd,"C:\\OTHER");
  queue(21);queue('N');queue(9);queue(F2);queue(13);queue('y');queue(SAVE);
  check(edit_program(&e)&&!strcmp(e.name,"N")&&!strcmp(e.program,"C:\\PICK\\NEW.EXE")&&!strcmp(e.cwd,"C:\\PICK"),"browse preserves edited name and updates working folder on request");
  check(!memcmp(&cfg.item[3],&before,sizeof(e)),"editor does not commit record before caller saves");
  e=before;queue('x');queue(27);queue(13);queue(SAVE);
  check(edit_program(&e)&&!strcmp(e.name,"Originalx"),"discard defaults to Cancel and retains edits");
  e=before;queue('x');queue(27);queue('y');check(!edit_program(&e)&&!memcmp(&cfg.item[3],&before,sizeof(e)),"confirmed discard leaves stored entry unchanged");
  e=before;queue(9);queue(F2);queue(27);queue(SAVE);check(edit_program(&e)&&!memcmp(&e,&before,sizeof(e)),"cancel nested browser preserves all entry fields");
  e=before;strcpy(e.cwd,"C:\\OTHER");queue(9);queue(9);queue(9);queue(9);queue(F2);queue(F2);queue(SAVE);
  check(edit_program(&e)&&!strcmp(e.cwd,"C:\\OTHER"),"working folder Browse selects directory");
 }
 for(layout=0;layout<3;layout++){
  int sel=0,start=0,page;
  mode(layout);reset();page=rows-10;bar.capture=0;scrollbar(38,5,page,100,page,0);
  check(bar_pointer(1,1,38,bar.thumb)==BAR_MOVE&&bar.capture,"thumb press captures scrollbar");
  check(bar_pointer(1,0,38,100)==BAR_MOVE&&bar.target==100-page,"thumb drag clamps to bottom");
  pointer_scroll(BAR_MOVE,&sel,&start,100,page,4,5,35);
  check(start==100-page&&sel>=start&&sel<start+page,"drag keeps selection in viewport");
  bar.capture=0;scrollbar(38,5,page,100,page,0);
  check(bar_pointer(1,1,38,5+page-1)==BAR_MOVE&&bar.target==1,"scrollbar down arrow moves one row");
  bar.capture=0;scrollbar(38,5,page,100,page,0);
  check(bar_pointer(1,1,38,bar.thumb+bar.size)==BAR_MOVE&&bar.target==page,"scrollbar track pages down");
  bar.capture=0;start=sel=0;mx=10;my=6;wheel_delta=2;
  check(pointer_scroll(WHEEL,&sel,&start,100,page,4,5,35)&&start==6,"wheel scrolls three rows per detent");
  mx=50;check(!pointer_scroll(WHEEL,&sel,&start,100,page,4,5,35)&&start==6,"wheel outside list does nothing");
  mx=10;wheel_delta=-20;pointer_scroll(WHEEL,&sel,&start,100,page,4,5,35);check(!start,"wheel clamps at top");
  scrollbar(38,5,page,0,page,0);check(!bar_pointer(1,1,38,6),"empty scrollbar does not capture");
  for(i=3;i<80;i++)program(i,0,"Many");enter_menu(0,0);selected=70;draw_menu();check(selected>=top&&selected<top+page,"selected entry visible after navigation");
  check(bar.thumb+bar.size<=bar.y+bar.h-1,"scroll thumb stays inside track");
  draw_menu();check(!last_writes,"unchanged scrollbar causes no writes");
  check((frame[1*80+74]&255)!=' '&&(frame[0*80+74]&255)==' ',"clock is centred on three-row header");
 }
 reset();program(3,ROOT,"Zulu");program(4,ROOT,"alpha");program(5,ROOT,"Beta");list_menu(3);
 sort_entries(0,1);check(members[1]==1&&members[2]==0&&members[3]==2&&members[4]==4&&members[5]==5&&members[6]==3,"sort ascending with folders first");
 check(members[selected]==3&&save_config(home)&&load_config(home)==1,"sorted menu saves without losing selected entry");
 list_menu(3);sort_entries(1,0);check(members[1]==3&&members[count-1]==4,"descending mixed sort");
 queue(27);sort_menu();check(members[1]==3,"cancel sort leaves order unchanged");
 /* Exercise real INT 33 polling with an isolated simulated DOS driver. */
 {void (__interrupt __far *oldmouse)(void)=_dos_getvect(0x33);
  _dos_setvect(0x33,fake_mouse);mouse_ok=1;fake_api=1;mode(0);reset();wake_mouse=0;
  check(wheel_ok,"wheel capability handshake recognized");bar.enabled=0;fake_x=80;fake_y=64;fake_wheel=-2;
  check(poll_input(0)==WHEEL&&wheel_delta==-2&&mx==10&&my==8,"signed wheel delta and pointer decoded via INT33");
  check(!poll_input(0),"wheel event consumed once");
  fake_api=0;mode(0);fake_wheel=3;check(!wheel_ok&&!poll_input(0),"old drivers ignore BH without wheel support");
  fake_api=1;mode(0);scrollbar(38,5,15,100,15,0);fake_x=38*8;fake_y=bar.thumb*8;fake_held=1;
  check(poll_input(0)==BAR_MOVE&&bar.capture,"poll captures held thumb");
  fake_y=18*8;check(poll_input(0)==BAR_MOVE&&bar.target>0,"poll tracks thumb drag");
  fake_held=0;fake_release=1;fake_rx=38*8;fake_ry=19*8;fake_y=19*8;
  i=poll_input(0);check((i==0||i==BAR_MOVE)&&!bar.capture&&!fake_release,"release drains drag click without activating entry");
  check(!poll_input(0),"no leftover click after dragging");
  scrollbar(38,5,15,100,15,0);fake_release=1;fake_rx=38*8;fake_ry=19*8;
  check(poll_input(0)==BAR_MOVE&&bar.target==1&&!bar.capture,"quick press-release scroll arrow works between polls");
  fake_release=1;fake_rx=10*8;fake_ry=8*8;
  check(poll_input(0)==CLICK,"ordinary list click still reaches UI");
  _dos_setvect(0x33,oldmouse);mouse_ok=mouse_visible=wheel_ok=mouse_buttons=0;bar.capture=0;
 }
 /* Frames for a visual check of the new header, scrollbars and focused form. */
 mode(0);reset();for(i=3;i<24;i++)program(i,0,"Example program");enter_menu(0,0);draw_menu();
 f=fopen("MENU.SCR","wb");fwrite(frame,2,2000,f);fclose(f);
 {Item e=cfg.item[3];queue(9);queue(9);queue(9);queue(9);queue(9);queue(9);queue(9);queue(27);edit_program(&e);}
 f=fopen("EDIT.SCR","wb");fwrite(frame,2,2000,f);fclose(f);
 {unsigned char __far *font=font_ptr();f=fopen("FONT.BIN","wb");for(i=0;i<4096;i++)fputc(font[i],f);fclose(f);}
 _ffree(shown);shown=NULL;mode(0);fprintf(report,"UI: %d checks, %d failures\n",checks,failures);fclose(report);return failures!=0;
}
