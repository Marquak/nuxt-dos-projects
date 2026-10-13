static const char *saver_names[5]={"Off","Blank screen","Roaming clock","Star drift","Orbit frames"};
static unsigned median3(unsigned a,unsigned b,unsigned c){unsigned t;if(a>b){t=a;a=b;b=t;}if(b>c){b=c;}return a>b?a:b;}
static void calibrate_joy(void){
 static const char *steps[3]={"Centre the stick","Hold up-left","Hold down-right"};
 unsigned v[3][2],a[3],b[3],c[3];int step=0,released=0,fail=0,k;unsigned long last=now_ticks(),t;
 desktop("Settings");box(5,5,70,13,"Joystick calibration");
 text(8,11,"Press button 1 at each position. Release between steps.",63,PANEL);
 text(8,13,"Esc cancels. Calibration is saved with Settings.",63,DIM);button(8,15,"[Cancel]",27);
 while(step<3){
  fill(8,8,60,1,' ',PANEL);text(8,8,steps[step],60,ACCENT);flush();
  k=poll_input(0);if(k==27||k==BACK_CLICK)return;
  t=now_ticks();if(t==last){idle();continue;}last=t;
  if(!joy_read(a)){if(++fail>=8){message("Joystick unavailable","No response on the first gameport joystick. Connect it and try again.");return;}continue;}fail=0;
  if(!(a[2]&1))released=1;
  if((a[2]&1)&&released&&joy_read(b)&&joy_read(c)){
   v[step][0]=median3(a[0],b[0],c[0]);v[step][1]=median3(a[1],b[1],c[1]);step++;released=0;
  }
 }
 if(v[0][0]<=v[1][0]+12||v[2][0]<=v[0][0]+12||v[0][1]<=v[1][1]+12||v[2][1]<=v[0][1]+12){message("Try calibration again","Both axes must reach the centre and both corners.");return;}
 cfg.opt.xmin=v[0][0]-(v[0][0]-v[1][0])/3;cfg.opt.xmax=v[0][0]+(v[2][0]-v[0][0])/3;
 cfg.opt.ymin=v[0][1]-(v[0][1]-v[1][1])/3;cfg.opt.ymax=v[0][1]+(v[2][1]-v[0][1])/3;
 cfg.opt.joy=1;joy_settle=1;joy_bad=0;joy_old=3;joy_dir=0;
}
static void settings(void){
 unsigned char oldmode=cfg.mode,oldtheme=cfg.theme,oldcolors[8];Options oldopt=cfg.opt;
 int sel=0,start=0,k,i,n,change,page;char b[90];memcpy(oldcolors,cfg.colors,8);
 for(;;){
  page=rows-11;if(page>20)page=20;scroll(&sel,&start,20,page,0);
  desktop("Settings");box(4,3,72,rows-8,"Settings");
  for(i=0;i<page&&start+i<20;i++){
   n=start+i;
   switch(n){
   case 0:sprintf(b,"Screen layout       80 x %d",cfg.mode==1?43:cfg.mode==2?50:25);break;
   case 1:sprintf(b,"Theme               %s",theme_names[cfg.theme]);break;
   case 2:sprintf(b,"Clock               %s",cfg.opt.clock_on?"On":"Off");break;
   case 3:sprintf(b,"Time format         %s",cfg.opt.clock24?"24 hour":"12 hour");break;
   case 4:sprintf(b,"Screensaver         %s",saver_names[cfg.opt.saver]);break;
   case 5:sprintf(b,"Idle timeout        %u minute%s",cfg.opt.timeout,cfg.opt.timeout==1?"":"s");break;
   case 6:strcpy(b,"Preview screensaver  Enter");break;
   case 7:sprintf(b,"Joystick            %s",cfg.opt.joy?"On":"Off");break;
   case 8:strcpy(b,"Calibrate joystick   Enter");break;
   case 9:sprintf(b,"Joystick repeat     %s",cfg.opt.repeat==0?"Slow":cfg.opt.repeat==1?"Normal":"Fast");break;
   case 10:sprintf(b,"Start in            %s",cfg.opt.start_last?"Last-used menu":"Home");break;
   case 11:sprintf(b,"Confirm exit        %s",cfg.opt.confirm_exit?"On":"Off");break;
   default:sprintf(b,"%-18s  Ink %2u  Paper %2u",roles[n-12],cfg.colors[n-12]&15,cfg.colors[n-12]>>4);break;
   }
   fill(6,5+i,68,1,' ',n==sel?SELECT:PANEL);text(7,5+i,b,65,n==sel?SELECT:PANEL);
   if(n>=12)text(51,5+i," Sample text ",14,n-12);
  }
  text(6,rows-5,"Arrows: select/change   PgUp/PgDn: page (or paper colour)",68,DIM);
  button(1,rows-3,"[Save]",SAVE);button(11,rows-3,"[Cancel]",27);button(23,rows-3,"[^]",UP);button(29,rows-3,"[v]",DOWN);
  button(35,rows-3,"[<]",LEFT);button(41,rows-3,"[>]",RIGHT);button(47,rows-3,"[PgUp]",PGUP);button(56,rows-3,"[PgDn]",PGDN);button(65,rows-3,"[Run]",13);
  text(2,rows-1,"Enter: change / run   Ctrl-S or F10: save   Esc: cancel",76,TITLE);flush();k=key();
  if(k==27||k==BACK_CLICK){cfg.mode=oldmode;cfg.theme=oldtheme;memcpy(cfg.colors,oldcolors,8);cfg.opt=oldopt;joy_settle=1;mode(cfg.mode);return;}
  if(k=='s'||k=='S'||k==F10||k==SAVE){if(commit())return;}
  if(k==CLICK&&mx>=6&&mx<74&&my>=5&&my<5+page&&start+my-5<20)sel=start+my-5;
  if(k==UP||k==DOWN||k==HOME||k==END||((k==PGUP||k==PGDN)&&sel<12)){scroll(&sel,&start,20,page,k);continue;}
  change=(k==RIGHT||k==13)?1:k==LEFT?-1:0;
  if(sel==0&&change){if(vga){cfg.mode=(unsigned char)((cfg.mode+change+3)%3);mode(cfg.mode);}}
  else if(sel==1&&change){cfg.theme=(unsigned char)((cfg.theme+change+11)%11);if(cfg.theme<CUSTOM_THEME)memcpy(cfg.colors,themes[cfg.theme],8);}
  else if(sel==2&&change)cfg.opt.clock_on^=1;
  else if(sel==3&&change)cfg.opt.clock24^=1;
  else if(sel==4&&change)cfg.opt.saver=(unsigned char)((cfg.opt.saver+change+5)%5);
  else if(sel==5&&change){if(cfg.opt.timeout+change>=1&&cfg.opt.timeout+change<=60)cfg.opt.timeout=(unsigned char)(cfg.opt.timeout+change);}
  else if(sel==6&&k==13){if(cfg.opt.saver)screensaver(cfg.opt.saver);}
  else if(sel==7&&change){if(cfg.opt.joy)cfg.opt.joy=0;else if(cfg.opt.xmin&&cfg.opt.xmin<cfg.opt.xmax&&cfg.opt.ymin&&cfg.opt.ymin<cfg.opt.ymax){cfg.opt.joy=1;joy_settle=1;}else calibrate_joy();}
  else if(sel==8&&k==13)calibrate_joy();
  else if(sel==9&&change)cfg.opt.repeat=(unsigned char)((cfg.opt.repeat+change+3)%3);
  else if(sel==10&&change)cfg.opt.start_last^=1;
  else if(sel==11&&change)cfg.opt.confirm_exit^=1;
  else if(sel>=12){
   if(change){cfg.colors[sel-12]=(unsigned char)((cfg.colors[sel-12]&0xf0)|((cfg.colors[sel-12]+change)&15));cfg.theme=CUSTOM_THEME;}
   if(k==PGUP||k==PGDN){cfg.colors[sel-12]=(unsigned char)(cfg.colors[sel-12]+(k==PGUP?16:-16));cfg.theme=CUSTOM_THEME;}
  }
 }
}
