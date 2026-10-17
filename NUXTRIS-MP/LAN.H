/* NuXTris LAN transport. Copyright 2026 Marquak.
 * Adapted callback/registration: Copyright (C) 2005-2026 Michael B. Brutman.
 * GPL-3.0-or-later. The packet-driver callback and registration sequence
 * are adapted from mTCP Packet.cpp (GPL-3.0-or-later).
 * Only small Ethernet/ARP/IPv4/UDP frames are handled; game snapshots have
 * their own CRC and sequence checks in LINK.H and MPLAY.C. */
#include <i86.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LAN_PORT 47587U
#define LAN_SLOTS 4
#define LAN_FRAME 256
#define LAN_WIRE (14+20+8+LSIZE)
static volatile unsigned char lan_state[LAN_SLOTS];
static volatile unsigned lan_length[LAN_SLOTS];
static unsigned char lan_frames[LAN_SLOTS][LAN_FRAME],lan_tx[LAN_FRAME];
static volatile unsigned char lan_pending;
static unsigned char lan_local[4],lan_mask[4],lan_gateway[4],lan_peerip[4];
static unsigned char lan_mac[6],lan_peermac[6],lan_sourceip[4],lan_sourcemac[6];
static unsigned char lan_payload[LSIZE];
static unsigned lan_int,lan_handle,lan_lastarp,lan_bad,lan_senterror;
static volatile unsigned lan_dropped;
static unsigned lan_peerport,lan_sourceport;
static int lan_up,lan_peerknown,lan_macknown;
static char lan_reason[64];

/* Packet drivers call the receiver twice. First return ES:DI for a buffer;
 * second publish that completed buffer. No DOS, parsing or drawing in it. */
static void __far __interrupt lan_receiver(union INTPACK r) {
    unsigned i;
    if(r.w.ax==0) {
        r.w.es=r.w.di=0;
        /* Game UDP frames and ARP fit here. Larger unrelated LAN traffic
         * is deliberately ignored, not reported as a dropped game frame. */
        if(r.w.cx>=42 && r.w.cx<=LAN_FRAME) for(i=0;i<LAN_SLOTS;i++) if(lan_state[i]==0) {
            lan_state[i]=1; lan_pending=(unsigned char)(i+1);
            r.w.es=FP_SEG(lan_frames[i]); r.w.di=FP_OFF(lan_frames[i]);
            break;
        }
        if(r.w.cx>=42 && r.w.cx<=LAN_FRAME && !r.w.es) ++lan_dropped;
    } else if(lan_pending) {
        i=lan_pending-1; lan_pending=0;
        if(r.w.cx>LAN_FRAME) { lan_state[i]=0; ++lan_dropped; }
        else { lan_length[i]=r.w.cx; lan_state[i]=2; }
    }
    /* A far CALL from a packet driver needs RETF, not IRET. This follows
     * mTCP's tested Watcom callback epilogue, including returned ES:DI. */
    _asm {
        pop ax
        pop ax
        pop es
        pop ds
        pop di
        pop si
        pop bp
        pop bx
        pop bx
        pop dx
        pop cx
        pop ax
        retf
    }
}
static int lan_parseip(const char *s,unsigned char *ip) {
    unsigned a,b,c,d; char tail;
    if(sscanf(s,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)!=4 ||
       a>255 || b>255 || c>255 || d>255) return 0;
    ip[0]=a; ip[1]=b; ip[2]=c; ip[3]=d; return 1;
}
static int lan_sameip(const unsigned char *a,const unsigned char *b) {
    return !memcmp(a,b,4);
}
static int lan_onlink(const unsigned char *a) {
    unsigned i;
    for(i=0;i<4;i++) if((a[i]&lan_mask[i])!=(lan_local[i]&lan_mask[i])) return 0;
    return 1;
}
static int lan_config(void) {
    const char *path=getenv("MTCPCFG");
    FILE *f; char line[180],*key,*value,*end; unsigned found=0;
    unsigned long driverint;
    if(!path || !*path) { strcpy(lan_reason,"SET MTCPCFG BEFORE LAN PLAY"); return 0; }
    f=fopen(path,"rt");
    if(!f) { strcpy(lan_reason,"CANNOT OPEN MTCPCFG FILE"); return 0; }
    memset(lan_gateway,0,sizeof(lan_gateway));
    while(fgets(line,sizeof(line),f)) {
        key=strtok(line," \t\r\n"); if(!key || *key=='#') continue;
        value=strtok(0," \t\r\n"); if(!value) continue;
        if(!stricmp(key,"PACKETINT")) {
            driverint=strtoul(value,&end,0);
            if(*end || driverint<0x20 || driverint>0xff) goto bad;
            lan_int=(unsigned)driverint; found|=1;
        } else if(!stricmp(key,"IPADDR")) {
            if(!lan_parseip(value,lan_local)) goto bad; found|=2;
        } else if(!stricmp(key,"NETMASK")) {
            if(!lan_parseip(value,lan_mask)) goto bad; found|=4;
        } else if(!stricmp(key,"GATEWAY")) {
            if(!lan_parseip(value,lan_gateway)) goto bad; found|=8;
        }
    }
    fclose(f);
    if((found&7)!=7 || !lan_local[0] || !lan_mask[0]) {
        strcpy(lan_reason,"MTCPCFG NEEDS PACKETINT/IPADDR/NETMASK"); return 0;
    }
    return 1;
bad:
    fclose(f); strcpy(lan_reason,"INVALID MTCPCFG NETWORK SETTING"); return 0;
}
static unsigned lan_checksum(const unsigned char *p,unsigned n) {
    unsigned long sum=0;
    while(n>=2) { sum+=((unsigned)p[0]<<8)|p[1]; p+=2; n-=2; }
    if(n) sum+=(unsigned)p[0]<<8;
    while(sum>>16) sum=(sum&65535UL)+(sum>>16);
    return (unsigned)(~sum)&65535U;
}
static int lan_drivercall(unsigned fn,unsigned size,unsigned char *buffer) {
    union REGS in,out; struct SREGS seg;
    memset(&in,0,sizeof(in)); segread(&seg);
    in.h.ah=fn; in.x.bx=lan_handle; in.x.cx=size;
    in.x.si=(unsigned)buffer;
    int86x(lan_int,&in,&out,&seg);
    return !out.x.cflag;
}
static int lan_sendframe(unsigned size) {
    if(size<60) size=60;
    if(lan_drivercall(4,size,lan_tx)) return 1;
    ++lan_senterror; return 0;
}
static void lan_arp(const unsigned char *target,const unsigned char *destmac,int reply) {
    unsigned char *p=lan_tx+14;
    memset(lan_tx,0,60);
    memcpy(lan_tx,destmac,6); memcpy(lan_tx+6,lan_mac,6);
    lan_tx[12]=8; lan_tx[13]=6;
    p[1]=1; p[2]=8; p[4]=6; p[5]=4; p[7]=reply?2:1;
    memcpy(p+8,lan_mac,6); memcpy(p+14,lan_local,4);
    if(reply) memcpy(p+18,destmac,6);
    memcpy(p+24,target,4);
    lan_sendframe(60);
}
static const unsigned char *lan_hop(void) {
    return lan_onlink(lan_peerip)?lan_peerip:lan_gateway;
}
static int lan_send(const unsigned char *data) {
    static const unsigned char broadcast[6]={255,255,255,255,255,255};
    unsigned char *ip=lan_tx+14,*udp=ip+20;
    if(!lan_peerknown) return 0;
    if(!lan_macknown) {
        if((unsigned)(ticks-lan_lastarp)>=18) {
            lan_lastarp=ticks; lan_arp(lan_hop(),broadcast,0);
        }
        return 0;
    }
    memcpy(lan_tx,lan_peermac,6); memcpy(lan_tx+6,lan_mac,6);
    lan_tx[12]=8; lan_tx[13]=0;
    memset(ip,0,20); ip[0]=0x45; ip[2]=(20+8+LSIZE)>>8;
    ip[3]=(20+8+LSIZE)&255; ip[6]=0x40; ip[8]=64; ip[9]=17;
    memcpy(ip+12,lan_local,4); memcpy(ip+16,lan_peerip,4);
    { unsigned check=lan_checksum(ip,20); ip[10]=check>>8; ip[11]=check&255; }
    udp[0]=LAN_PORT>>8; udp[1]=LAN_PORT&255;
    udp[2]=lan_peerport>>8; udp[3]=lan_peerport&255;
    udp[4]=(8+LSIZE)>>8; udp[5]=(8+LSIZE)&255;
    udp[6]=udp[7]=0; /* IPv4 UDP checksum is optional; snapshot has CRC. */
    memcpy(udp+8,data,LSIZE);
    return lan_sendframe(LAN_WIRE);
}
static void lan_close(void) {
    if(lan_up) { lan_drivercall(3,0,lan_tx); lan_up=0; }
}
static int lan_open(unsigned char *joinip) {
    union REGS in,out; struct SREGS seg;
    unsigned char __far *vector;
    unsigned char __far *eye;
    static const char signature[]="PKT DRVR";
    unsigned i;
    lan_reason[0]=0; lan_up=lan_peerknown=lan_macknown=0;
    lan_peerport=LAN_PORT; lan_sourceport=0;
    lan_dropped=lan_bad=lan_senterror=0; lan_pending=0;
    memset((void *)lan_state,0,sizeof(lan_state));
    if(!lan_config()) return 0;
    if(!role) {
        if(lan_sameip(joinip,lan_local)) {
            strcpy(lan_reason,"PEER IP IS THIS COMPUTER"); return 0;
        }
        if(!lan_onlink(joinip) &&
           (!lan_gateway[0] || !lan_onlink(lan_gateway) ||
            lan_sameip(lan_gateway,lan_local))) {
            strcpy(lan_reason,"MTCPCFG NEEDS A LOCAL GATEWAY"); return 0;
        }
        memcpy(lan_peerip,joinip,4); lan_peerknown=1;
    }
    vector=(unsigned char __far *)MK_FP(0,lan_int*4);
    if(!*(unsigned __far *)vector || !*(unsigned __far *)(vector+2)) {
        strcpy(lan_reason,"PACKET DRIVER NOT FOUND"); return 0;
    }
    eye=(unsigned char __far *)MK_FP(*(unsigned __far *)(vector+2),*(unsigned __far *)vector);
    eye+=3;
    for(i=0;i<8;i++) if(eye[i]!=(unsigned char)signature[i]) {
        strcpy(lan_reason,"PACKET DRIVER NOT FOUND"); return 0;
    }
    memset(&in,0,sizeof(in)); memset(&seg,0,sizeof(seg));
    in.h.ah=2; in.h.al=1; in.x.bx=0xffff; in.x.cx=0;
    in.h.dl=0; in.x.si=0; seg.ds=0;
    in.x.di=FP_OFF(lan_receiver); seg.es=FP_SEG(lan_receiver);
    int86x(lan_int,&in,&out,&seg);
    if(out.x.cflag) { strcpy(lan_reason,"PACKET DRIVER IS BUSY OR INCOMPATIBLE"); return 0; }
    lan_handle=out.x.ax; lan_up=1;
    memset(&in,0,sizeof(in)); segread(&seg);
    in.h.ah=6; in.x.bx=lan_handle; in.x.cx=6;
    in.x.di=(unsigned)lan_mac; seg.es=seg.ds;
    int86x(lan_int,&in,&out,&seg);
    if(out.x.cflag) { lan_close(); strcpy(lan_reason,"CANNOT READ NETWORK CARD ADDRESS"); return 0; }
    lan_lastarp=ticks-18;
    return 1;
}
static int lan_read(void) {
    unsigned n,slot;
    unsigned char *p,*ip,*udp;
    for(slot=0;slot<LAN_SLOTS;slot++) if(lan_state[slot]==2) {
        _disable(); lan_state[slot]=3; n=lan_length[slot]; _enable();
        p=lan_frames[slot];
        if(n>=42 && p[12]==8 && p[13]==6 &&
           p[14]==0 && p[15]==1 && p[16]==8 && p[17]==0 &&
           p[18]==6 && p[19]==4 && lan_sameip(p+38,lan_local)) {
            if(p[20]==0 && p[21]==1) lan_arp(p+28,p+22,1);
            if(p[20]==0 && p[21]==2 && lan_peerknown &&
               lan_sameip(p+28,lan_hop())) {
                memcpy(lan_peermac,p+22,6); lan_macknown=1;
            }
        } else if(n>=LAN_WIRE && p[12]==8 && p[13]==0) {
            ip=p+14; udp=ip+20;
            if(ip[0]==0x45 && ip[9]==17 && (ip[6]&0x3f)==0 && ip[7]==0 &&
               ((unsigned)ip[2]<<8|ip[3])>=20+8+LSIZE &&
               ((unsigned)ip[2]<<8|ip[3])<=n-14 &&
               !lan_checksum(ip,20) && lan_sameip(ip+16,lan_local) &&
               (!lan_peerknown || lan_sameip(ip+12,lan_peerip)) &&
               (udp[0] || udp[1]) &&
               udp[2]==LAN_PORT>>8 && udp[3]==(LAN_PORT&255) &&
               (((unsigned)udp[4]<<8)|udp[5])==8+LSIZE) {
                memcpy(lan_payload,udp+8,LSIZE);
                memcpy(lan_sourceip,ip+12,4); memcpy(lan_sourcemac,p+6,6);
                lan_sourceport=((unsigned)udp[0]<<8)|udp[1];
                lan_state[slot]=0;
                if(lvalid(lan_payload)) return 1;
                ++lan_bad; return -1;
            }
        }
        lan_state[slot]=0;
    }
    return 0;
}
static void lan_accept_source(void) {
    if(!lan_peerknown) { memcpy(lan_peerip,lan_sourceip,4); lan_peerknown=1; }
    lan_peerport=lan_sourceport;
    memcpy(lan_peermac,lan_sourcemac,6); lan_macknown=1;
}
