#ifndef NUXMENU_H
#define NUXMENU_H
#include <stddef.h>
#define ITEMS 128
#define PATHLEN 80
#define ROOT 255
#define FOLDER 1
#define PROGRAM 2
#pragma pack(__push,1)
typedef struct {
 unsigned char kind,parent,pause;
 unsigned short rank;
 char name[32],program[PATHLEN],args[80],cwd[PATHLEN];
} Item;
typedef struct {
 char magic[8];
 unsigned short version;
 unsigned char mode,theme,colors[8],current,selected;
 char browse[PATHLEN];
 Item item[ITEMS];
 unsigned short crc;
} Config;
#pragma pack(__pop)
extern Config cfg;
int valid_config(void);
unsigned short checksum(const void *,unsigned);
int load_config(const char *);
int save_config(const char *);
int safe_path(const char *);
int absolute_path(const char *);
int runnable(const char *);
int child_of(int,int);
void defaults(const char *);
#endif
