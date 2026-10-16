/* Cached perspective geometry and reversible XOR objects. No frame buffer
 * copies, screen clears or geometry calculations during ordinary play. */
#include "FONT.H"
extern void video_init(int,int);
#pragma aux video_init parm[ax][bx] modify[ax bx cx dx]
extern void ink(int);
#pragma aux ink parm[ax] modify[ax dx]
extern void raster(int);
#pragma aux raster parm[ax] modify[ax dx]
extern void wipe(void);
#pragma aux wipe modify[ax cx dx di]
extern void dot(int,int);
#pragma aux dot parm[ax][bx] modify[ax bx cx di]
extern void segment(int,int,int,int);
#pragma aux segment parm[ax][bx][cx][dx] modify[ax bx cx dx si di]
/* Inclusive span, with VGA set/reset configured by solid_begin(). */
extern void solidspan(int,int,int);
#pragma aux solidspan parm[ax][bx][cx] modify[ax bx cx dx si di]
extern unsigned probe87(unsigned *);
#pragma aux probe87 parm[si] value[ax] modify[ax]
extern void control87(unsigned *);
#pragma aux control87 parm[si] modify exact []
extern void setcontrol87(unsigned *);
#pragma aux setcontrol87 parm[si] modify exact []
typedef struct {long num; int den,result;} Fraction;
extern int divide87(Fraction *);
#pragma aux divide87 parm[si] value[ax] modify[ax]
typedef struct {int x,y;} Point;
static Point grid[9][9], rings[32][2][12], crowns[32][7], corners[32][4], lights[32][4];
static Point rim[4],shoulder[4],belt[4],foot[4],surface[4];
static unsigned drawbases;
static int azimuth,elevation=2,camcos=1024,camsin,tiltcos,tiltsin;
static int camscale=1024,camx=320,camy=280;
static int have87,no87,theme=0;
static signed char displayed[32];
static U8 shownmarks[32];
static unsigned drawpieces,drawmarks;
static char hudtext[4][56];
static int pointerx=320,pointery=240,pointeron;
static int project_div(long n,int d) {
    Fraction f;
    if(!have87) return (int)(n/d);
    f.num=n; f.den=d; return divide87(&f);
}
/* Fixed-point orbit camera. Only rebuild when a view key changes it.
 * Perspective divisions can use the original 8087; neither path uses libm. */
static Point raw_project(int x,int z,int h) {
    Point p; int xx,zz,depth,up,den;
    z-=256;
    xx=(int)(((long)x*camcos-(long)z*camsin)/1024);
    zz=(int)(((long)x*camsin+(long)z*camcos)/1024);
    depth=(int)(((long)zz*tiltcos-(long)h*tiltsin)/1024);
    up=(int)(((long)zz*tiltsin+(long)h*tiltcos)/1024);
    den=1000+depth;
    p.x=project_div((long)xx*1024,den);
    p.y=-project_div((long)up*1024,den);return p;
}
static Point project(int x,int z,int h) {
    Point p=raw_project(x,z,h);
    p.x=camx+(int)((long)p.x*camscale/1024);
    p.y=camy+(int)((long)p.y*camscale/1024);return p;
}
static void camera_fit(void) {
    static const int cosine[24]={1024,989,887,724,512,265,0,-265,-512,-724,-887,-989,-1024,-989,-887,-724,-512,-265,0,265,512,724,887,989};
    static const int tc[5]={887,839,784,724,658},ts[5]={512,587,658,724,784};
    int x,z,h,loX=32767,hiX=-32767,loY=32767,hiY=-32767,sy;Point a;
    camcos=cosine[azimuth];camsin=cosine[(azimuth+18)%24];
    tiltcos=tc[elevation];tiltsin=ts[elevation];
    for(x=-278;x<=278;x+=556)for(z=-22;z<=534;z+=556)for(h=-36;h<=40;h+=76) {
        a=raw_project(x,z,h);
        if(a.x<loX)loX=a.x;if(a.x>hiX)hiX=a.x;
        if(a.y<loY)loY=a.y;if(a.y>hiY)hiY=a.y;
    }
    camscale=(int)(568L*1024/(hiX-loX));sy=(int)(286L*1024/(hiY-loY));
    if(sy<camscale)camscale=sy;
    camx=320-(int)((long)(loX+hiX)*camscale/2048);
    camy=286-(int)((long)(loY+hiY)*camscale/2048);
}
static void geometry(void) {
    /* Radius 22, truncated exactly as the original Q10 sine/cosine math.
     * Shared offsets avoid repeating the fixed-point scaling for every piece. */
    static const int rx[12]={22,19,11,0,-11,-19,-22,-19,-11,0,11,19};
    static const int rz[12]={0,11,19,22,19,11,0,-11,-19,-22,-19,-11};
    unsigned cw,localcw;
    int r,c,s,k,x,z,dx,dz;
    static const int crownx[7]={-14,-18,-7,0,7,18,14},crownh[7]={17,34,26,40,26,34,17};
    if(have87) {control87(&cw); localcw=(cw&~0xc00)|0xc00; setcontrol87(&localcw);}
    camera_fit();
    for(r=0;r<9;++r) for(c=0;c<9;++c) grid[r][c]=project(c*64-256,(8-r)*64,0);
    for(s=0;s<32;++s) {
        r=rowof(s); c=colof(s); x=c*64-224; z=(7-r)*64+32;
        for(k=0;k<12;++k) {
            dx=rx[k];dz=rz[k];
            rings[s][0][k]=project(x+dx,z+dz,5);
            rings[s][1][k]=project(x+dx,z+dz,15);
        }
        corners[s][0]=project(x-28,z+28,1);
        corners[s][1]=project(x+28,z+28,1);
        corners[s][2]=project(x+28,z-28,1);
        corners[s][3]=project(x-28,z-28,1);
        for(k=0;k<7;++k) crowns[s][k]=project(x+(int)((long)crownx[k]*camcos/1024),
            z-(int)((long)crownx[k]*camsin/1024),crownh[k]);
        /* Inset white outlines on the light squares leave shared grid edges dim. */
        c^=1;x=c*64-224;
        lights[s][0]=project(x-29,z+29,0);lights[s][1]=project(x+29,z+29,0);
        lights[s][2]=project(x+29,z-29,0);lights[s][3]=project(x-29,z-29,0);
    }
    /* Clockwise world perimeter from the near-left corner. */
    for(k=0;k<4;++k) {
        dx=k==0||k==3?-1:1;dz=k<2?-1:1;
        surface[k]=project(dx*256,256+dz*256,0);
        rim[k]=project(dx*272,256+dz*272,0);
        shoulder[k]=project(dx*278,256+dz*278,-7);
        belt[k]=project(dx*278,256+dz*278,-26);
        foot[k]=project(dx*272,256+dz*272,-36);
    }
    if(have87) setcontrol87(&cw);
}
static void line(Point a,Point b) {segment(a.x,a.y,b.x,b.y);}
static void rectangle(int x,int y,int w,int h) {
    segment(x,y,x+w,y); segment(x+w,y,x+w,y+h);
    segment(x+w,y+h,x,y+h); segment(x,y+h,x,y);
}
static unsigned glyph(int c,int r) {
    if(c=='!') return r==5?0:32;
    if(c=='[') return r==0||r==6?112:64;
    if(c==']') return r==0||r==6?112:16;
    if(c=='>') {static const U8 a[7]={128,64,32,16,32,64,128};return a[r];}
    if(c=='<') {static const U8 a[7]={8,16,32,64,32,16,8};return a[r];}
    if(c=='?') {static const U8 a[7]={112,136,8,16,32,0,32};return a[r];}
    if(c=='=') return r==2||r==4?248:0;
    if(c<32||c>90) return 0;
    return font[(c-32)*7+r];
}
static void text(int x,int y,const char *s,int color,int size) {
    int r,c,a,b;
    unsigned bits;
    ink(color); raster(0x18);
    while(*s) {
        for(r=0;r<7;++r) {
            bits=glyph(*s,r);
            for(c=0;c<5;++c) if(bits&(128>>c))
                for(a=0;a<size;++a) for(b=0;b<size;++b) dot(x+c*size+a,y+r*size+b);
        }
        x+=size*6; ++s;
    }
}
static void center(int y,const char *s,int color,int size) {
    text((640-(int)strlen(s)*size*6)/2,y,s,color,size);
}
static void delta(int x,int y,char *old,const char *s,int color,int size) {
    int i,olen=strlen(old),nlen=strlen(s);
    char a[2]={0,0};
    for(i=0;i<olen||i<nlen;++i) if((i<olen?old[i]:' ')!=(i<nlen?s[i]:' ')) {
        a[0]=i<olen?old[i]:' '; text(x+i*6*size,y,a,color,size);
        a[0]=i<nlen?s[i]:' '; text(x+i*6*size,y,a,color,size);
    }
    strcpy(old,s);
}
static void dac(int i,int r,int g,int b) {
    outp(0x3c8,i); outp(0x3c9,r); outp(0x3c9,g); outp(0x3c9,b);
}
static void palette(void) {
    static const U8 colors[4][4][3]={
        {{12,18,27},{63,12,25},{8,53,63},{62,49,20}},
        {{15,17,20},{60,15,12},{18,35,63},{22,55,30}},
        {{20,13,4},{63,16,8},{16,44,63},{63,52,20}},
        {{9,17,24},{63,16,26},{18,51,63},{60,61,63}}
    };
    int i,k;
    dac(0,0,0,0);
    for(i=1;i<16;++i) {
        k=(i&7)==3?1:(i&7)==5?2:(i&7)==6?3:0;
        if(i==1) dac(i,colors[theme][0][0]/2,colors[theme][0][1]/2,colors[theme][0][2]/2);
        else if(i==2) dac(i,colors[theme][0][0],colors[theme][0][1],colors[theme][0][2]);
        else if(i==4) dac(i,colors[theme][0][0]+16,colors[theme][0][1]+16,colors[theme][0][2]+16);
        else if(i==7||i==15) dac(i,59,60,63);
        else if(i==8) dac(i,colors[theme][0][0],colors[theme][0][1],colors[theme][0][2]);
        else dac(i,colors[theme][k][0],colors[theme][k][1],colors[theme][k][2]);
    }
}
static void biosmode(int m) {union REGS r;memset(&r,0,sizeof(r));r.w.ax=m;int86(0x10,&r,&r);}
static int modequery(void) {union REGS r;r.h.ah=15;int86(0x10,&r,&r);return r.h.al&127;}
static int video_start(void) {
    union REGS r; int i;
    biosmode(0x12); if(modequery()!=0x12) return 0;
    for(i=0;i<16;++i) {memset(&r,0,sizeof(r));r.w.ax=0x1000;r.h.bl=i;r.h.bh=i;int86(0x10,&r,&r);}
    memset(&r,0,sizeof(r));r.w.ax=0x1013;int86(0x10,&r,&r);
    r.w.ax=0x1013;r.h.bl=1;r.h.bh=0;int86(0x10,&r,&r);
    outpw(0x3c4,0x0604);outpw(0x3ce,0x0506);
    outpw(0x3ce,0x0000);outpw(0x3ce,0x0001);outpw(0x3ce,0x0003);
    outpw(0x3ce,0x0005);outpw(0x3ce,0xff08);outpw(0x3c4,0x0f02);
    outp(0x3c6,255); video_init(80,480);palette();wipe();return 1;
}
static void cursor_shape(void) {
    ink(7);raster(0x18);
    segment(pointerx-5,pointery,pointerx-1,pointery);
    segment(pointerx+2,pointery,pointerx+6,pointery);
    segment(pointerx,pointery-5,pointerx,pointery-1);
    segment(pointerx,pointery+2,pointerx,pointery+6);
}
static void pointer_hide(void) {if(pointeron) {cursor_shape();pointeron=0;}}
static void pointer_show(void) {if(!pointeron) {cursor_shape();pointeron=1;}}
static void screen_clear(void) {pointer_hide();wipe();}
static void title(void) {
    center(25,"3D CHECKERS 88",15,3);
    ink(8);segment(30,61,610,61);
}
static void piece(int s,int p) {
    int k;
    ink(p>0?3:5);raster(0x18);
    for(k=0;k<12;++k) {
        line(rings[s][0][k],rings[s][0][(k+1)%12]);
        line(rings[s][1][k],rings[s][1][(k+1)%12]);
        if(!(k%3)) line(rings[s][0][k],rings[s][1][k]);
    }
    if(p==2||p==-2) {
        for(k=0;k<6;++k) line(crowns[s][k],crowns[s][k+1]);
        line(crowns[s][6],crowns[s][0]);
    }
    ++drawpieces;
}
/* 1=cursor; 2=selected; 4=landing; 8=movable; 16=last from; 32=last to. */
static void mark(int s,int kind) {
    int k,x=(corners[s][0].x+corners[s][2].x)/2,y=(corners[s][0].y+corners[s][2].y)/2;
    raster(0x18);
    if(kind&3) { ink(kind&2?6:7); for(k=0;k<4;++k) line(corners[s][k],corners[s][(k+1)%4]); }
    if(kind&4) {ink(6);rectangle(x-4,y-2,8,4);}
    if(kind&8) {ink(6);segment(x-3,y+9,x+4,y+9);}
    if(kind&16) {ink(8);rectangle(x-6,y-4,12,8);}
    if(kind&32) {ink(6);segment(x-6,y+5,x+7,y+5);}
    ++drawmarks;
}
/* Static solid base. Set/reset fills all four planes, preserving masked
 * neighbours. Restore the XOR setup before pieces or the cursor are drawn. */
static void solid_begin(int color) {
    ink(15);raster(0);outpw(0x3ce,(color<<8)|0);outpw(0x3ce,0x0f01);
}
static void solid_end(void) {
    outpw(0x3ce,0x0005);outpw(0x3ce,0x0001);outpw(0x3ce,0xff08);raster(0x18);
}
static long face_area(const Point *q) {
    long area=0;int i,j;
    for(i=0;i<4;++i){j=(i+1)&3;area+=(long)q[i].x*q[j].y-(long)q[j].x*q[i].y;}
    return area;
}
static void fill_face(const Point *q,int color) {
    int i,y,top=479,bottom=0,left,right,x;
    for(i=0;i<4;++i){if(q[i].y<top)top=q[i].y;if(q[i].y>bottom)bottom=q[i].y;}
    solid_begin(color);
    for(y=top;y<bottom;++y) {
        left=640;right=-1;
        for(i=0;i<4;++i) {
            Point a=q[i],b=q[(i+1)&3];
            if((y>=a.y&&y<b.y)||(y>=b.y&&y<a.y)) {
                /* Sample at the scanline centre. The convex faces need two edges. */
                x=a.x+(int)((long)(2*(y-a.y)+1)*(b.x-a.x)/(2*(b.y-a.y)));
                if(x<left)left=x;if(x>right)right=x;
            }
        }
        if(left<=right)solidspan(left,y,right);
    }
    solid_end();
}
static void base_strip(const Point *upper,const Point *lower,int color,int all) {
    Point q[4];int i,j;
    for(i=0;i<4;++i) {
        j=(i+1)&3;q[0]=upper[i];q[1]=upper[j];q[2]=lower[j];q[3]=lower[i];
        if(all||face_area(q)>0) {
            fill_face(q,color==0?(i==0||i==3?2:1):color);
            /* VGA write mode 3 turns each line's CPU byte into a mask:
             * exact edge colour, including shared corners, without XOR seams. */
            solid_begin(4);outpw(0x3ce,0x0305);
            line(q[0],q[1]);line(q[1],q[2]);line(q[2],q[3]);line(q[3],q[0]);
            solid_end();
        }
    }
    raster(0x18);
}
static void draw_base(void) {
    int i;
    base_strip(shoulder,belt,0,0); /* visible vertical faces, two shades */
    base_strip(belt,foot,1,0);     /* bevel below the body */
    base_strip(rim,shoulder,4,0);  /* top chamfer */
    base_strip(surface,rim,1,1);  /* narrow top frame */
    fill_face(surface,0);         /* playing surface remains black */
    solid_begin(4);outpw(0x3ce,0x0305);for(i=0;i<4;++i)line(rim[i],rim[(i+1)&3]);solid_end();
    raster(0x18);++drawbases;
}
static void draw_board(const Board *b) {
    int r,c,s;
    screen_clear();draw_base();ink(8);raster(0x18);
    for(r=0;r<9;++r) line(grid[r][0],grid[r][8]);
    for(c=0;c<9;++c) line(grid[0][c],grid[8][c]);
    for(s=0;s<32;++s) {
        ink(7);for(c=0;c<4;++c)line(lights[s][c],lights[s][(c+1)%4]);
    }
    for(s=0;s<32;++s) {
        int x=(corners[s][0].x+corners[s][2].x)/2;
        int y=(corners[s][0].y+corners[s][2].y)/2;
        ink(8); segment(x-2,y,x+3,y); /* playable-square centre */
        if(b->cell[s]) piece(s,b->cell[s]);
    }
    memcpy(displayed,b->cell,32);memset(shownmarks,0,32);memset(hudtext,0,sizeof(hudtext));
    text(32,454,"F1 HELP   U UNDO   WASD VIEW   R RESET   ESC MENU",15,1);
}
static void update_board(const Board *b,const U8 *marks) {
    int s;
    for(s=0;s<32;++s) {
        if(displayed[s]!=b->cell[s]) {
            if(displayed[s]) piece(s,displayed[s]);
            if(b->cell[s]) piece(s,b->cell[s]);
            displayed[s]=b->cell[s];
        }
        if(shownmarks[s]!=marks[s]) {
            if(shownmarks[s]) mark(s,shownmarks[s]);
            if(marks[s]) mark(s,marks[s]);
            shownmarks[s]=marks[s];
        }
    }
}
/* Convex projected squares work at every camera angle, including a full turn. */
static int hit_square(int x,int y) {
    int r,c,k,pos,neg;Point q[4];long cross;
    for(r=0;r<8;++r)for(c=0;c<8;++c) {
        q[0]=grid[r][c];q[1]=grid[r][c+1];q[2]=grid[r+1][c+1];q[3]=grid[r+1][c];
        pos=neg=0;
        for(k=0;k<4;++k) {
            Point a=q[k],b=q[(k+1)%4];
            cross=(long)(b.x-a.x)*(y-a.y)-(long)(b.y-a.y)*(x-a.x);
            if(cross>0)pos=1;else if(cross<0)neg=1;
        }
        if(!(pos&&neg))return square(r,c);
    }
    return -1;
}
