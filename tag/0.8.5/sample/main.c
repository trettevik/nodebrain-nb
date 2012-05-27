/*
* Compile:
*
*    cc -o main main.c -lnb -lm -ldl
*
*    -or- if trying in sample directory before install
*
*    cc -o main main.c ../libnb.a -lm -ldl
*========================================================================
*/
#include <nb/nbapi.h>
// #include "../nbapi.h" // use this if testing in sample directory before install
int main(){
  int i;
  int nbargc=2;
  char *nbargv[2]={"mypgm","main.nb"};
  nbCELL context;

  printf("hello\n");
  context=nbStart(nbargc,nbargv);

  nbCmd(context,">nbDaemon alert a=1,b=2;",NB_CMDOPT_ECHO);
  for(i=0;i<10;i++){
    system("sleep 1");
    nbCmd(context,">nbDaemon show /c",NB_CMDOPT_ECHO);
    }

  nbStop(context);
  }
