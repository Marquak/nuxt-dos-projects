#include "MENU.H"
#include <stdio.h>
#include <string.h>
#include <io.h>
static int failures,checks;
static void check(int v,const char *s){checks++;if(!v){printf("FAIL: %s\n",s);failures++;}}
static void seal(void){cfg.crc=checksum(&cfg,(unsigned)offsetof(Config,crc));}
static unsigned short reference_crc(const unsigned char *p,unsigned size){
 unsigned short crc=0xffff;unsigned j;
 while(size--){crc^=*p++;for(j=0;j<8;j++)crc=(crc&1)?(crc>>1)^0xa001:crc>>1;}return crc;
}
int main(void){
 FILE *f;unsigned char old,bytes[256];int i,match=1;
 check(checksum("123456789",9)==0x4b37,"CRC known check value");
 for(i=0;i<256;i++)bytes[i]=(unsigned char)i;
 for(i=0;i<=256;i++)if(checksum(bytes,(unsigned)i)!=reference_crc(bytes,(unsigned)i))match=0;
 check(match,"nibble CRC matches bitwise CRC for all prefix lengths");
 if(!access("NUXMENU.DAT",0)||!access("NUXMENU.BAK",0)){puts("Run only in an empty test folder.");return 2;}
 defaults("C:\\MENU\\");check(valid_config(),"defaults valid");check(sizeof(Item)==277,"packed DOS record size");
 cfg.item[3].kind=PROGRAM;cfg.item[3].parent=0;strcpy(cfg.item[3].name,"Demo");strcpy(cfg.item[3].program,"C:\\DEMO\\GO.BAT");strcpy(cfg.item[3].cwd,"C:\\DEMO");strcpy(cfg.item[3].args,"/A /B");cfg.item[3].pause=1;seal();
 check(valid_config(),"valid program");check(save_config(""),"first save");strcpy(cfg.item[3].name,"Changed");check(save_config(""),"second save");
 memset(&cfg,0,sizeof(cfg));check(load_config("")==1,"load current");check(!strcmp(cfg.item[3].name,"Changed")&&cfg.item[3].pause==1,"roundtrip");
 f=fopen("NUXMENU.DAT","wb");fputs("broken",f);fclose(f);check(load_config("")==2,"recover corrupt primary");check(!strcmp(cfg.item[3].name,"Demo"),"backup matches previous save");
 strcpy(cfg.item[3].name,"Recovered edit");check(save_config(""),"save after corrupt-primary recovery");
 check(load_config("")==1&&!strcmp(cfg.item[3].name,"Recovered edit"),"repaired primary loads");
 remove("NUXMENU.DAT");check(load_config("")==2,"recover missing primary after interrupted rename");
 check(!strcmp(cfg.item[3].name,"Demo"),"recovery save preserved known-good backup");
 check(save_config(""),"repair missing primary");
 strcpy(cfg.item[3].name,"Next edit");check(save_config(""),"ordinary backup rotation resumes");
 remove("NUXMENU.DAT");check(load_config("")==2&&!strcmp(cfg.item[3].name,"Demo"),"rotated backup remains valid");
 old=cfg.item[0].parent;cfg.item[0].parent=0;seal();check(!valid_config(),"reject self-cycle");cfg.item[0].parent=old;
 cfg.item[0].parent=1;cfg.item[1].parent=0;seal();check(!valid_config(),"reject two-folder cycle");cfg.item[0].parent=cfg.item[1].parent=ROOT;
 cfg.item[3].parent=3;seal();check(!valid_config(),"reject program parent");cfg.item[3].parent=0;seal();
 cfg.item[3].name[0]^=1;check(!valid_config(),"CRC corruption detected");cfg.item[3].name[0]^=1;seal();
 for(i=0;i<32;i++)cfg.item[3].name[i]='A';seal();check(!valid_config(),"reject unterminated name");strcpy(cfg.item[3].name,"Demo");seal();
 check(child_of(3,0)&&!child_of(0,3),"ancestry");
 check(absolute_path("D:\\GAMES\\TEST.EXE")&&!absolute_path("C:TEST.EXE")&&!absolute_path("A")&&!absolute_path(""),"absolute paths");
 check(!safe_path("C:\\BAD&GO.EXE")&&!safe_path("C:\\%BAD%.EXE")&&!safe_path("C:\\LONG NAME.EXE"),"batch control characters blocked");
 check(runnable("C:\\ABC.COM")&&runnable("a.bat")&&!runnable("X.TXT"),"executable extension");
 remove("NUXMENU.BAK");check(load_config("")==0,"missing config");
 f=fopen("NUXMENU.BAK","wb");fputs("bad",f);fclose(f);check(load_config("")==-1,"do not silently replace corrupt backup");remove("NUXMENU.BAK");
 printf("DATA: %d checks, %d failures\n",checks,failures);return failures!=0;
}
