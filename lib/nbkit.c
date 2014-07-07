/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbkit.c 
*
* Title:    nbkit Command Processor
*
* Function:
*
*   This file provides minimal nbkit command functionality and passes the rest
*   off to the nbkit_cmd script in the specified caboodle.
*
* Synopsis:
*
*   int nbKit(int argc,char *argv[]);
* 
* Description
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2013-02-03 Ed Trettevik - Introduced in 0.8.14
* 2014-01-25 eat 0.9.00 - Checker updates
*=============================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>

static int nbKitUsage(void){
    printf("Usage:\n");
    printf("  nbkit { -k | --kits }              : Display kits\n");
    printf("  nbkit { -c | --caboodles }         : Display caboodles\n");
    printf("  nbkit <caboodle> unlink            : Remove caboodle link\n");
    printf("  nbkit <caboodle> link <directory>  : Create caboodle link\n");
    printf("  nbkit <caboodle> <verb> ...        : Caboodle nbkit_ operations\n");
    printf("  nbkit <caboodle> help              : Display nbkit_ help\n");
    return(0);
    }

static int nbKitKits(void){
  DIR *dir;
  struct dirent *ent;
  int i;
  char *kitroot[2]={"/usr/share/nbkit","/usr/local/share/nbkit"};
  for(i=0;i<2;i++){
    dir=opendir(kitroot[i]);
    if(dir){
      while((ent=readdir(dir))!=NULL){
        if(*ent->d_name!='.'){
          printf("%s/%s\n",kitroot[i],ent->d_name);
          }
        }
      closedir(dir);
      }
    }
  return(0);
  }

static int nbKitCaboodles(char *home){
  DIR *dir;
  struct dirent *ent;
  char *subdir[2]={".nb",".nbkit"};
  char dirname[1024];
  char linkname[1024];
  char cabdir[1024];
  int i;
  ssize_t linksize;

  for(i=0;i<2;i++){
    snprintf(dirname,sizeof(dirname),"%s/%s/caboodle",home,subdir[i]); 
    dir=opendir(dirname);
    if(dir){
      fprintf(stderr,"Caboodles defined at %s\n",dirname);
      while((ent=readdir(dir))!=NULL){
        if(*ent->d_name!='.'){
          snprintf(linkname,sizeof(linkname),"%s/%s",dirname,ent->d_name);
          linksize=readlink(linkname,cabdir,sizeof(cabdir));
          if(linksize>0 && linksize<sizeof(cabdir)){
            *(cabdir+linksize)=0;
            printf("  %s -> %s\n",ent->d_name,cabdir);
            }
          else if(linksize<0){
            fprintf(stderr,"Error reading link %s - %s\n",linkname,strerror(errno)); 
            closedir(dir);
            return(1);
            }
          }
        }
      closedir(dir);
      }
    }
  return(0);
  }

static int nbKitUnlink(char *home,char *caboodle){
  char pathname[1024];
  int len,rc;
  
  len=snprintf(pathname,sizeof(pathname),"%s/.nb/caboodle/%s",home,caboodle);
  if(len<0 || len>=sizeof(pathname)){
    fprintf(stderr,"Unable to unlink %s - link path too long for buffer\n",caboodle);
    return(1);
    }
  rc=unlink(pathname);
  if(rc<0){
    fprintf(stderr,"Unable to unlink %s - %s\n",pathname,strerror(errno));
    return(1);
    }
  return(0);
  }

static int nbKitLink(const char *home,const char *caboodle,const char *directory){
  char pathname[1024];
  int len,rc;
  DIR *dir;
  struct stat filestat;
 
  // 2014-02-03 eat - add verification of directory parameter
  if(stat(directory,&filestat)){
    fprintf(stderr,"Unable to obtain attributes of %s - %s\n",directory,strerror(errno));
    return(1);
    }
  if(!(S_ISDIR(filestat.st_mode))){
    fprintf(stderr,"Directory parameter %s is not a directory\n",directory);
    return(1);
    }
  len=snprintf(pathname,sizeof(pathname),"%s/.nb",home);
  if(len<0 || len>=sizeof(pathname)){
    fprintf(stderr,"Unable to link %s - link path too long for buffer\n",caboodle);
    return(1);
    }
  dir=opendir(pathname);
  if(!dir){
    if(mkdir(pathname,S_IRWXU)<0){
      fprintf(stderr,"Unable to make directory %s - %s\n",pathname,strerror(errno));
      return(1);
      }
    }
  else closedir(dir);
  len=snprintf(pathname,sizeof(pathname),"%s/.nb/caboodle",home);
  if(len<0 || len>=sizeof(pathname)){
    fprintf(stderr,"Unable to link %s - link path too long for buffer\n",caboodle);
    return(1);
    }
  dir=opendir(pathname);
  if(!dir){
    if(mkdir(pathname,S_IRWXU)<0){
      fprintf(stderr,"Unable to make directory %s - %s\n",pathname,strerror(errno));
      return(1);
      }
    }
  else closedir(dir);
  len=snprintf(pathname,sizeof(pathname),"%s/.nb/caboodle/%s",home,caboodle);
  if(len<0 || len>=sizeof(pathname)){
    fprintf(stderr,"Unable to link %s - link path too long for buffer\n",caboodle);
    return(1);
    }
  rc=unlink(pathname);
  if(rc<0 && errno!=ENOENT){
    fprintf(stderr,"Unable to unlink %s - %s\n",pathname,strerror(errno));
    return(1);
    }
  rc=symlink(directory,pathname); // 2014-02-08 eat - CID 1167487 TOCTOU - OK, checking again after
  if(rc<0){
    fprintf(stderr,"Unable to link %s to %s - %s\n",pathname,directory,strerror(errno));
    return(1);
    }
  if(stat(directory,&filestat) || !(S_ISDIR(filestat.st_mode))){
    fprintf(stderr,"Directory parameter %s not recognized as a directory - removing link\n",directory);
    rc=unlink(pathname);
    if(rc<0) fprintf(stderr,"Unable to unlink %s - %s\n",pathname,strerror(errno));
    return(1);
    }
  return(0);
  }

static int nbKitCaboodle(char *home,char *caboodle,char *cabdir,size_t size){
  char linkname[1024];
  int len;
  ssize_t linksize;

  len=snprintf(linkname,sizeof(linkname),"%s/.nb/caboodle/%s",home,caboodle);
  if(len<0 || len>=size){
    fprintf(stderr,"Unable to lookup caboodle %s/.nb/caboodle/%s - path too long\n",home,caboodle);
    return(1);
    }
  linksize=readlink(linkname,cabdir,size);
  if(linksize<0){
    if(errno==ENOENT) fprintf(stderr,"Caboodle %s not defined\n",caboodle);
    else fprintf(stderr,"Error reading link %s - %s\n",linkname,strerror(errno));
    return(1);
    }
  if(linksize>=size){
    fprintf(stderr,"Error reading link %s - caboodle directory path too long for buffer\n",linkname);
    return(1);
    }
  *(cabdir+linksize)=0;
  return(0);
  }
static int nbKitDirectory(char *home,char *caboodle){
  char cabdir[1024];
  int rc;

  rc=nbKitCaboodle(home,caboodle,cabdir,sizeof(cabdir));
  if(rc) return(rc);
  printf("%s",cabdir);
  return(0);
  }

static int nbKitCmd(char *home,char *caboodle,int argc,char *argv[]){
  char pathname[1024];
  char cabdir[1024];
  char *tarcmd=cabdir;  // use same area for tar command if needed
  int len,rc;

  rc=nbKitCaboodle(home,caboodle,cabdir,sizeof(cabdir));
  if(rc) return(rc);
  len=snprintf(pathname,sizeof(pathname),"%s/bin/nbkit_",cabdir);
  if(len<0 || len>=sizeof(pathname)){
    fprintf(stderr,"Unable to execute %s/nbkit_ - path too long for buffer\n",cabdir);
    return(1);
    }
  rc=chdir(cabdir);
  if(rc<0){
    fprintf(stderr,"Unable to change working directory to %s - %s\n",cabdir,strerror(errno));
    return(1);
    }
  execvp(pathname,argv);
  if(errno==ENOENT){
    if(argc==4 && strcmp(argv[2],"use")==0 && strstr(argv[3],"caboodle")!=NULL){
      len=snprintf(tarcmd,sizeof(cabdir),"tar -xf %s bin/nbkit_",argv[3]);
      if(len<0 || len>=sizeof(cabdir)){
        fprintf(stderr,"Unable to invoke tar command - too large for buffer\n");
        return(1);
        }
      rc=system(tarcmd);
      if(rc==0){
        execvp(pathname,argv);
        }
      else{ 
        fprintf(stderr,"\nAttempt to initialize the caboodle with the following command failed rc=%d\n",rc);
        fprintf(stderr,"  %s\n",tarcmd);
        return(1);
        }
      }
    else{
      fprintf(stderr,"File '%s' not found\n\n",pathname);
      fprintf(stderr,"Issue the following command to initialize the caboodle:\n");
      fprintf(stderr,"  nbkit %s use <caboodle-kit-archive>\n\n",caboodle);
      fprintf(stderr,"To locate the caboodle kit archive file:\n");
      fprintf(stderr,"  nbkit -k | grep caboodle\n\n");
      fprintf(stderr,"If not found, install nbkit-caboodle package.\n");
      fprintf(stderr,"See documentation at http://nodebrain.org\n");
      return(1);
      }
    }
  fprintf(stderr,"Unable to invoke the following command - %s\n",strerror(errno));
  fprintf(stderr,"  %s\n",pathname);
  return(1);
  }

int nbKit(int argc,char *argv[]){
  struct passwd *pwd;
  
  pwd=getpwuid(getuid());
  if(!pwd){
    fprintf(stderr,"Unable to local user account information.\n");
    return(1);
    }

  // Handle:  nbkit
  if(argc<=1) return(nbKitUsage());

  // Handle:  nbkit --option
  if(*argv[1]=='-'){
    if(strcmp(argv[1],"-c")==0 || strcmp(argv[1],"--caboodles")==0){
      if(argc>2){
        printf("Unexpected parameters after -c or --caboodles option\n");
        return(1);
        }
      return(nbKitCaboodles(pwd->pw_dir));
      }
    if(strcmp(argv[1],"-k")==0 || strcmp(argv[1],"--kits")==0){
      if(argc>2){
        printf("Unexpected parameters after -k or --kits option\n");
        return(1);
        }
      return(nbKitKits());
      }
    return(nbKitUsage());
    }

  // Handle:  nbkit <caboodle>
  if(argc==2) return(nbKitDirectory(pwd->pw_dir,argv[1]));

  // Handle:  nbkit <caboodle> <verb> ... 
  // Note: argc>2
  if(strcmp(argv[2],"link")==0){
    if(argc>4){
      printf("Link command passed too many parameters - only three allowed\n");
      printf("nbkit <caboodle> link <directory>\n");
      return(1);
      }
    if(argc<4){  // 2014-01-25 eat - CID 971425
      printf("Link command requires a directory - three parameters required\n");
      printf("nbkit <caboodle> link <directory>\n");
      return(1);
      }
    return(nbKitLink(pwd->pw_dir,argv[1],argv[3]));
    }
  if(strcmp(argv[2],"unlink")==0){
    if(argc>3){
      printf("Unlink command passed too many parameters - only two allowed\n");
      printf("nbkit <caboodle> unlink\n");
      return(1);
      }
    return(nbKitUnlink(pwd->pw_dir,argv[1]));
    }
  return(nbKitCmd(pwd->pw_dir,argv[1],argc,argv));
  }
