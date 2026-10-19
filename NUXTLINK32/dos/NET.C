/* NuXT Link LAN packet-driver transport. GPL-3.0-or-later.
 * The two-call Watcom packet-driver callback and registration sequence are
 * adapted from Michael B. Brutman's mTCP Packet.cpp, GPL-3.0-or-later.
 * One unfragmented IPv4/UDP datagram carries one NuXT Link v6 frame.
 */
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NET_PORT 47588U
#define NET_SLOTS 3
/* 1440-byte data blocks fit in one standard Ethernet frame. */
#define NET_MTU 1514
#define NET_DATA 1472
typedef unsigned char U8;
static volatile U8 slot_state[NET_SLOTS];
static volatile unsigned slot_length[NET_SLOTS];
static U8 slot_frame[NET_SLOTS][NET_MTU],tx_frame[NET_MTU];
static volatile U8 pending;
static U8 local_ip[4],mask[4],peer_ip[4],candidate_ip[4],local_mac[6];
static U8 answer_ip[4],answer_mac[6];
static unsigned answer_port,peer_port,candidate_port,driver_int,driver_handle,opened,peer_locked,candidate_valid;
static unsigned long dropped,bad;
static char reason[96];
static char local_text[16],peer_text[16];
void net_close(void);

static void __far __interrupt receiver(union INTPACK r){
    unsigned i;
    if(r.w.ax==0){
        r.w.es=r.w.di=0;
        if(r.w.cx>=42&&r.w.cx<=NET_MTU){
            for(i=0;i<NET_SLOTS;i++)if(slot_state[i]==0){
                slot_state[i]=1;pending=(U8)(i+1);
                r.w.es=FP_SEG(slot_frame[i]);r.w.di=FP_OFF(slot_frame[i]);break;
            }
            if(!r.w.es)dropped++;
        }
    }else if(pending){
        i=pending-1;pending=0;
        if(r.w.cx>NET_MTU){slot_state[i]=0;dropped++;}
        else{slot_length[i]=r.w.cx;slot_state[i]=2;}
    }
    /* Packet driver used far CALL, so return with RETF, preserving ES:DI. */
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
static int ip_parse(const char *s,U8 *out){
    unsigned a,b,c,d;char tail;
    if(sscanf(s,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)!=4||a>255||b>255||c>255||d>255)return 0;
    out[0]=(U8)a;out[1]=(U8)b;out[2]=(U8)c;out[3]=(U8)d;return 1;
}
static unsigned checksum(const U8 *p,unsigned n){
    unsigned long sum=0;
    while(n>=2){sum+=((unsigned)p[0]<<8)|p[1];p+=2;n-=2;}
    if(n)sum+=(unsigned)p[0]<<8;
    while(sum>>16)sum=(sum&65535UL)+(sum>>16);
    return (unsigned)~sum;
}
static int driver_call(unsigned fn,unsigned size,U8 *buffer){
    union REGS in,out;struct SREGS seg;
    memset(&in,0,sizeof(in));segread(&seg);
    in.h.ah=(U8)fn;in.x.bx=driver_handle;in.x.cx=size;in.x.si=(unsigned)buffer;
    int86x(driver_int,&in,&out,&seg);return !out.x.cflag;
}
static int send_frame(U8 *frame,unsigned size){
    if(size<60)size=60;
    return size<=NET_MTU&&driver_call(4,size,frame);
}
static void arp_reply(const U8 *request){
    U8 *p=tx_frame+14;
    memset(tx_frame,0,60);
    memcpy(tx_frame,request+22,6);memcpy(tx_frame+6,local_mac,6);
    tx_frame[12]=8;tx_frame[13]=6;
    p[1]=1;p[2]=8;p[4]=6;p[5]=4;p[7]=2;
    memcpy(p+8,local_mac,6);memcpy(p+14,local_ip,4);
    memcpy(p+18,request+22,6);memcpy(p+24,request+28,4);
    send_frame(tx_frame,60);
}
static int config(void){
    const char *path=getenv("MTCPCFG");FILE *f;char line[180],*key,*value,*end;
    unsigned found=0;unsigned long number;
    if(!path||!*path){strcpy(reason,"Set MTCPCFG to the mTCP config file.");return 0;}
    f=fopen(path,"rt");if(!f){strcpy(reason,"Cannot open MTCPCFG file.");return 0;}
    while(fgets(line,sizeof(line),f)){
        key=strtok(line," \t\r\n");if(!key||*key=='#')continue;
        value=strtok(0," \t\r\n");if(!value)continue;
        if(!stricmp(key,"PACKETINT")){
            number=strtoul(value,&end,0);if(*end||number<0x20||number>0xff)goto invalid;
            driver_int=(unsigned)number;found|=1;
        }else if(!stricmp(key,"IPADDR")){
            if(!ip_parse(value,local_ip))goto invalid;found|=2;
        }else if(!stricmp(key,"NETMASK")){
            if(!ip_parse(value,mask))goto invalid;found|=4;
        }
    }
    fclose(f);
    if((found&7)!=7||!local_ip[0]||!mask[0]){
        strcpy(reason,"MTCPCFG needs PACKETINT, IPADDR and NETMASK.");return 0;
    }
    return 1;
invalid:
    fclose(f);strcpy(reason,"Invalid network setting in MTCPCFG.");return 0;
}
const char *net_error(void){return reason;}
int net_open(void){
    union REGS in,out;struct SREGS seg;U8 __far *vector;U8 __far *eye;
    unsigned i;static const char signature[]="PKT DRVR";
    reason[0]=0;opened=0;pending=0;dropped=bad=0;peer_locked=candidate_valid=0;
    memset((void *)slot_state,0,sizeof(slot_state));
    if(!config())return 0;
    vector=(U8 __far *)MK_FP(0,driver_int*4);
    if(!*(unsigned __far *)vector||!*(unsigned __far *)(vector+2))goto missing;
    eye=(U8 __far *)MK_FP(*(unsigned __far *)(vector+2),*(unsigned __far *)vector);eye+=3;
    for(i=0;i<8;i++)if(eye[i]!=(U8)signature[i])goto missing;
    memset(&in,0,sizeof(in));memset(&seg,0,sizeof(seg));
    in.h.ah=2;in.h.al=1;in.x.bx=0xffff;
    in.x.di=FP_OFF(receiver);seg.es=FP_SEG(receiver);
    int86x(driver_int,&in,&out,&seg);
    if(out.x.cflag){strcpy(reason,"Packet driver is busy or incompatible.");return 0;}
    driver_handle=out.x.ax;opened=1;
    memset(&in,0,sizeof(in));segread(&seg);
    in.h.ah=6;in.x.bx=driver_handle;in.x.cx=6;
    in.x.di=(unsigned)local_mac;seg.es=seg.ds;
    int86x(driver_int,&in,&out,&seg);
    if(out.x.cflag){net_close();strcpy(reason,"Cannot read network card address.");return 0;}
    sprintf(local_text,"%u.%u.%u.%u",(unsigned)local_ip[0],(unsigned)local_ip[1],(unsigned)local_ip[2],(unsigned)local_ip[3]);
    strcpy(peer_text,"none");
    return 1;
missing:
    strcpy(reason,"Packet driver not found at PACKETINT.");return 0;
}
const char *net_local_text(void){return local_text;}
const char *net_client_text(void){return peer_text;}
int net_lock_peer(void){
    int changed;
    if(!candidate_valid)return 0;
    changed=!peer_locked||memcmp(peer_ip,candidate_ip,4)||peer_port!=candidate_port;
    memcpy(peer_ip,candidate_ip,4);peer_port=candidate_port;peer_locked=1;
    sprintf(peer_text,"%u.%u.%u.%u",(unsigned)peer_ip[0],(unsigned)peer_ip[1],(unsigned)peer_ip[2],(unsigned)peer_ip[3]);
    candidate_valid=0;return changed;
}
void net_unlock_peer(void){peer_locked=0;candidate_valid=0;peer_port=0;peer_text[0]=0;}
void net_close(void){if(opened){driver_call(3,0,tx_frame);opened=0;}}
/* Returns one complete v6 frame. ARP is handled even while idle. */
int net_receive(U8 *out,unsigned capacity){
    unsigned slot,n,hlen,total,ulen,paylen,port,srcport,hello,match;
    U8 *p,*ip,*udp,*payload;
    for(slot=0;slot<NET_SLOTS;slot++)if(slot_state[slot]==2){
        _disable();slot_state[slot]=3;n=slot_length[slot];_enable();
        p=slot_frame[slot];
        if(n>=42&&p[12]==8&&p[13]==6&&
           p[14]==0&&p[15]==1&&p[16]==8&&p[17]==0&&p[18]==6&&p[19]==4&&
           p[20]==0&&p[21]==1&&!memcmp(p+38,local_ip,4)){
            arp_reply(p);
        }else if(n>=42&&p[12]==8&&p[13]==0){
            ip=p+14;hlen=(ip[0]&15)*4;
            if((ip[0]>>4)==4&&hlen>=20&&n>=14+hlen+8&&ip[9]==17&&
               !(ip[6]&0x3f)&&!ip[7]&&!checksum(ip,hlen)&&
               !memcmp(ip+16,local_ip,4)&&ip[12]!=0&&ip[12]<224&&memcmp(ip+12,local_ip,4)){
                total=((unsigned)ip[2]<<8)|ip[3];udp=ip+hlen;
                ulen=((unsigned)udp[4]<<8)|udp[5];
                port=((unsigned)udp[2]<<8)|udp[3];
                if(total>=hlen+8&&total<=n-14&&ulen>=8&&ulen<=total-hlen&&
                   port==NET_PORT&&(((unsigned)udp[0]<<8)|udp[1])!=0){
                    paylen=ulen-8;
                    payload=udp+8;srcport=((unsigned)udp[0]<<8)|udp[1];
                    hello=paylen>=8&&payload[0]=='N'&&payload[1]=='X'&&payload[2]==1;
                    match=peer_locked&&!memcmp(ip+12,peer_ip,4)&&srcport==peer_port;
                    if(paylen>=8&&payload[0]=='N'&&payload[1]=='X'&&
                       (hello|| (peer_locked&&match))&&paylen<=NET_DATA&&paylen-2<=capacity){
                        /* Drop the magic bytes here so NUXFER can validate the
                         * rest of the frame in place, without a full memmove. */
                        memcpy(out,payload+2,paylen-2);
                        memcpy(candidate_ip,ip+12,4);candidate_port=srcport;candidate_valid=1;
                        memcpy(answer_ip,ip+12,4);memcpy(answer_mac,p+6,6);
                        answer_port=srcport;
                        slot_state[slot]=0;return paylen-2;
                    }
                    bad++;
                }
            }
        }
        slot_state[slot]=0;
    }
    return 0;
}
int net_send(const U8 *data,unsigned n){
    U8 *frame=(U8 *)data-42,*ip=(U8 *)data-28,*udp=(U8 *)data-8;unsigned check;
    if(!answer_port||n>NET_DATA||n+42>NET_MTU)return 0;
    memset(frame,0,42);
    memcpy(frame,answer_mac,6);memcpy(frame+6,local_mac,6);
    frame[12]=8;frame[13]=0;
    ip[0]=0x45;ip[2]=(U8)((20+8+n)>>8);ip[3]=(U8)(20+8+n);
    ip[6]=0x40;ip[8]=64;ip[9]=17;
    memcpy(ip+12,local_ip,4);memcpy(ip+16,answer_ip,4);
    check=checksum(ip,20);ip[10]=(U8)(check>>8);ip[11]=(U8)check;
    udp[0]=(U8)(NET_PORT>>8);udp[1]=(U8)NET_PORT;
    udp[2]=(U8)(answer_port>>8);udp[3]=(U8)answer_port;
    udp[4]=(U8)((8+n)>>8);udp[5]=(U8)(8+n);
    if(n<18)memset((U8 *)data+n,0,18-n);
    return send_frame(frame,n+42);
}
void net_stats(void){printf("LAN: dropped %lu, invalid/oversize %lu\n",dropped,bad);}
