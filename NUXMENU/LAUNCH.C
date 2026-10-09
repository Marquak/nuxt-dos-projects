/* Small supervisor. The complete UI exits before COMMAND.COM runs the job. */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <direct.h>
#include <conio.h>
#include <malloc.h>
#include <io.h>
#include <ctype.h>
static char home[260],ui[280],job[280],original[260];
static int __far hard(unsigned a,unsigned b,unsigned __far *c){(void)a;(void)b;(void)c;_hardresume(_HARDERR_FAIL);return 0;}
int main(int argc,char **argv){
 char *slash,*shell;const char *args[16];int i,rc=0;
 if(argc>13){puts("Too many options.");return 1;}
 if(!_cmdname(home)||!(slash=strrchr(home,'\\'))){puts("Cannot locate MENU.EXE.");return 1;}slash[1]=0;
 if(strlen(home)>68||strpbrk(home," %&|<>^\"")){puts("Use a short DOS directory, e.g. C:\\NUXMENU.");return 1;}
 sprintf(ui,"%sNUI.EXE",home);sprintf(job,"%sNM-RUN.BAT",home);
 if(access(ui,0)){puts("Keep MENU.EXE and NUI.EXE together in the same folder.");return 1;}
 shell=getenv("COMSPEC");if(!shell||access(shell,0)){puts("COMSPEC must point to COMMAND.COM.");return 1;}
 if(!getcwd(original,sizeof(original))){puts("Cannot read the current directory.");return 1;}
 _harderr(hard);
 args[0]=ui;args[1]="/DRIVER";for(i=1;i<argc;i++)args[i+1]=argv[i];args[argc+1]=NULL;
 for(;;){
  i=(int)strlen(home);if(i>3)home[i-1]=0;
  rc=_chdrive(toupper(home[0])-'A'+1);if(!rc)rc=chdir(home);
  if(i>3)home[i-1]='\\';
  if(rc){perror("Cannot open the menu folder");puts(home);rc=1;break;}
  remove(job);_heapshrink();rc=spawnv(P_WAIT,ui,args);
  /* Startup overrides apply once. Honour appearance changes after a child. */
  args[2]=NULL;
  if(rc!=10)break;
  if(access(job,0)){puts("The menu did not create its launch job.");rc=1;break;}
  _heapshrink();rc=spawnl(P_WAIT,shell,shell,"/C",job,NULL);
  remove(job);
  if(rc<0){perror("Could not launch COMMAND.COM");puts("Press a key to return to the menu.");getch();}
 }
 _chdrive(toupper(original[0])-'A'+1);chdir(original);
 if(rc<0)perror("Could not launch NUI.EXE");
 return rc<0?1:rc;
}
