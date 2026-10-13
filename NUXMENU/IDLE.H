/* Menu-owned clock, bounded gameport reads and text-mode idle animation.
   No hooks or TSR: everything stops before a launched program starts. */
static int joy_settle=1,joy_old,joy_dir,joy_bad,mouse_lastx=-1,mouse_lasty=-1;
static unsigned long joy_tick,joy_repeat;
static unsigned long now_ticks(void){
 volatile unsigned long __far *p=MK_FP(0x40,0x6c);unsigned long a,b;
 do{a=*p;b=*p;}while(a!=b);return a;
}
static unsigned long since(unsigned long a,unsigned long b){return a>=b?a-b:0x1800b0UL-b+a;}
static unsigned pit_count(void){unsigned lo;outp(0x43,0);lo=inp(0x40);return lo|((unsigned)inp(0x40)<<8);}
static int joy_read(unsigned *r){
 unsigned start,bits,pending=3,elapsed,limit=4096;
 r[0]=r[1]=0xffff;r[2]=0;start=pit_count();outp(0x201,255);
 do{
  bits=inp(0x201);elapsed=start-pit_count();
  if(elapsed>=2386)return 0;
  if((pending&1)&&!(bits&1)){r[0]=elapsed;pending&=~1;}
  if((pending&2)&&!(bits&2)){r[1]=elapsed;pending&=~2;}
  if(!pending){r[2]=(~bits>>4)&3;return 1;}
 }while(--limit);return 0;
}
static int joy_action(unsigned *r,unsigned long t){
 int dir=0,edge=(int)r[2]&~joy_old;
 joy_old=(int)r[2];
 if(r[1]<cfg.opt.ymin)dir=UP;else if(r[1]>cfg.opt.ymax)dir=DOWN;
 else if(r[0]<cfg.opt.xmin)dir=LEFT;else if(r[0]>cfg.opt.xmax)dir=RIGHT;
 if(joy_settle){if(!dir&&!r[2])joy_settle=0;joy_dir=0;return 0;}
 if(edge&2)return BACK_CLICK;if(edge&1)return 13;
 if(!dir){joy_dir=0;return 0;}
 if(dir!=joy_dir){joy_dir=dir;joy_repeat=t;return dir;}
 if(since(t,joy_repeat)>=(unsigned)(cfg.opt.repeat==0?9:cfg.opt.repeat==1?5:3)){joy_repeat=t;return dir;}
 return 0;
}
static void time_string(char *s,unsigned hour,unsigned minute,int use24){
 if(use24)sprintf(s,"%02u:%02u",hour,minute);
 else sprintf(s,"%2u:%02u %s",hour%12?hour%12:12,minute,hour<12?"AM":"PM");
}
static int clock_draw(int force){
 struct dostime_t t;
 if(!cfg.opt.clock_on){if(force)fill(68,0,11,1,' ',TITLE);clock_hour=-1;return force;}
 _dos_gettime(&t);
 if(force||clock_hour!=t.hour||clock_minute!=t.minute||clock_format!=cfg.opt.clock24){
  char s[12];time_string(s,t.hour,t.minute,cfg.opt.clock24);
  fill(68,0,11,1,' ',TITLE);text(79-(int)strlen(s),0,s,10,TITLE);
  clock_hour=t.hour;clock_minute=t.minute;clock_format=cfg.opt.clock24;return 1;
 }return 0;
}
/* Input for all screens; joystick navigation is confined to the launcher.
   Mouse movement counts as activity without turning it into an action. */
static int poll_input(int joystick){
 union REGS r;unsigned k,v[3];unsigned long t=now_ticks();int moved=0;
 if(_bios_keybrd(_KEYBRD_READY)){activity_tick=t;click_guard=0;k=_bios_keybrd(_KEYBRD_READ);return (k&255)&&(k&255)!=224?(int)(k&255):(int)(k&0xff00);}
 if(mouse_ok){
  r.x.ax=3;int86(0x33,&r,&r);
  if((int)r.x.cx!=mouse_lastx||(int)r.x.dx!=mouse_lasty||r.x.bx){activity_tick=t;moved=1;}
  mouse_lastx=r.x.cx;mouse_lasty=r.x.dx;
  if(wake_mouse){
   unsigned held=r.x.bx;
   r.x.ax=6;r.x.bx=0;int86(0x33,&r,&r);r.x.ax=6;r.x.bx=1;int86(0x33,&r,&r);
   if(!held)wake_mouse=0;
  }else{
   r.x.ax=6;r.x.bx=1;int86(0x33,&r,&r);if(r.x.bx){activity_tick=t;click_guard=0;return BACK_CLICK;}
   r.x.ax=6;r.x.bx=0;int86(0x33,&r,&r);
   if(r.x.bx){
    activity_tick=t;mx=r.x.cx/8;my=r.x.dx/8;
    if(click_guard&&mx==guard_x&&my==guard_y&&clock()-guard_time<CLOCKS_PER_SEC/2)return 0;
    click_guard=0;k=(unsigned)hit_button(mx,my);
    if(k!=CLICK&&k!=UP&&k!=DOWN&&k!=PGUP&&k!=PGDN&&k!=LEFT&&k!=RIGHT)guard_click();return k;
   }
  }
 }
 if(joystick&&cfg.opt.joy&&since(t,joy_tick)>=(joy_bad>=8?18UL:1UL)){
  joy_tick=t;
  if(joy_read(v)){
   int a;joy_bad=0;a=joy_action(v,t);
   if(a||v[2]||joy_dir){activity_tick=t;moved=1;}
   if(a)return a;
  }else{if(joy_bad<8)joy_bad++;joy_settle=1;joy_dir=0;}
 }
 return saver_active&&moved?CLICK:0;
}
static unsigned saver_seed=1907;
static unsigned saver_rand(void){saver_seed=(unsigned)(saver_seed*25173U+13849U);return saver_seed;}
static void saver_cell(int x,int y,unsigned ch,unsigned colour){if(x>=0&&x<80&&y>=0&&y<rows)frame[y*80+x]=(unsigned short)(ch|(colour<<8));}
static void saver_text(int x,int y,const char *s){while(*s)saver_cell(x++,y,(unsigned char)*s++,11);}
/* Erase only the previous animation cells; full initialization happens once. */
static void saver_shape(int x,int y,int radius,unsigned colour){
 int i;for(i=-radius;i<=radius;i++){
  saver_cell(x+i,y-radius,colour?196:' ',colour);saver_cell(x+i,y+radius,colour?196:' ',colour);
  saver_cell(x-radius,y+i,colour?179:' ',colour);saver_cell(x+radius,y+i,colour?179:' ',colour);
 }
 saver_cell(x-radius,y-radius,colour?218:' ',colour);saver_cell(x+radius,y-radius,colour?191:' ',colour);
 saver_cell(x-radius,y+radius,colour?192:' ',colour);saver_cell(x+radius,y+radius,colour?217:' ',colour);
}
static void screensaver(int kind){
 unsigned short __far *saved;unsigned long t,last;int x=10,y=5,dx=1,dy=1,i,k;
 unsigned char sx[32],sy[32],sc[32];char s[12];struct dostime_t dt;
 saved=_fmalloc(rows*160U);if(!saved)return;_fmemcpy(saved,frame,rows*160U);
 saver_active=1;mouseshow(0);fill_words(frame,0x0720,(unsigned)rows*80);flush();
 for(i=0;i<32;i++){sx[i]=(unsigned char)(saver_rand()%80);sy[i]=(unsigned char)(saver_rand()%rows);sc[i]=(unsigned char)(1+saver_rand()%3);}
 last=now_ticks();activity_tick=last;
 for(;;){
  t=now_ticks();k=poll_input(1);if(k||activity_tick!=last)break;
  if(since(t,last)>=4){
   if(kind==2){
    for(i=0;i<10;i++)saver_cell(x+i,y,' ',7);
    x+=dx;y+=dy;if(x<=0||x>=70)dx=-dx;if(y<=0||y>=rows-1)dy=-dy;
    _dos_gettime(&dt);time_string(s,dt.hour,dt.minute,cfg.opt.clock24);saver_text(x,y,s);
   }else if(kind==3){
    for(i=0;i<32;i++)saver_cell(sx[i],sy[i],' ',7);
    for(i=0;i<32;i++){if(sx[i]<sc[i]){sx[i]=79;sy[i]=(unsigned char)(saver_rand()%rows);}else sx[i]-=sc[i];saver_cell(sx[i],sy[i],sc[i]==3?'*':'.',sc[i]==3?15:sc[i]==2?7:8);}
   }else if(kind==4){
    saver_shape(x,y,3,0);saver_shape(79-x,rows-1-y,2,0);
    x+=dx;y+=dy;if(x<=4||x>=75)dx=-dx;if(y<=4||y>=rows-5)dy=-dy;
    saver_shape(x,y,3,11);saver_shape(79-x,rows-1-y,2,13);
   }
   if(kind!=1)flush();last=t;activity_tick=t;
  }
  idle();
 }
 /* Drain the wake keystrokes; held joystick/mouse must settle before reuse. */
 while(_bios_keybrd(_KEYBRD_READY))_bios_keybrd(_KEYBRD_READ);
 wake_mouse=1;joy_settle=1;joy_old=3;joy_dir=0;click_guard=0;
 _fmemcpy(frame,saved,rows*160U);_ffree(saved);saver_active=0;clock_draw(1);flush();mouseshow(1);activity_tick=now_ticks();
}
static int key(void){
 int k;unsigned long t;
 activity_tick=now_ticks();
 for(;;){
  k=poll_input(joy_context);if(k)return k;
  t=now_ticks();
  if(since(t,ui_tick)>=18){ui_tick=t;if(clock_draw(0))flush();}
  if(allow_idle&&cfg.opt.saver&&since(t,activity_tick)>=(unsigned long)cfg.opt.timeout*1093UL){screensaver(cfg.opt.saver);}
  idle();
 }
}
