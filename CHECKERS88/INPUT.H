/* BIOS keyboard, hidden INT 33 mouse, and our PIT-timed gameport reader. */
#define UP 0x4800
#define DOWN 0x5000
#define LEFT 0x4b00
#define RIGHT 0x4d00
#define F1 0x3b00
#define F2 0x3c00
#define F10 0x4400
extern unsigned joyread(unsigned *);
#pragma aux joyread parm[si] value[ax] modify[ax bx dx]
static int mousefound,mouseclick,mouseright,mouseold;
static int joyenabled,joyold,joysettle,joyfail,joydir;
static unsigned joyxmin,joyxmax,joyymin,joyymax,joylast,joydelay=14;
static void mouse_init(void) {
    union REGS r;
    void (__interrupt __far *v)(void)=_dos_getvect(0x33);
    if(!v || *(U8 __far *)v==0xcf) return;
    memset(&r,0,sizeof(r));int86(0x33,&r,&r);mousefound=r.w.ax==0xffff;
    if(mousefound) {
        r.w.ax=7;r.w.cx=6;r.w.dx=633;int86(0x33,&r,&r);
        r.w.ax=8;r.w.cx=6;r.w.dx=473;int86(0x33,&r,&r);
    }
}
static void mouse_poll(void) {
    union REGS r;int x,y,buttons;
    mouseclick=mouseright=0;if(!mousefound)return;
    r.w.ax=3;int86(0x33,&r,&r);buttons=r.w.bx&3;
    x=r.w.cx;y=r.w.dx;
    if(x<6)x=6;if(x>633)x=633;if(y<6)y=6;if(y>473)y=473;
    if(x!=pointerx||y!=pointery) {pointer_hide();pointerx=x;pointery=y;}
    pointer_show();
    mouseclick=(buttons&1)&&!(mouseold&1);mouseright=(buttons&2)&&!(mouseold&2);mouseold=buttons;
}
static int keyread(void) {
    unsigned k;
    if(!_bios_keybrd(_KEYBRD_READY))return 0;
    k=_bios_keybrd(_KEYBRD_READ);
    return k&255 ? k&255 : k;
}
static void keys_flush(void) {while(keyread()) {}}
static int joystick_key(void) {
    unsigned a[3];int dir=0,key=0;
    if(!joyenabled)return 0;
    if(!joyread(a)) {if(++joyfail>=8)joyenabled=0;return 0;}
    joyfail=0;
    if(joysettle) {
        if(!a[2]&&a[0]>=joyxmin&&a[0]<=joyxmax&&a[1]>=joyymin&&a[1]<=joyymax)joysettle=0;
        joyold=a[2];return 0;
    }
    if((a[2]&1)&&!(joyold&1))key=13;
    if((a[2]&2)&&!(joyold&2))key=27;
    joyold=a[2];
    if(a[0]<joyxmin)dir=LEFT;else if(a[0]>joyxmax)dir=RIGHT;
    else if(a[1]<joyymin)dir=UP;else if(a[1]>joyymax)dir=DOWN;
    if(dir && (dir!=joydir || (unsigned)(ticks-joylast)>=joydelay)) {
        if(!key)key=dir;joylast=ticks;
        joydelay=dir!=joydir?14:6;
    }
    joydir=dir;return key;
}
static int input(void) {
    int k=keyread(),j;
    static unsigned last;
    mouse_poll();
    if(last!=ticks) {last=ticks;j=joystick_key();if(!k)k=j;}
    if(mouseright&&!k)k=27;
    return k;
}
/* Static dialogs need the same input rate as play, not a busy polling loop.
 * IRQ music, BIOS ticks and the mouse driver keep running while halted. */
static int wait_input(void) {
    unsigned last=ticks;
    while(last==ticks)idle();
    return input();
}
static unsigned median(unsigned a,unsigned b,unsigned c) {
    unsigned t;if(a>b){t=a;a=b;b=t;}if(b>c){t=b;b=c;c=t;}if(a>b)b=a;return b;
}
static int calibrate(void) {
    static const char *stages[3]={"CENTER STICK - PRESS BUTTON 1","HOLD UP-LEFT - PRESS BUTTON 1","HOLD DOWN-RIGHT - PRESS BUTTON 1"};
    unsigned vals[3][2],a[3],b[3],c[3],last=ticks;
    int stage=0,release=0,fail=0;char old[48]="";
    joyenabled=0;screen_clear();title();center(115,"JOYSTICK CALIBRATION",15,2);
    center(365,"ESC CANCELS",15,1);
    while(stage<3) {
        if(last==ticks){idle();continue;}last=ticks;
        delta(128,235,old,stages[stage],15,2);
        if(keyread()==27)return 0;
        if(!joyread(a)) {if(++fail>=8)break;continue;}fail=0;
        if(!(a[2]&1))release=1;
        if((a[2]&1)&&release) {
            if(!joyread(b)||!joyread(c))continue;
            vals[stage][0]=median(a[0],b[0],c[0]);vals[stage][1]=median(a[1],b[1],c[1]);
            ++stage;release=0;
        }
    }
    if(stage==3&&vals[0][0]>vals[1][0]+8&&vals[2][0]>vals[0][0]+8&&
       vals[0][1]>vals[1][1]+8&&vals[2][1]>vals[0][1]+8) {
        joyxmin=vals[0][0]-(vals[0][0]-vals[1][0])/3;
        joyxmax=vals[0][0]+(vals[2][0]-vals[0][0])/3;
        joyymin=vals[0][1]-(vals[0][1]-vals[1][1])/3;
        joyymax=vals[0][1]+(vals[2][1]-vals[0][1])/3;
        joyenabled=1;joysettle=1;joyold=3;joydir=0;return 1;
    }
    screen_clear();center(210,"NO VALID JOYSTICK CALIBRATION",15,2);
    center(245,"KEYBOARD AND MOUSE STILL WORK",15,1);
    center(285,"PRESS A KEY",15,1);while(!keyread())idle();return 0;
}
