/* NuXT Link 3.1. Open Watcom, 8086 tiny model, DOS 3.31+.
 * The DOS side owns paths and validates every request; the Linux side owns
 * recursion, queues and manifests. No directory tree is held in DOS memory.
 */
#include <dos.h>
#include <i86.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <ctype.h>

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
#define CAP 512
#define PATHLEN 66
#define CACHE "\\NX$CACHE"
#define SIGN "NuXT Link 2 resume cache\r\n"
static U8 req[CAP+6], prev[CAP+4], reply[CAP+8], record[96];
static U8 block[4096];
static U16 crc_table[256];
static unsigned base, com=1, speed=3, fifo, saved[5];
static unsigned divisors[]={96,48,24,12,6,3,2,1};
static const char *bauds[]={"1200","2400","4800","9600","19200","38400","57600","115200"};
static unsigned quitting, changing, cached, active, last_tick;
static int handle=-1;
static char drive, path[PATHLEN], target[PATHLEN], cache[30], part[40], state[40], backup[40];
static char program[PATHLEN];
static struct find_t findbuf, original;
static U32 position, total, scan_to;
static U16 file_crc, expected_crc;
static U8 meta[9];
static unsigned overwrite, old_attr, list_active;
static unsigned flow, peer, chunksize=128, rx_fault, readpos, readlen;
static U32 counters[6]; /* CRC, overrun, parity, framing, break, CTS timeout. */
static unsigned pending, switch_tick, old_speed, old_flow, old_peer, old_chunk;
static unsigned new_speed, new_flow, new_peer, new_chunk;
static U32 switch_token;
static void settings_poll(void);
static void uart_speed(void);
static void show_settings(void);
extern U32 uart_rx(unsigned port,U8 *dest,unsigned count);
#pragma aux uart_rx parm [dx] [di] [cx] value [dx ax] modify [ax cx dx si di];
static void rts(unsigned on){outp(base+4,1|((!flow||on)?2:0));}
static void line_errors(unsigned s){
    if(s&2)counters[1]++;
    if(s&4)counters[2]++;
    if(s&8)counters[3]++;
    if(s&16)counters[4]++;
    rx_fault=1;
}
static unsigned status(void){
    unsigned s=inp(base+5);
    if(s&0x1e)line_errors(s);
    return s;
}
static void (__interrupt __far *old24)(void);
extern void critical(void);
static unsigned code_segment(void);
#pragma aux code_segment = "mov ax,cs" value [ax];
static U16 word(const U8 *p){return p[0]|((U16)p[1]<<8);}
static U32 dword(const U8 *p){return word(p)|((U32)word(p+2)<<16);}
static void pw(U8 *p,U16 x){p[0]=(U8)x;p[1]=(U8)(x>>8);}
static void pd(U8 *p,U32 x){pw(p,(U16)x);pw(p+2,(U16)(x>>16));}
static U16 crc(const U8 *p,unsigned n,U16 c){while(n--)c=(c<<8)^crc_table[(c>>8)^*p++];return c;}
static unsigned ticks(void){return *(volatile U16 __far *)MK_FP(0x40,0x6c);}
static void keys(void){
    if(*(U16 __far *)MK_FP(0x40,0x1a)!=*(U16 __far *)MK_FP(0x40,0x1c)){
        union REGS r;r.h.ah=0;int86(0x16,&r,&r);
        if(r.h.al==27)quitting=1;
        if(!active&&!pending){if(r.h.al=='b'||r.h.al=='B')changing=1;
        if(r.h.al=='f'||r.h.al=='F')changing=2;
        if(r.h.al=='d'||r.h.al=='D')changing=3;
        if(r.h.al=='c'||r.h.al=='C')changing=4;
        if(r.h.al=='s'||r.h.al=='S')changing=5;}
    }
}
static int receive(void){
    unsigned start=ticks(),s;
    do{
        settings_poll();s=status();
        if(rx_fault){if(s&1)inp(base);rx_fault=0;return -1;}
        if(s&1)return inp(base);
        keys();if(quitting||changing)return -1;
    }while((unsigned)(ticks()-start)<18);
    return -1;
}
static int transmit(U8 b){
    unsigned start=ticks(),mask=peer|(flow?0x10:0);
    do{
        if((status()&0x20)&&((inp(base+6)&mask)==mask)){outp(base,b);return 1;}
        keys();if(quitting)return 0;
    }while((unsigned)(ticks()-start)<36);
    counters[5]++;return 0;
}
static int drain(void){
    unsigned start=ticks();
    while(!(status()&0x40)){keys();if(quitting||(unsigned)(ticks()-start)>36)return 0;}
    return 1;
}
static void uart_speed(void){
    outp(base+1,0);outp(base+3,0x83);outp(base,divisors[speed]);outp(base+1,0);
    outp(base+3,3);rts(0);outp(base+2,fifo?7:0);rx_fault=0;
}
static void uart_open(void){
    saved[0]=inp(base+3);outp(base+3,saved[0]&127);
    saved[1]=inp(base+1);outp(base+1,0);saved[2]=inp(base+4);
    outp(base+3,0x83);saved[3]=inp(base);saved[4]=inp(base+1);
    outp(base+3,3);outp(base+2,7);fifo=(inp(base+2)&0xc0)==0xc0;uart_speed();
}
static void uart_close(void){
    outp(base+3,0x83);outp(base,saved[3]);outp(base+1,saved[4]);outp(base+3,3);
    outp(base+2,0);outp(base+4,saved[2]);outp(base+1,saved[1]);outp(base+3,saved[0]);
}
/* Capture an entire header/body without per-byte C calls or CRC work.
 * DX reports a UART fault or an inter-byte timeout; partial frames never dispatch.
 * RTS remains low on failure and while the completed frame is checked.
 */
static int receive_exact(U8 *dest,unsigned n){
    U32 result=uart_rx(base,dest,n);unsigned fault=(unsigned)(result>>16);
    if(!fault&&(unsigned)result==n&&!rx_fault)return 1;
    rts(0);
    if(fault&0x1e)line_errors(fault);
    if(fifo)outp(base+2,3);else if(inp(base+5)&1)inp(base);
    rx_fault=0;return 0;
}
static int frame(void){
    int b;unsigned n;U16 check;
    if(receive()!='N')return 0;
    do{b=receive();}while(b=='N');if(b!='X')return 0;
    if(!receive_exact(req,4))return 0;
    n=word(req+2);if(n>CAP)return 0;
    if(!receive_exact(req+4,n+2))return 0;
    rts(0);check=word(req+4+n);
    if(crc(req,n+4,0xffff)!=check){counters[0]++;return 0;}return 1;
}
static int send(void){
    unsigned i,n=word(reply+4)+6;pw(reply+n,crc(reply+2,n-2,0xffff));
    for(i=0;i<n+2;i++)if(!transmit(reply[i]))return 0;return drain();
}
static void error(const char *s){reply[2]=255;pw(reply+4,strlen(s));strcpy((char *)reply+6,s);}
static void ioerror(unsigned e){char s[100];sprintf(s,"DOS error %u (path, disk full, protection or I/O failure)",e);error(s);}
static void pause_file(void){if(handle>=0){_dos_close(handle);handle=-1;}active=0;list_active=0;readpos=readlen=0;}
static int name_ok(const char *s){
    unsigned n=0,e=0,dot=0;char stem[9];U8 ch;
    while((ch=(U8)*s++)!=0){
        if(ch=='.'){if(dot||!n)return 0;dot=1;continue;}
        if(ch<33||strchr("\"*/:<>?\\|+,;=[]",ch))return 0;
        if(dot){if(++e>3)return 0;}else{if(n==8)return 0;stem[n++]=(char)toupper(ch);}
    }
    if(!n||(dot&&!e))return 0;stem[n]=0;
    if(!strcmp(stem,"CON")||!strcmp(stem,"PRN")||!strcmp(stem,"AUX")||!strcmp(stem,"NUL")||!strcmp(stem,"CLOCK$"))return 0;
    if(n==4&&(!memcmp(stem,"COM",3)||!memcmp(stem,"LPT",3))&&stem[3]>='0'&&stem[3]<='9')return 0;
    return 1;
}
/* Wire paths begin at this server's root, with no drive letter or dot segments. */
static int parse_path(unsigned off,unsigned n,int root){
    unsigned i,start;char component[13];const char *s=(const char *)req+4+off;
    if(n<=off||n-off>64||s[n-off-1]||strlen(s)!=n-off-1||s[0]!='\\')goto bad;
    if(!s[1]){if(!root)goto bad;}
    else for(start=1;s[start];){
        i=0;while(s[start]&&s[start]!='\\'){if(i==12)goto bad;component[i++]=s[start++];}
        component[i]=0;if(!name_ok(component))goto bad;
        if(s[start]=='\\'&&!s[++start])goto bad;
    }
    path[0]=drive;path[1]=':';strcpy(path+2,s);return 1;
bad:error("Invalid path: use root-relative DOS 8.3 names on the shared drive");return 0;
}
static int stat_path(const char *p,struct find_t *f){unsigned e=_dos_findfirst(p,0x37,f);return e==18?2:e;}
static void metadata(U8 *out,const struct find_t *f){pd(out,f->size);pw(out+4,f->wr_date);pw(out+6,f->wr_time);out[8]=f->attrib&0x37;}
static int cache_owned(void){
    int h;unsigned n;char p[40],s[sizeof(SIGN)];
    sprintf(p,"%c:%s\\OWNER.ID",drive,CACHE);
    if(_dos_open(p,O_RDONLY,&h))return 0;
    n=0;_dos_read(h,s,sizeof(s),&n);_dos_close(h);
    return n==sizeof(SIGN)-1&&!memcmp(s,SIGN,n);
}
static int is_cache(const char *p){return !strnicmp(p+2,CACHE,9)&&(p[11]==0||p[11]=='\\')&&cache_owned();}
static int make_cache(void){
    unsigned a,n,e;int h;char p[40];
    sprintf(cache,"%c:%s",drive,CACHE);
    if(!_dos_getfileattr(cache,&a)){if(cache_owned())return 1;error("NX$CACHE already exists without a NuXT Link ownership marker");return 0;}
    if(mkdir(cache)){error("Cannot create resume cache (disk full or write protected)");return 0;}
    sprintf(p,"%s\\OWNER.ID",cache);e=_dos_creatnew(p,2,&h);
    if(!e){e=_dos_write(h,SIGN,sizeof(SIGN)-1,&n);_dos_close(h);if(n!=sizeof(SIGN)-1)e=5;}
    if(e){ioerror(e);return 0;}_dos_setfileattr(cache,2);return 1;
}
static void slot_paths(unsigned id,unsigned attempt){
    sprintf(state,"%s\\%04X%02X.RSM",cache,id,attempt);
    sprintf(part,"%s\\%04X%02X.PRT",cache,id,attempt);
    sprintf(backup,"%s\\%04X%02X.BAK",cache,id,attempt);
}
static int read_record(void){
    int h;unsigned n=0,e;if(_dos_open(state,O_RDONLY,&h))return 0;
    e=_dos_read(h,record,sizeof(record),&n);_dos_close(h);
    return !e&&n==96&&!memcmp(record,"NXL2RSM!",8)&&crc(record,94,0xffff)==word(record+94)&&memchr(record+20,0,66);
}
static int save_record(void){
    int h;unsigned n=0,e;pw(record+94,crc(record,94,0xffff));
    e=_dos_creat(state,2,&h);if(e)return e;
    e=_dos_write(h,record,96,&n);if(_dos_close(h)&&!e)e=5;return e?e:(n==96?0:5);
}
static void remove_owned(const char *p){_dos_setfileattr(p,0);remove(p);}
static int open_regular(const char *p,int *h){
    union REGS r;unsigned e=_dos_open(p,O_RDONLY,h);if(e)return e;
    r.x.ax=0x4400;r.x.bx=*h;intdos(&r,&r);
    if(r.x.cflag||(r.x.dx&0x80)){_dos_close(*h);*h=-1;return 5;}return 0;
}
/* Read back through DOS after setting metadata: success means exact raw DOS
 * date/time and R/H/S/A attributes, including a cleared archive bit. */
extern int directory_time(const char *path,unsigned date,unsigned time);
static int set_metadata(const char *p,const U8 *m){
    struct find_t f;int h;unsigned e,at=m[8]&0x27;
    e=stat_path(p,&f);if(e)return e;
    if((f.attrib&16)!=(m[8]&16))return 5;
    if(f.attrib&16){
        if(f.wr_date!=word(m+4)||f.wr_time!=word(m+6)){
            e=directory_time(p,word(m+4),word(m+6));if(e)return e;
        }
    }else{
        e=_dos_setfileattr(p,0);if(e)return e;
        e=_dos_open(p,O_RDWR,&h);
        if(!e){e=_dos_setftime(h,word(m+4),word(m+6));if(_dos_close(h)&&!e)e=5;}
        if(e){_dos_setfileattr(p,f.attrib&0x27);return e;}
    }
    e=_dos_setfileattr(p,at);if(e)return e;
    e=stat_path(p,&f);if(e)return e;
    return f.wr_date==word(m+4)&&f.wr_time==word(m+6)&&(f.attrib&0x37)==m[8]?0:5;
}
static void scan_reply(int upload){
    unsigned got,n,e;U32 left=scan_to-position;
    n=left>sizeof(block)?sizeof(block):(unsigned)left;
    if(n){e=_dos_read(handle,block,n,&got);if(e||got!=n){ioerror(e?e:5);pause_file();return;}
        file_crc=crc(block,n,file_crc);position+=n;}
    if(position==scan_to){
        if(active==3){_dos_close(handle);handle=-1;active=0;}
        else active=upload?1:2;
    }
    pd(reply+6,position);pd(reply+10,scan_to);pw(reply+14,file_crc);reply[16]=(position==scan_to);pw(reply+4,11);
}
static void put_begin(unsigned n){
    unsigned i,e,a,found=0;int h;U8 wanted[96];struct find_t f;
    if(n<14){error("Invalid upload request");return;}
    if(!parse_path(12,n,0))return;
    if(req[12]&~0x27||req[15]&~1||dword(req+4)>0x7fffffffUL){error("Invalid upload metadata");return;}
    if(is_cache(path)){error("Resume cache is managed by NuXT Link");return;}
    if(!make_cache())return;
    pause_file();strcpy(target,path);memcpy(meta,req+4,9);expected_crc=word(req+13);overwrite=req[15];total=dword(meta);
    memset(wanted,0,96);memcpy(wanted,"NXL2RSM!",8);memcpy(wanted+8,meta,9);pw(wanted+17,expected_crc);wanted[19]=(U8)overwrite;strcpy((char *)wanted+20,target);
    for(i=0;i<256;i++){
        slot_paths(crc((const U8 *)target,strlen(target),0xffff),i);
        if(read_record()){
            if(!memcmp(record,wanted,86)){found=1;break;}
        }else if(_dos_getfileattr(state,&a)==2&&_dos_getfileattr(part,&a)==2&&_dos_getfileattr(backup,&a)==2){
            memcpy(record,wanted,96);break;
        }
    }
    if(i==256){error("Too many retained partial files for this path");return;}
    /* A stopped commit leaves the original in .BAK. Never erase it blindly. */
    if(!_dos_getfileattr(backup,&a)){
        if(_dos_getfileattr(target,&a)==2){
            if(rename(backup,target)){error("Cannot recover original from resume cache .BAK");return;}
            _dos_setfileattr(target,record[86]);record[87]=0;e=save_record();if(e){ioerror(e);return;}
        }else {error("Interrupted commit: original .BAK retained in NX$CACHE; inspect before retrying");return;}
    }
    e=stat_path(target,&f);
    if(!e&&((f.attrib&16)||!overwrite)){error("Destination exists; choose overwrite, skip or rename in the preview");return;}
    if(e&&e!=2&&e!=3){ioerror(e);return;}
    old_attr=e?0:f.attrib;
    if(!found){
        e=_dos_creatnew(part,2,&h);if(e){ioerror(e);return;}_dos_close(h);
        e=save_record();if(e){remove_owned(part);ioerror(e);return;}
    }
    e=_dos_open(part,O_RDWR,&handle);if(e){ioerror(e);return;}
    scan_to=(U32)lseek(handle,0,SEEK_END);lseek(handle,0,SEEK_SET);
    if(scan_to>total){error("Retained partial is longer than source; discard partials before retrying");pause_file();return;}
    position=0;file_crc=0xffff;active=4;
    pd(reply+6,scan_to);pw(reply+4,4);
}
static void put_end(unsigned n){
    unsigned e,a;U16 c;int had_old=0;
    if(active!=1||n!=2){error("No upload is ready");return;}
    c=word(req+4);
    if(position!=total||c!=file_crc||c!=expected_crc){error("Upload size or whole-file CRC mismatch; partial retained");pause_file();return;}
    e=_dos_setftime(handle,word(meta+4),word(meta+6));if(_dos_close(handle)&&!e)e=5;handle=-1;active=0;
    if(e){ioerror(e);return;}
    if(!_dos_getfileattr(target,&a)){
        if(!overwrite||a&16){error("Destination appeared during upload; partial retained");return;}
        old_attr=a;record[86]=(U8)a;record[87]=1;e=save_record();if(e){ioerror(e);return;}
        e=_dos_setfileattr(target,0);if(e){ioerror(e);return;}
        if(rename(target,backup)){_dos_setfileattr(target,a);error("Cannot stage original destination; partial retained");return;}
        had_old=1;
    }
    if(rename(part,target)){
        if(had_old){rename(backup,target);_dos_setfileattr(target,old_attr);}
        error("Cannot commit upload; original and partial retained");return;
    }
    e=set_metadata(target,meta);
    if(e){
        _dos_setfileattr(target,0);rename(target,part);
        if(had_old){rename(backup,target);_dos_setfileattr(target,old_attr);}
        ioerror(e);return;
    }
    if(had_old)remove_owned(backup);remove_owned(state);
    pw(reply+6,c);pw(reply+4,2);
}
/* Discard only staging files with a valid ownership record. Interrupted
 * replacement backups are always retained for inspection/recovery. */
static void partials(unsigned discard){
    struct find_t item;unsigned e,a,ready=0,kept=0;char pattern[40];
    pause_file();
    if(!cache_owned()){pw(reply+6,0);pw(reply+8,0);pw(reply+4,4);return;}
    sprintf(cache,"%c:%s",drive,CACHE);sprintf(pattern,"%s\\*.RSM",cache);
    e=_dos_findfirst(pattern,0x27,&item);
    while(!e){
        sprintf(state,"%s\\%s",cache,item.name);
        strcpy(part,state);strcpy(part+strlen(part)-3,"PRT");
        strcpy(backup,state);strcpy(backup+strlen(backup)-3,"BAK");
        if(!read_record()||!_dos_getfileattr(backup,&a))kept++;
        else if(discard){
            _dos_setfileattr(part,0);
            if(remove(part)&&_dos_getfileattr(part,&a)!=2)kept++;
            else{_dos_setfileattr(state,0);if(remove(state))kept++;else ready++;}
        }else ready++;
        e=_dos_findnext(&item);
    }
    if(e!=18&&e!=2){ioerror(e);return;}
    pw(reply+6,ready);pw(reply+8,kept);pw(reply+4,4);
}
static void show_settings(void){
    unsigned m=inp(base+6);
    printf("COM%u %s baud / %s / peer %02X / block %u / FIFO %s\n",
        com,bauds[speed],flow?"RTS/CTS":"3-wire",peer,chunksize,fifo?"yes":"no");
    printf("CTS %u DSR %u DCD %u | CRC %lu UART %lu/%lu/%lu/%lu waits %lu\n",
        !!(m&16),!!(m&32),!!(m&128),counters[0],counters[1],counters[2],counters[3],counters[4],counters[5]);
}
static void settings_poll(void){
    if(pending==1&&(unsigned)(ticks()-switch_tick)>=3){
        speed=new_speed;flow=new_flow;peer=new_peer;chunksize=new_chunk;
        uart_speed();cached=0;pending=2;switch_tick=ticks();rts(1);
    }else if(pending==2&&(unsigned)(ticks()-switch_tick)>=218){
        speed=old_speed;flow=old_flow;peer=old_peer;chunksize=old_chunk;
        uart_speed();cached=0;pending=0;puts("Unconfirmed settings rolled back.");show_settings();rts(1);
    }
}
static void info_reply(void){
    union REGS r;unsigned i;U32 unit,freebytes=0,totalbytes=0;
    r.h.ah=0x36;r.h.dl=drive-'A'+1;intdos(&r,&r);
    if(r.x.ax!=0xffff){unit=(U32)r.x.ax*r.x.cx;freebytes=unit*r.x.bx;totalbytes=unit*r.x.dx;}
    reply[6]=3;reply[7]=(U8)com;reply[8]=(U8)speed;reply[9]=(U8)flow;
    reply[10]=(U8)peer;reply[11]=(U8)fifo;reply[12]=(U8)inp(base+6);reply[13]=(U8)inp(base+4);
    for(i=0;i<6;i++)pd(reply+14+i*4,counters[i]);
    pd(reply+38,freebytes);pd(reply+42,totalbytes);pw(reply+46,CAP);pw(reply+48,chunksize);
    reply[50]=(U8)pending;strcpy((char *)reply+51,program+2);pw(reply+4,46+strlen(program+2));
}
static int protected_path(const char *p){
    unsigned n=strlen(p);
    if(n<=3)return 1;
    if(!strnicmp(p+2,CACHE,9)&&(p[11]==0||p[11]=='\\'))return 1;
    /* Keep the running server and any containing directory available. */
    if(!strnicmp(p,program,n)&&(program[n]==0||program[n]=='\\'))return 1;
    return 0;
}
static int expected(const struct find_t *f,const U8 *m){
    U8 actual[9];metadata(actual,f);
    if(f->attrib&16)return (f->attrib&0x37)==m[8];
    return !memcmp(actual,m,9);
}
static void delete_entry(unsigned n){
    struct find_t f;unsigned e,attrs;U8 *p=req+4;
    if(active||pending){error("Finish or cancel the active operation first");return;}
    if(n<12||p[0]>1){error("Invalid delete request");return;}
    if(!parse_path(10,n,0))return;
    if(protected_path(path)){error("Cannot delete root, running server, or resume cache");return;}
    e=stat_path(path,&f);if(e){ioerror(e);return;}
    if(!expected(&f,p+1)){error("Entry changed since deletion preview; refresh it");return;}
    attrs=f.attrib&0x27;
    if((attrs&7)&&!p[0]){error("Read-only/hidden/system deletion requires explicit confirmation");return;}
    if(attrs&7){e=_dos_setfileattr(path,attrs&~7);if(e){ioerror(e);return;}}
    if((f.attrib&16)?rmdir(path):remove(path)){
        _dos_setfileattr(path,attrs);error("Delete failed: directory not empty, protected or I/O error");return;
    }
    list_active=0;
}
static void rename_entry(unsigned n){
    char from[PATHLEN];unsigned first,e;struct find_t f,d;U8 *p=req+4;
    if(active||pending){error("Finish or cancel the active operation first");return;}
    if(n<13){error("Invalid rename request");return;}
    for(first=9;first<n&&p[first];first++);if(first>=n){error("Invalid rename paths");return;}
    if(!parse_path(9,first+1,0))return;strcpy(from,path);
    if(!parse_path(first+1,n,0))return;
    if(protected_path(from)||protected_path(path)){error("Cannot rename root, running server, or resume cache");return;}
    e=stat_path(from,&f);if(e){ioerror(e);return;}
    if(!expected(&f,p)){error("Entry changed since rename was requested");return;}
    if(!stat_path(path,&d)){error("Rename destination already exists");return;}
    if((f.attrib&16)&&!strnicmp(from,path,strlen(from))&&path[strlen(from)]=='\\'){
        error("Cannot move a directory inside itself");return;
    }
    if(rename(from,path)){error("Rename failed: parent missing, protection or I/O error");return;}
    e=stat_path(path,&d);if(e){ioerror(e);return;}
    if(!expected(&d,p)){error("Rename completed but metadata verification failed");return;}
    list_active=0;
}
static void dispatch(void){
    unsigned n=word(req+2),e,got;U8 *p=req+4;struct find_t f;char pattern[72];
    if(pending&&req[0]!=0x40&&req[0]!=0x42){error("Connection settings await confirmation");return;}
    switch(req[0]){
    case 0x40:if(n){error("Invalid diagnostics request");break;}info_reply();break;
    case 0x41:
        if(n!=9||p[0]>7||p[1]>1||(p[2]&~0xa0)||(word(p+3)!=128&&word(p+3)!=256&&word(p+3)!=512)){
            error("Invalid connection settings");break;
        }
        if(active||pending){error("Settings can change only while idle");break;}
        if((inp(base+6)&p[2])!=p[2]||(p[1]&&!(inp(base+6)&16))){error("Required CTS/DSR/DCD line is low; check cable and peer");break;}
        old_speed=speed;old_flow=flow;old_peer=peer;old_chunk=chunksize;
        new_speed=p[0];new_flow=p[1];new_peer=p[2];new_chunk=word(p+3);switch_token=dword(p+5);
        memcpy(reply+6,p,9);pw(reply+4,9);pending=3;break;
    case 0x42:
        if(n!=4||pending!=2||dword(p)!=switch_token){error("No matching settings change");break;}
        /* Finish console output before ACK: a 3-wire peer may send the next
         * packet immediately. Never print after saying the server is ready. */
        show_settings();puts("Connection settings confirmed.");
        info_reply();pending=4;break;
    case 0x50:delete_entry(n);break;
    case 0x51:rename_entry(n);break;
    case 1:
        pause_file();sprintf((char *)reply+6,"NUXTLINK/3 %c:\\ file server",drive);pw(reply+4,strlen((char *)reply+6));break;
    case 0x30:pause_file();break;
    case 0x31:
    case 0x32:
        if(n){error("Invalid partial-cache command");break;}partials(req[0]==0x31);break;
    case 2:
        if(!parse_path(0,n,1))break;pause_file();
        if(strlen(path)>3){e=stat_path(path,&f);if(e||!(f.attrib&16)){ioerror(e?e:3);break;}}
        strcpy(pattern,path);if(pattern[strlen(pattern)-1]!='\\')strcat(pattern,"\\");strcat(pattern,"*.*");
        e=_dos_findfirst(pattern,0x37,&findbuf);list_active=1;goto list_result;
    case 3:
        if(n||!list_active){error("No directory listing is active");break;}
        e=_dos_findnext(&findbuf);
list_result:
        while(!e&&((findbuf.attrib&8)||!strcmp(findbuf.name,".")||!strcmp(findbuf.name,"..")))e=_dos_findnext(&findbuf);
        if(e){list_active=0;if(e!=18&&e!=2)ioerror(e);break;}
        metadata(reply+6,&findbuf);
        /* A private cache is visible but marked, so backups report its omission. */
        if(!stricmp(findbuf.name,"NX$CACHE")&&cache_owned()&&!strcmp(path+2,"\\"))reply[14]|=0x80;
        strcpy((char *)reply+15,findbuf.name);pw(reply+4,10+strlen(findbuf.name));break;
    case 4:
        if(!parse_path(0,n,0))break;e=stat_path(path,&f);
        if(e)ioerror(e);else{metadata(reply+6,&f);pw(reply+4,9);}break;
    case 5:
        if(!parse_path(0,n,0))break;
        if(is_cache(path)){error("Resume cache is managed by NuXT Link");break;}
        if(!stat_path(path,&f)){if(!(f.attrib&16))error("A file occupies the directory path");}
        else if(mkdir(path))error("Cannot create directory (parent missing, protection or disk full)");break;
    case 6:
        if(n<11){error("Invalid metadata request");break;}
        if(!parse_path(9,n,0))break;
        if(p[8]&~0x37||is_cache(path)){error("Invalid attributes or private cache path");break;}
        e=set_metadata(path,p);if(e)ioerror(e);break;
    case 7:
    case 0x20:
        if(!parse_path(req[0]==7?0:4,n,0))break;pause_file();
        e=stat_path(path,&original);if(e||original.attrib&16){ioerror(e?e:5);break;}
        e=open_regular(path,&handle);if(e){ioerror(e);break;}
        strcpy(target,path);total=original.size;
        scan_to=req[0]==7?total:dword(p);
        if(total>0x7fffffffUL||scan_to>total){error("Invalid file length or resume offset");pause_file();break;}
        position=0;file_crc=0xffff;active=req[0]==7?3:5;
        metadata(reply+6,&original);pw(reply+4,9);break;
    case 8:
        if(n||active!=3){error("No verification scan is active");break;}scan_reply(0);break;
    case 0x24:
        if(n||active!=5){error("No download resume scan is active");break;}scan_reply(0);break;
    case 0x13:
        if(n||active!=4){error("No upload resume scan is active");break;}scan_reply(1);break;
    case 0x10:put_begin(n);break;
    case 0x11:
        if(active!=1||!n||position+n>total){error("Invalid upload block or no upload active");break;}
        e=_dos_write(handle,p,n,&got);if(e||got!=n){ioerror(e?e:112);pause_file();break;}
        file_crc=crc(p,n,file_crc);position+=n;break;
    case 0x12:put_end(n);break;
    case 0x21:
        if(n||active!=2){error("No download is ready");break;}
        got=0;n=total-position>chunksize?chunksize:(unsigned)(total-position);
        if(readpos==readlen){
            readpos=0;readlen=total-position>sizeof(block)?sizeof(block):(unsigned)(total-position);
            e=_dos_read(handle,block,readlen,&got);
            if(e||got!=readlen){ioerror(e?e:5);pause_file();break;}
        }
        if(n>readlen-readpos)n=readlen-readpos;
        memcpy(reply+6,block+readpos,n);readpos+=n;
        file_crc=crc(reply+6,n,file_crc);position+=n;pw(reply+4,n);break;
    case 0x22:
        if(n||active!=2||position!=total){error("Download is incomplete");break;}
        pause_file();e=stat_path(target,&f);
        if(e||f.size!=original.size||f.wr_date!=original.wr_date||f.wr_time!=original.wr_time||f.attrib!=original.attrib){error("Source metadata changed during download");break;}
        pw(reply+6,file_crc);pw(reply+4,2);break;
    default:error("Unknown command");
    }
}
static int executable_drive(void){
    const char __far *env;unsigned seg,off=0,count;
    seg=*(U16 __far *)MK_FP(_psp,0x2c);if(!seg)return 0;env=MK_FP(seg,0);
    while(off<32760&&(env[off]||env[off+1]))off++;
    if(off>=32760)return 0;off+=2;count=(U8)env[off]|((U16)(U8)env[off+1]<<8);off+=2;
    if(!count||env[off+1]!=':'||env[off+2]!='\\')return 0;
    drive=(char)toupper(env[off]);if(drive<'A'||drive>'Z')return 0;
    count=0;while(env[off]&&count<PATHLEN-1)program[count++]=env[off++];program[count]=0;
    return !env[off];
}
int main(int argc,char **argv){
    unsigned i,j,x;union REGS r;
    for(i=0;i<256;i++){x=i<<8;for(j=0;j<8;j++)x=(x<<1)^((x&0x8000)?0x1021:0);crc_table[i]=x;}
    for(i=1;i<(unsigned)argc;i++){
        if(strlen(argv[i])==4&&!strnicmp(argv[i],"COM",3)&&argv[i][3]>='1'&&argv[i][3]<='4')com=argv[i][3]-'0';
        else if(!strnicmp(argv[i],"/FLOW:",6)){
            if(!stricmp(argv[i]+6,"NONE"))flow=0;else if(!stricmp(argv[i]+6,"RTSCTS"))flow=1;else goto usage;
        }else if(!strnicmp(argv[i],"/PEER:",6)){
            if(!stricmp(argv[i]+6,"NONE"))peer=0;else if(!stricmp(argv[i]+6,"DSR"))peer=32;
            else if(!stricmp(argv[i]+6,"DCD"))peer=128;else if(!stricmp(argv[i]+6,"BOTH"))peer=160;else goto usage;
        }else if(!strnicmp(argv[i],"/BLOCK:",7)){
            if(!strcmp(argv[i]+7,"128"))chunksize=128;else if(!strcmp(argv[i]+7,"256"))chunksize=256;
            else if(!strcmp(argv[i]+7,"512"))chunksize=512;else goto usage;
        }else {for(j=0;j<8;j++)if(!strcmp(argv[i],bauds[j]))break;if(j==8)goto usage;speed=j;}
    }
    r.h.ah=0x30;intdos(&r,&r);if(r.h.al<3||(r.h.al==3&&r.h.ah<31)){puts("DOS 3.31 or later required.");return 1;}
    if(!executable_drive()){puts("Cannot find executable's drive in DOS environment. Launch from COMMAND.COM.");return 1;}
    base=*(U16 __far *)MK_FP(0x40,(com-1)*2);if(!base){puts("COM port is not present in the BIOS table.");return 1;}
    old24=_dos_getvect(0x24);_dos_setvect(0x24, (void (__interrupt __far *)(void))MK_FP(code_segment(),(unsigned)critical));
    uart_open();reply[0]='N';reply[1]='X';
    printf("\nNUXT LINK 3.2 - entire %c:\\ drive\n",drive);
    show_settings();
    puts("Files, folders, hidden/system files and DOS metadata.\nEsc exits. Idle keys: B baud, F flow, D peer lines, C COM, S status.\nPartials survive disconnects. Defaults: COM1 9600 /FLOW:NONE.");
    last_tick=ticks();
    while(!quitting){
        rts(0);keys();
        settings_poll();
        if(changing){
            if(!active&&!pending){
                if(changing==1)speed=(speed+1)%8;
                if(changing==2)flow=!flow;
                if(changing==3)peer=peer?0:0xa0;
                if(changing==4){
                    for(j=1;j<=4;j++){x=(com-1+j)%4+1;if(*(U16 __far *)MK_FP(0x40,(x-1)*2))break;}
                    uart_close();com=x;base=*(U16 __far *)MK_FP(0x40,(com-1)*2);uart_open();
                }
                if(changing!=5){uart_speed();cached=0;}show_settings();
            }
            changing=0;
        }
        if(active&&(unsigned)(ticks()-last_tick)>546){pause_file();cached=0;}
        rts(1);if(!frame())continue;rts(0);last_tick=ticks();
        if(!cached||memcmp(req,prev,word(req+2)+4)){
            reply[2]=128;reply[3]=req[1];pw(reply+4,0);
            dispatch();memcpy(prev,req,word(req+2)+4);cached=1;
        }
        if(send()){
            if(pending==3){pending=1;switch_tick=ticks();}
            else if(pending==4)pending=0;
        }else if(pending==3){pending=0;cached=0;}else if(pending==4){pending=2;cached=0;}
        rts(1);
    }
    pause_file();uart_close();_dos_setvect(0x24,old24);puts("\nStopped. UART restored; resumable partials retained.");return 0;
usage:
    puts("NUXFER [COM1..COM4] [1200..115200] [/FLOW:NONE|RTSCTS]\n"
         "       [/PEER:NONE|DSR|DCD|BOTH] [/BLOCK:128|256|512]\n"
         "Defaults: COM1 9600 /FLOW:NONE /PEER:NONE /BLOCK:128");return 1;
}
