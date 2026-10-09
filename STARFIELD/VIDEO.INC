/* Adapted from our MATRIX 2.0 VGA/VBE/TVGA9000 backend. */
static unsigned word(int at){return info[at]|((unsigned)info[at+1]<<8);}
static void biosmode(unsigned mode){union REGS r;memset(&r,0,sizeof(r));r.w.ax=mode;int86(0x10,&r,&r);}
static int trident(void){unsigned i;const char __far *rom=MK_FP(0xc000,0);for(i=0;i<32760;i++)if(!_fmemcmp(rom+i,"Trident",7)||!_fmemcmp(rom+i,"TRIDENT",7))return 1;return 0;}
static void setbank(unsigned b){
 union REGS r;if(bank==(int)b)return;bank=b;
 if(driver==1){memset(&r,0,sizeof(r));r.w.ax=0x4f05;r.w.bx=window;r.w.dx=b*bank_factor;int86(0x10,&r,&r);if(r.w.ax!=0x004f)running=0;}
 else if(driver==2)outpw(0x3c4,((trident_flags|(b^2))<<8)|0x0e);
}
static unsigned char __far *address(uint32_t a){
 unsigned b;if(!driver)return MK_FP(0xa000,(unsigned)a);
 if(span_k==64){setbank((unsigned)(a>>16));return MK_FP(segment,(unsigned)a);}
 b=(unsigned)(a/((uint32_t)span_k*1024));setbank(b);
 return MK_FP(segment,(unsigned)(a-(uint32_t)b*span_k*1024));
}
static int vbe(unsigned mode){
 union REGS r;struct SREGS sr;unsigned attr,size;int win;
 memset(info,0,sizeof(info));memset(&r,0,sizeof(r));segread(&sr);
 r.w.ax=0x4f01;r.w.cx=mode;r.w.di=FP_OFF(info);sr.es=FP_SEG(info);int86x(0x10,&r,&r,&sr);
 if(r.w.ax!=0x004f)return 0;attr=word(0);
 if((attr&0x79)!=0x19||word(18)!=width||word(20)!=height||info[25]!=(depth==16?4:8)||info[27]!=(depth==16?3:4))return 0;
 if(depth==16&&info[24]!=4)return 0;
 size=depth==16?7:5;
 win=(info[2]&size)==size?0:(info[3]&size)==size?1:-1;if(win<0)return 0;
 gran=word(4);size=word(6);if(!gran||gran>64||!size)return 0;
 span_k=size>64?64:size;if(span_k%gran)return 0;
 pitch=word(16);segment=word(win?10:8);if(!segment||pitch<(unsigned)(depth==16?width/8:width))return 0;
 memset(&r,0,sizeof(r));r.w.ax=0x4f02;r.w.bx=mode;int86(0x10,&r,&r);if(r.w.ax!=0x004f)return 0;
 driver=1;window=win;bank_factor=span_k/gran;window_bytes=span_k*1024U;return 1;
}
static int startmode(int res,int color){
 static const unsigned vmode[3][2]={{0x12,0x101},{0x102,0x103},{0x104,0x105}};
 static const unsigned tmode[3][2]={{0x12,0x5d},{0x5b,0x5e},{0x5f,0}};
 union REGS r;unsigned mode;
 width=res==3?320:res==0?640:res==1?800:1024;height=res==3?200:res==0?480:res==1?600:768;depth=color?256:16;
 pitch=depth==16?width/8:width;segment=0xa000;span_k=gran=64;driver=0;bank=-1;bank_factor=1;window_bytes=0;
 if(res==3){biosmode(color?0x13:0x0d);return 1;}
 if(!res&&!color){biosmode(0x12);return 1;}
 /* Some VBE ROMs leave packed-mode CRTC state behind in planar modes. */
 if(depth==16)biosmode(0x12);
 if(vbe(vmode[res][color]))return 1;
 /* The NuXT's 512 KB TVGA9000i ROM provides these native modes. */
 if(!trident()||!(mode=tmode[res][color]))return 0;
 biosmode(mode);memset(&r,0,sizeof(r));r.h.ah=15;int86(0x10,&r,&r);if((r.h.al&127)!=mode)return 0;
 driver=2;pitch=depth==16?width/8:width;span_k=gran=64;segment=0xa000;
 outp(0x3c4,0x0b);inp(0x3c5);outp(0x3c4,0x0e);trident_flags=inp(0x3c5)&0xf0;
 return 1;
}
static int planar_probe(void){
 volatile unsigned char __far *v=address(256);unsigned c,p,want;
 /* Set/reset forces background planes to zero; other planes take host data.
  * Test both adjacent bytes in all sixteen colours, without plane switching
  * on writes. This also rejects unusable advertised VBE planar modes. */
 outpw(0x3c4,0x0f02);outpw(0x3ce,0x0000);
 for(c=0;c<16;c++){
  outpw(0x3ce,(((~c)&15)<<8)|1);v[0]=0xa5;v[1]=0x5a;
  for(p=0;p<4;p++){
   outpw(0x3ce,(p<<8)|4);want=(c&(1U<<p))?0xa5:0;
   if(v[0]!=want||v[1]!=(want?0x5a:0)){outpw(0x3ce,0x0004);outpw(0x3ce,0x0001);return 0;}
  }
 }
 outpw(0x3ce,0x0004);outpw(0x3ce,0x0001);return 1;
}

static void clear_video(void){
 uint32_t a=0,total=(uint32_t)pitch*height,bytes=(uint32_t)span_k*1024,left;
 unsigned n;
 if(depth==16){outpw(0x3c4,0x0f02);outpw(0x3ce,0x0001);}
 while(a<total){
  left=bytes-(a%bytes);if(left>total-a)left=total-a;
  n=left>16384UL?16384:(unsigned)left;_fmemset(address(a),0,n);a+=n;
 }
}

