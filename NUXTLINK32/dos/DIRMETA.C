/* Directory dates have no DOS 3.x/6.x handle API. For local FAT12/FAT16
 * volumes ONLY, update the four date/time bytes of the named directory entry.
 * All data allocation, directory creation and attribute operations use DOS.
 * This code never writes the boot sector, FAT, file contents or size fields.
 * Disk reset flushes DOS buffers before and after the bounded metadata write.
 */
#include <dos.h>
#include <i86.h>
#include <string.h>
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
static U8 sector_buf[512], before[512];
static unsigned drive_no, extended, spc, fat12;
static U32 sectors, fat_start, root_start, data_start, clusters;
static unsigned root_sectors;
#pragma pack(push,1)
static struct {U32 sector;U16 count;void __far *buffer;} packet;
#pragma pack(pop)
static unsigned read_abs(unsigned,unsigned,unsigned,void *);
#pragma aux read_abs = "int 25h" "pop bx" "sbb dx,dx" "and ax,dx" parm [ax] [cx] [dx] [bx] value [ax] modify [bx dx];
static unsigned write_abs(unsigned,unsigned,unsigned,void *);
#pragma aux write_abs = "int 26h" "pop bx" "sbb dx,dx" "and ax,dx" parm [ax] [cx] [dx] [bx] value [ax] modify [bx dx];
static U16 word(const U8 *p){return p[0]|((U16)p[1]<<8);}
static U32 dword(const U8 *p){return word(p)|((U32)word(p+2)<<16);}
static void pw(U8 *p,unsigned n){p[0]=(U8)n;p[1]=(U8)(n>>8);}
static void flush(void){union REGS r;r.h.ah=0x0d;intdos(&r,&r);}
static int read_sector(U32 s){
    if(s>=sectors)return 5;
    if(extended){packet.sector=s;packet.count=1;packet.buffer=sector_buf;return read_abs(drive_no,0xffff,0,&packet);}
    if(s>65535UL)return 5;return read_abs(drive_no,1,(unsigned)s,sector_buf);
}
static int write_sector(U32 s){
    if(s<root_start||s>=sectors)return 5;
    if(extended){packet.sector=s;packet.count=1;packet.buffer=sector_buf;return write_abs(drive_no,0xffff,0,&packet);}
    if(s>65535UL)return 5;return write_abs(drive_no,1,(unsigned)s,sector_buf);
}
static int geometry(void){
    unsigned reserved,fats,fat_size,roots;U32 count;
    sectors=1;extended=0;if(read_sector(0)){extended=1;if(read_sector(0))return 5;}
    if(word(sector_buf+11)!=512||word(sector_buf+510)!=0xaa55)return 5;
    spc=sector_buf[13];reserved=word(sector_buf+14);fats=sector_buf[16];roots=word(sector_buf+17);fat_size=word(sector_buf+22);
    sectors=word(sector_buf+19);if(!sectors)sectors=dword(sector_buf+32);
    if(!spc||(spc&(spc-1))||spc>128||!reserved||!fats||fats>2||!roots||!fat_size)return 5;
    root_sectors=(roots+15)/16;fat_start=reserved;root_start=fat_start+(U32)fats*fat_size;data_start=root_start+root_sectors;
    if(data_start>=sectors)return 5;
    clusters=(sectors-data_start)/spc;fat12=clusters<4085;
    if(!clusters||clusters>=65525UL)return 5;
    count=clusters+2;if((fat12?(count*3+1)/2:count*2)>(U32)fat_size*512)return 5;
    extended=sectors>65535UL;return 0;
}
static U32 next_cluster(unsigned c){
    U32 offset=fat12?(U32)c+c/2:(U32)c*2;unsigned value,lo;
    if(read_sector(fat_start+offset/512))return 0;
    lo=sector_buf[(unsigned)(offset%512)];
    if(offset%512==511){if(read_sector(fat_start+offset/512+1))return 0;value=lo|((unsigned)sector_buf[0]<<8);}
    else value=word(sector_buf+(unsigned)(offset%512));
    if(fat12)value=(c&1)?value>>4:value&0xfff;
    if(value>=(fat12?0xff8:0xfff8))return 0;
    return value>=2&&value<clusters+2?value:0;
}
static int find_entry(unsigned cluster,const U8 *name,U32 *where,unsigned *off){
    U32 s,steps=0;unsigned i,n,offset;
    do{
        if(cluster){if(cluster<2||cluster>=clusters+2)return 5;s=data_start+(U32)(cluster-2)*spc;n=spc;}
        else{s=root_start;n=root_sectors;}
        for(i=0;i<n;i++){
            if(read_sector(s+i))return 5;
            for(offset=0;offset<512;offset+=32){
                if(!sector_buf[offset])return 2;
                if(sector_buf[offset]==0xe5||sector_buf[offset+11]==15)continue;
                if(!memcmp(sector_buf+offset,name,11)){
                    if(!(sector_buf[offset+11]&16)||word(sector_buf+offset+20))return 5;
                    *where=s+i;*off=offset;return 0;
                }
            }
        }
        if(!cluster)break;cluster=(unsigned)next_cluster(cluster);
    }while(cluster&&++steps<clusters);
    return 2;
}
int directory_time(const char *path,unsigned date,unsigned time){
    const char *p=path+3;U8 name[11];unsigned c=0,i,off=0,e;U32 where=0;union REGS r;
    drive_no=path[0]-'A';
    r.x.ax=0x4409;r.x.bx=drive_no+1;intdos(&r,&r);
    if(r.x.cflag||(r.x.dx&0x1000))return 5;
    flush();if(geometry())return 5;
    while(*p){
        memset(name,' ',11);i=0;
        while(*p&&*p!='\\'){
            if(*p=='.')i=8;
            else {if(i>=11)return 5;name[i++]=(U8)*p;}
            p++;
        }
        if(name[0]==0xe5)name[0]=5;
        e=find_entry(c,name,&where,&off);if(e)return e;
        c=word(sector_buf+off+26);if(*p)p++;
    }
    if(!where)return 5;
    memcpy(before,sector_buf,512);pw(sector_buf+off+22,time);pw(sector_buf+off+24,date);
    e=write_sector(where);flush();if(e)return e;
    if(read_sector(where))return 5;
    pw(before+off+22,time);pw(before+off+24,date);
    return memcmp(before,sector_buf,512)?5:0;
}
