#include "MENU.H"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <dos.h>
Config cfg;
static int preserve_backup;
/* Same CRC-16/IBM, initial FFFF. Two nibble lookups replace eight bit loops.
   A 32-byte table avoids the 512-byte cost of a full byte table on an XT. */
unsigned short checksum(const void *src,unsigned size){
 static const unsigned short table[16]={
  0x0000,0xcc01,0xd801,0x1400,0xf001,0x3c00,0x2800,0xe401,
  0xa001,0x6c00,0x7800,0xb401,0x5000,0x9c01,0x8801,0x4400};
 const unsigned char *p=src;unsigned short crc=0xffff;
 while(size--){crc^=*p++;crc=(crc>>4)^table[crc&15];crc=(crc>>4)^table[crc&15];}
 return crc;
}
static int text_ok(const char *s,unsigned n){
 unsigned i;for(i=0;i<n;i++){if(!s[i])return 1;if((unsigned char)s[i]<32)return 0;}return 0;
}
int safe_path(const char *s){
 const unsigned char *p=(const unsigned char *)s;
 if(!*p)return 0;
 while(*p){if(!isalnum(*p)&&!strchr("\\:._-~!#$'()@{}",*p))return 0;p++;}
 return 1;
}
int absolute_path(const char *s){return safe_path(s)&&isalpha(s[0])&&s[1]==':'&&s[2]=='\\';}
int runnable(const char *s){
 const char *p=strrchr(s,'.');return p&&(!stricmp(p,".EXE")||!stricmp(p,".COM")||!stricmp(p,".BAT"));
}
int child_of(int node,int ancestor){
 unsigned n=0;
 while(node!=ROOT&&n++<ITEMS){if(node==ancestor)return 1;node=cfg.item[node].parent;}
 return 0;
}
static int valid_fields(void){
 unsigned i,j,n;int p;
 if(memcmp(cfg.magic,"NUXMENU1",8)||cfg.version!=1||cfg.mode>2||cfg.theme>6||!text_ok(cfg.browse,PATHLEN))return 0;
 if(!absolute_path(cfg.browse))return 0;
 for(i=0;i<ITEMS;i++){
  Item *e=&cfg.item[i];
  if(!e->kind)continue;
  if(e->pause>1||e->kind>PROGRAM||!text_ok(e->name,32)||!e->name[0]||!text_ok(e->program,PATHLEN)||!text_ok(e->cwd,PATHLEN)||!text_ok(e->args,80))return 0;
  if(e->kind==PROGRAM&&(!absolute_path(e->program)||!runnable(e->program)||!absolute_path(e->cwd)))return 0;
  p=i;n=0;
  while(p!=ROOT){
   if(p<0||p>=ITEMS||!cfg.item[p].kind||n++>=ITEMS)return 0;
   j=cfg.item[p].parent;if(j!=ROOT&&(j>=ITEMS||cfg.item[j].kind!=FOLDER))return 0;p=j;
  }
 }
 if(cfg.current!=ROOT&&(cfg.current>=ITEMS||cfg.item[cfg.current].kind!=FOLDER))return 0;
 if(cfg.selected!=ROOT&&(cfg.selected>=ITEMS||!cfg.item[cfg.selected].kind))return 0;
 return 1;
}
int valid_config(void){
 return valid_fields()&&cfg.crc==checksum(&cfg,(unsigned)offsetof(Config,crc));
}
void defaults(const char *directory){
 unsigned i;static const unsigned char colors[8]={0x17,0x1f,0x70,0x1b,0x3f,0x17,0x0f,0x19};
 static const char *names[3]={"Games","Demos","Utilities"};
 memset(&cfg,0,sizeof(cfg));memcpy(cfg.magic,"NUXMENU1",8);cfg.version=1;cfg.current=ROOT;cfg.selected=0;
 memcpy(cfg.colors,colors,8);strcpy(cfg.browse,directory);
 for(i=0;i<3;i++){cfg.item[i].kind=FOLDER;cfg.item[i].parent=ROOT;cfg.item[i].rank=(unsigned short)i;strcpy(cfg.item[i].name,names[i]);}
 cfg.crc=checksum(&cfg,(unsigned)offsetof(Config,crc));
}
static int read_one(const char *path){
 FILE *f=fopen(path,"rb");int ok;if(!f)return 0;
 ok=fread(&cfg,1,sizeof(cfg),f)==sizeof(cfg);if(fgetc(f)!=EOF)ok=0;fclose(f);return ok&&valid_config();
}
int load_config(const char *base){
 char p[PATHLEN+16];int exists;preserve_backup=0;sprintf(p,"%sNUXMENU.DAT",base);
 if(read_one(p))return 1;
 exists=access(p,0)==0;
 sprintf(p,"%sNUXMENU.BAK",base);if(read_one(p)){preserve_backup=1;return 2;}
 return exists||access(p,0)==0?-1:0;
}
int save_config(const char *base){
 char target[PATHLEN+16],temp[PATHLEN+16],bak[PATHLEN+16];FILE *f;int ok,existed;
 sprintf(target,"%sNUXMENU.DAT",base);sprintf(temp,"%sNUXMENU.NEW",base);sprintf(bak,"%sNUXMENU.BAK",base);
 if(!valid_fields())return 0;
 cfg.crc=checksum(&cfg,(unsigned)offsetof(Config,crc));
 f=fopen(temp,"wb");if(!f)return 0;
 ok=fwrite(&cfg,1,sizeof(cfg),f)==sizeof(cfg);
 if(fflush(f))ok=0;
 /* fclose plus DOS disk reset also works with older DOS emulators. */
 if(fclose(f))ok=0;
 if(!ok){remove(temp);return 0;}
 {union REGS r;r.h.ah=0x0d;intdos(&r,&r);}
 existed=access(target,0)==0;
 if(existed&&preserve_backup){
  /* The primary was corrupt when loaded. Keep the verified recovery copy. */
  if(remove(target)){remove(temp);return 0;}
 }else if(existed){
  if(access(bak,0)==0&&remove(bak)){remove(temp);return 0;}
  if(rename(target,bak)){remove(temp);return 0;}
 }
 if(rename(temp,target)){if(existed&&!preserve_backup)rename(bak,target);remove(temp);return 0;}
 preserve_backup=0;return 1;
}
