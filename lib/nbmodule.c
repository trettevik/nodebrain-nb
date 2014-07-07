/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software, provided that the above copyright notice,
* this permission notice, and the following disclaimer are retained with source
* code and reproduced in documentation included with binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*==============================================================================
* Program:  NodeBrain
*
* File:     nbmodule.c 
*
* Title:    Module Management Functions
*
* Function:
*
*   This file provides functions for managing dynamic load modules used as
*   NodeBrain plugins.  Initially this will be used to support "skill modules"
*   although other plugins may be supported in the future.  
*
* Synopsis:
*
*   #include "nbmodule.h"
*
*   void nbModuleInit();
*
*   NB_Term *nbModuleDeclare(NB_Term *context,char *ident,char *cursor,char *msg,size_t msglen);
*   NB_Term *nbModuleLocate(char *ident);
*   NB_Term *nbModuleUndeclare(char *ident);
*
*   void *nbModuleSymbol(NB_Term *context,char *ident,char *suffix,void **moduleHandleP,char *msg,size_t msglen);
*
* Description
*
*   Modules are represented as NodeBrain objects and assigned names in a
*   special name space, like other DECLARE'd objects.  The module name space
*   is managed by this source file.
*
*	declare <term> module <filename>;
*
*   A module object is represented by the NB_MODULE structure as defined in
*   the nbmodule.h, and is created by a call to nbModuleDeclare().
*
*       TERM *modterm=nbModuleDeclare(context,ident,cursor);
*
*   The cursor argument points just past the module keyword in the DECLARE
*   command.  The same filename may be referenced in multiple declarations,
*   but a module object will only be created on the first call for a given
*   filename.  Subsequent declarations will reuse the existing module object.  
*
*   Modules are not actually loaded until referenced by a call to 
*   nbModuleSymbol().
*   
*       address=nbModuleSymbol(context,"module.symbol","Bind",&moduleHandle,msg,msglen).
*
*   The "module" component of the identifier is used to search the declared
*   module namespace.  If the associated module has not been loaded, it is
*   loaded at this time.  Then the "symbol" component is used to locate the
*   symbol in the loaded module.
*
*   Object methods are provided for printing and destroying module objects. A
*   module object will be destroyed when the use count goes to zero.  At that
*   time a "delete" operation will be performed on the loaded module.  The
*   use count on a module object will only decrement if a module term is
*   undeclared by a call to nbModuleUndeclare().
*   
*   The handling of dynamic load modules is different on various platforms
*   NodeBrain supports.  We use conditional compilation in this file to
*   select the code needed to implement these functions on a given platform.
*   
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-04-22 eat 0.5.4  Introduced prototype
* 2003-07-22 eat 0.5.4  Included Mac OS X support
* 2003-07-22 eat 0.5.4  Included HP-UX support
* 2003-03-31 eat 0.6.0  Prefixed symbols with "_" on OS X.    
* 2004-10-08 eat 0.6.1  Qualified error with const to eliminate warning on BSD
* 2004-10-11 eat 0.6.1  Support wild module suffix (e.g. nb_mod_tree.?)
*                       This was included to make scripts that reference
*                       skill modules portable to a variety of platforms that
*                       use different suffixes.
* 2005-04-09 eat 0.6.2  changed name of INIT() function to nbBind()
* 2005-05-14 eat 0.6.3  fixed bug in path separator on Windows
* 2005-05-14 eat 0.6.3  added support for ";" as platform independent path separator
* 2005-05-14 eat 0.6.3  included path in search for unique module definitions
* 2005-05-14 eat 0.6.3  included args and text in search for unique module definitions
* 2005-05-14 eat 0.6.3  fixed bug in module print method
* 2005-05-14 eat 0.6.3  nbModuleSymbol modified to return moduleHandle
* 2007-07-26 eat 0.6.8  Included nbModuleLoad() export option for preloading shared libraries for modules
* 2008-10-30 eat 0.7.3  Module names changed from nb_mod_<module>.<suffix> to nb_<module>.<suffix>
* 2008-11-01 eat 0.7.3  Replaced NB_MOD_SUFFIX with LT_MODULE_EXT
* 2009-02-13 eat 0.7.4  Fixed error message when loading modules on OS X 
* 2009-02-13 eat 0.7.4  Fixed bug in "show +m" on OS X
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-02-06 dtl Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-12-15 eat 0.8.13 Checker updates
* 2012-12-31 eat 0.8.13 Checker updates
* 2013-01-11 eat 0.8.13 Checker updates 
* 2013-03-10 eat 0.8.15 Dropping NB_API_VERSION from module names.
*                       Modules where ${_libdir}/nb/0.x/nb_<name>.so.0 symbolic
*                       links, but now just ${_libdir}/nb/0.x/nb_<name>.so files.
*                       The API version is in the path 0.x, and the x determine
*                       compatibilty of the module with an application using
*                       nb-0.x.p and the associated libnb-0.x.p.so
* 2013-04-06 eat 0.8.15 Modified nbModuleDeclare to avoid buffer overflow
* 2014-04-05 eat 0.9.01 Checker update
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

struct NB_MODULE *moduleList=NULL; /* module object used list */
struct NB_MODULE *moduleFree=NULL; /* module object free list */
NB_Term   *moduleC=NULL;    /* module context */
//struct HASH   *moduleH=NULL;    /* module hash */
struct TYPE   *moduleType=NULL;
struct NB_MODULE *nb_ModuleFree;

/**********************************************************************
* Platform specific routines
**********************************************************************/
#if defined(WIN32)

// shared library option not implemented
HINSTANCE nbModuleLoad(char *name,int export,char *msg,size_t msglen){
  HINSTANCE hinstLib;

  if(trace) outMsg(0,'T',"Module %s requested",name);

  *msg=0;
  // Get a handle to the DLL module
  hinstLib = LoadLibrary(name);
  if(hinstLib==NULL) snprintf(msg,msglen,"Unable to load %s - errcode=%d",name,GetLastError());
  else if(trace) outMsg(0,'T',"Module %s loaded",name);
  return(hinstLib);
  }

#elif defined(MACOS)

#include <mach-o/dyld.h>

// shared library option not implemented
void *nbModuleLoad(char *name,int export,char *msg,size_t msglen){
  NSModule module;
  NSObjectFileImage objectFileImage;
  NSObjectFileImageReturnCode returnCode;

  returnCode=NSCreateObjectFileImageFromFile(name,&objectFileImage);
  if(returnCode!=NSObjectFileImageSuccess){
    //outMsg(0,'E',"Unable to load %s",name);
    snprintf(msg,msglen,"Unable to load %s\n",name); 
    return(NULL);
    }
  module=NSLinkModule(objectFileImage,"",NSLINKMODULE_OPTION_PRIVATE|NSLINKMODULE_OPTION_BINDNOW);
  NSDestroyObjectFileImage(objectFileImage) ;
  //if(module==NULL) outMsg(0,'E',"Unable to link %s",name);
  if(module==NULL) snprintf(msg,msglen,"Unable to link %s",name);
  return((void *)module);
  }

#elif defined(HPUX)

#include <dl.h>

// shared library option not implemented
void *nbModuleLoad(char *name,int export,char *msg,size_t msglen){
  void *handle;

  *msg=0;
  handle=shl_load(name,BIND_DEFERRED,0L);
  if(handle==NULL) snprintf(msg,msglen,"Unable to load %s - errno=%d\n",name,errno);
  return(handle);
  }

#else

#include <dlfcn.h>  /* linux, solaris, FreeBSD, NetBSD, OpenBSD */

void *nbModuleLoad(char *name,int export,char *msg,size_t msglen){
  void *handle;
  const char *error;
  int flag=RTLD_LAZY;
  
  if(export) flag=RTLD_NOW|RTLD_GLOBAL;      // adjust flag for export request 
  handle=dlopen(name,flag);
  if(!handle){
    error=dlerror();
    if(error!=NULL) snprintf(msg,msglen,"Unable to load %s - %s",name,error);
    else snprintf(msg,msglen,"Unable to load %s - error unknown",name);
    return(NULL);
    }
  if(trace) outMsg(0,'T',"Module %s loaded",name);
  *msg=0;
  return(handle);
  }

#endif

#if defined(WIN32)

void *nbModuleSym(HINSTANCE handle,char *symbol,char *msg,size_t msglen){
  void *addr;

  *msg=0;
  addr=(void *)GetProcAddress(handle,symbol);
  if(addr==NULL){
    snprintf(msg,msglen,"Unable to locate \"%s\"",symbol);
    return(NULL);
    }
  if(trace) outMsg(0,'T',"Symbol %s located",symbol);
  return(addr);
  }

#elif defined(MACOS)

void *nbModuleSym(void *handle,char *symbol,char *msg,size_t msglen){
  void *addr;
  char ubsymbol[128];
  NSSymbol nsSymbol;

  *msg=0;
  *ubsymbol='_';
  strncpy(ubsymbol+1,symbol,126);
  *(ubsymbol+127)=0;
  nsSymbol=NSLookupSymbolInModule((NSModule)handle,ubsymbol);  
  if(nsSymbol==NULL){
    snprintf(msg,msglen,"Unable to lookup %s",symbol);
    return(NULL);
    }
  addr=NSAddressOfSymbol(nsSymbol);
  if(addr==NULL) snprintf(msg,msglen,"Unable to locate \"%s\"",symbol);
  return(addr); 
  }

#elif defined(HPUX)

void *nbModuleSym(void *handle,char *symbol,char *msg,size_t msglen){
  void *addr;

  *msg=0;
  if(shl_findsym((shl_t *)&handle,symbol,(short)TYPE_PROCEDURE,&addr)<0){
    snprintf(msg,msglen,"Unable to locate %s - errno=%d\n",symbol,errno);
    return(NULL);
    }
  return(addr);
  }

#else

void *nbModuleSym(void *handle,char *symbol,char *msg,size_t msglen){
  void *addr;

  *msg=0;
  if((addr=dlsym(handle,symbol))==NULL){
    snprintf(msg,msglen,"Unable to locate \"%s\" - %s",symbol,dlerror());
    return(NULL);
    }
  return(addr);
  }

#endif

/**********************************************************************
* Private Object Methods
**********************************************************************/
void printModule(struct NB_MODULE *module){
  if(module==NULL) outPut("???");
  else{
    outPut("%s ",module->object.type->name);
    if(module->path!=NULL && *(module->path->value)!=0){
      outPut("{");
      printObject((NB_Object *)module->path);
      outPut("}");
      }
    printObject((NB_Object *)module->name);
    if(module->args!=NULL) printObject((NB_Object *)module->args);
    if(module->text!=NULL){
      outPut(":");
      printStringRaw(module->text);
      }
    }
  }

/*
*  Call destructor  - using destroyCondition()
*/
void destroyModule(struct NB_MODULE *mod){
  mod->object.next=(NB_Object *)nb_ModuleFree;
  nb_ModuleFree=mod;
  }

/*
* Constructor - not intended as a public method
*/
static struct NB_MODULE *newModule(char *path,char *name,NB_List *args,char *text){
  struct NB_MODULE *module;

  module=(struct NB_MODULE *)newObject(moduleType,(void **)&nb_ModuleFree,sizeof(struct NB_MODULE));
  module->path=useString(path);
  module->name=useString(name);
  module->args=args;
  module->text=useString(text);
  module->handle=NULL;
  module->address=NULL;
  return(module);
  }

void *nbModuleSearchPath(char *path,char *filename,char *msg,size_t msglen){
  char fullname[512],*cursor=path,*delim,*separator;
  void *handle;
  size_t len;

  if(trace) outMsg(0,'T',"nbModuleSearchPath(\"%s\",\"%s\") called",path,filename);
  delim=strchr(cursor,',');   // platform independent separator
  separator=strchr(cursor,NB_MODULE_PATH_SEPARATOR); //':' or ';'
  if(separator!=NULL && (delim==NULL || separator<delim)) delim=separator;
  while(delim!=NULL){
    len=delim-cursor;
    if(len+1>=sizeof(fullname)){
      outMsg(0,'L',"nbModuleSearchPath: path element longer than max of %d in path %s",sizeof(fullname)-1,path);
      return(NULL);
      }
    strncpy(fullname,cursor,len); // 2012-12-15 eat - CID 751638
    fullname[len]='/';
    if(len+strlen(filename)+2>=sizeof(fullname)){
      outMsg(0,'L',"nbModuleSearchPath: full path name of file longer than max of %d in path %s/%s",sizeof(fullname)-1,path,filename);
      return(NULL);
      }
    strcpy(fullname+len+1,filename);
    if(trace) outMsg(0,'T',"calling nbModuleLoad(\"%s\")",fullname);
    handle=nbModuleLoad(fullname,0,msg,msglen); 
    if(handle!=NULL) return(handle);
    cursor=delim+1;
    delim=strchr(cursor,',');
    separator=strchr(cursor,NB_MODULE_PATH_SEPARATOR);
    if(separator!=NULL && (delim==NULL || separator<delim)) delim=separator;
    }
  len=strlen(cursor);
  if(len>=sizeof(fullname)-1){  // 2012-12-15 eat - CID 751576
    outMsg(0,'L',"nbModuleSearchPath: path element longer than max of %d in path %s",sizeof(fullname)-1,path);
    return(NULL);
    }
  strncpy(fullname,cursor,len); //2014-04-05 eat - VID 7618 FP
  *(fullname+len)='/';
  if(len+strlen(filename)+2>=sizeof(fullname)){
    outMsg(0,'L',"nbModuleSearchPath: full path name of file longer than max of %d in path %s/%s",sizeof(fullname)-1,path,filename);
    return(NULL);
    }
  strcpy(fullname+len+1,filename);
  if(trace) outMsg(0,'T',"calling nbModuleLoad(\"%s\")",fullname);
  handle=nbModuleLoad(fullname,0,msg,msglen);
  return(handle);
  }
  
void *nbModuleSearch(char *path,char *filename,char *msg,size_t msglen){
  void *handle;
  
  if(strchr(filename,'/')!=NULL) return(nbModuleLoad(filename,0,msg,msglen));
  if(*path!=0) return(nbModuleSearchPath(path,filename,msg,msglen));
  path=getenv("NB_MODULE_PATH");
  if(path!=NULL){
    handle=nbModuleSearchPath(path,filename,msg,msglen);
    if(handle!=NULL) return(handle);
    }
  handle=nbModuleSearchPath(NB_MODULE_PATH,filename,msg,msglen);
  if(handle!=NULL) return(handle);  
  return(nbModuleLoad(filename,0,msg,msglen));  // try native pathing
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void nbModuleInit(NB_Stem *stem){
  nb_ModuleFree=NULL;
  moduleType=nbObjectType(stem,"module",0,0,printModule,destroyModule);
  moduleC=nbTermNew(NULL,"module",nbNodeNew());
  }

NB_Term *nbModuleLocate(char *ident){
  NB_Term *term;
  if((term=nbTermFind(moduleC,ident))==NULL) return(NULL);
  return(term);
  }

/*
*  Declare module
*
*    [{"<path>"}][<modId>|"<filename>"][(args)][:text]
*          
*/
NB_Term *nbModuleDeclare(NB_Term *context,char *ident,char *cursor){
  struct NB_MODULE *module;
  NB_List *args=NULL;
  NB_Term *term;
  char path[512],modname[512],filename[512],*text="",*delim;  // 2013-04-06 eat - changed size of modname and filename
  size_t len;

  if(nbTermFind(moduleC,ident)!=NULL){
    outMsg(0,'E',"Module \"%s\" already declared.",ident);
    return(NULL);
    }
  *path=0;
  while(*cursor==' ') cursor++;
  if(*cursor=='{'){
    cursor++;
    while(*cursor==' ') cursor++;
    if(*cursor!='"'){
      outMsg(0,'E',"Expecting quoted path string at: %s",cursor);
      return(NULL);
      }
    cursor++;
    delim=strchr(cursor,'"');
    if(delim==NULL){
      outMsg(0,'E',"Unbalanced quotes in path");
      return(NULL);
      }
    strncpy(path,cursor,delim-cursor);
    *(path+(delim-cursor))=0;
    cursor=delim+1;
    while(*cursor==' ') cursor++;
    if(*cursor!='}'){
      outMsg(0,'E',"Expecting '}' at: %s",cursor);
      return(NULL);
      }
    cursor++;
    while(*cursor==' ') cursor++;
    }
  if(*cursor=='"'){
    cursor++;
    delim=strchr(cursor,'"');
    if(delim==NULL){
      outMsg(0,'E',"Unbalanced quotes in file name");
      return(NULL);
      }
    strncpy(filename,cursor,delim-cursor);
    *(filename+(delim-cursor))=0;
    if(*path!=0 && strchr(filename,'/')!=NULL){
      outMsg(0,'E',"Module name contains invalid characters");
      return(NULL);
      }
    cursor=delim+1;
    cursor++;
    }
  else{  
    int isFile=0;  // treated like quoted filename if deprecated syntax found
    delim=cursor;
    while(*delim!=' ' && *delim!='(' && *delim!=':' && *delim!=';' && *delim!=0) delim++;
    len=delim-cursor;
    if(len==0) snprintf(filename,sizeof(filename),"nb_%s%s",ident,LT_MODULE_EXT);
    else{
      if(len>=sizeof(modname)){
        outMsg(0,'E',"Module name too long for buffer at--> %s",cursor);
        return(NULL);
        }
      strncpy(modname,cursor,len);
      *(modname+len)=0;
      delim=modname+len;
      cursor+=len;
      if(*path!=0){  // enforce new syntax rules when we have a path
        if(strchr(modname,'/')!=NULL || strchr(modname,'.')!=NULL || strchr(modname,'?')!=NULL){
          outMsg(0,'E',"Module name contains invalid characters: '/', '.', or '?'");
          return(NULL);
          }
        }
      else if(len>2 && len+strlen(LT_MODULE_EXT)<sizeof(modname)){          // support deprecated syntax when we have no path
        delim--;
        if(*delim=='?'){
          outMsg(0,'W',"Question mark in module name is deprecated"); 
          delim--;
          snprintf(delim,sizeof(modname)-len,"%s",LT_MODULE_EXT);
          isFile=1;
          } 
        if(strchr(modname,'/')!=NULL || strncmp(modname,"nb_",3)==0) isFile=1;
        } 
      if(isFile){
        outMsg(0,'W',"Deprecated syntax - enclose file name in quotes or use [{<path>}][<modId>] instead");
        strcpy(filename,modname);
        }
      else if(3+strlen(modname)+strlen(LT_MODULE_EXT)>=sizeof(filename)){  // 2013-04-27 eat - VID 8759-0.8.15-2-R231
        outMsg(0,'E',"Module name too long for buffer");
        return(NULL);
        }
      else sprintf(filename,"nb_%s%s",modname,LT_MODULE_EXT); // 2014-04-05 eat VID 7618 FP
      }
    } 
  while(*cursor==' ') cursor++;
  if(*cursor=='(') args=grabObject(nbSkillArgs(context,&cursor));
  if(*cursor==':') text=cursor;
  else{
    text="";
    if(*cursor!=';' && *cursor!=0){
      outMsg(0,'E',"expecting ';' or end of line at: %s",cursor);
      if(args!=NULL) dropObject(args);
      return(NULL);
      }
    }
  for(module=moduleList;module!=NULL && (strcmp(path,module->path->value)!=0 || strcmp(filename,module->name->value)!=0 || args!=module->args || text!=module->text->value);module=(struct NB_MODULE *)module->object.next);
  if(module==NULL){
    module=newModule(path,filename,args,text);
    module->object.next=(NB_Object *)moduleList;
    moduleList=module;
    }
  term=nbTermNew(moduleC,ident,module);
  return(term);
  }

void nbModuleBind(nbCELL context,char *name,char *msg,size_t msglen){
  struct NB_MODULE *module;
  struct NB_TERM *term;
  void *(*symbol)();

  if((term=nbTermFind(moduleC,name))==NULL){
    // implicitly declare the module if necessary
    term=nbModuleDeclare((struct NB_TERM *)context,name,name);
    if(term==NULL){
      snprintf(msg,msglen,"Module \"%s\" not declared and not found",name); //2012-01-26 dtl use snprintf
      return;
      }
    }
  module=(struct NB_MODULE *)(term->def);
  if(module->address!=NULL) return; // commands are already declared
  module->address=nbModuleSearch(module->path->value,module->name->value,msg,msglen);
  if(!module->address) return;
  symbol=nbModuleSym(module->address,"nbBind",msg,msglen);
  if(!symbol) return;
  module->handle=(*symbol)(nb_SkillGloss,name,module->args,module->text->value);
  }

/*
*  Locate a symbol in a module
*
*    [module.]symbol
*
*    If module is not provided it is assumed to be the same as the
*    symbol name.
*
*    The suffix is appended to the symbol.
*
*  The symbol address is returned, and the module handle is returned
*  via the moduleHandle argument.
*/
void *nbModuleSymbol(NB_Term *context,char *ident,char *suffix,void **moduleHandleP,char *msg,size_t msglen){
  NB_Term *term;
  struct NB_MODULE *module;
  char *delim,modName[256],symName[256],*sym=ident;
  void *(*symbol)();
  size_t size;

  delim=strchr(ident,'.');
  if(delim) sym=delim+1;
  else delim=ident+strlen(ident);
  size=delim-ident;
  if(size>=sizeof(modName)) size=sizeof(modName)-1;    // 2012-12-31 eat - VID 5449-0.8.13-1 
  strncpy(modName,ident,size);
  *(modName+size)=0;
  snprintf(symName,sizeof(symName),"%s%s",sym,suffix); // 2012-12-31 eat - VID 5049,4951,5426-0.8.13-1
  if(trace) outMsg(0,'T',"module=\"%s\",symbol=\"%s\"",modName,symName);
  if((term=nbTermFind(moduleC,modName))==NULL){
    term=nbModuleDeclare(context,modName,modName);  // implicitly declare the module if necessary
    if(term==NULL){
      snprintf(msg,msglen,"Module \"%s\" not declared and not found",ident);
      return(NULL);
      }
    }
  module=(struct NB_MODULE *)(term->def);
  if(module->address==NULL){
    module->address=nbModuleSearch(module->path->value,module->name->value,msg,msglen);
    if(module->address==NULL) return(NULL);
    if(NULL!=(symbol=nbModuleSym(module->address,"nbBind",msg,msglen)))
      module->handle=(*symbol)(nb_SkillGloss,modName,module->args,module->text->value);
    }
  *moduleHandleP=module->handle;
  symbol=nbModuleSym(module->address,symName,msg,msglen);
  return(symbol); 
  }

/*
*  Display modules defined and available
*/
#if defined(WIN32)

void nbModuleShowPath(nbCELL context,char *pathcur){
  //struct dirent *ent;
  char *fullpath,*cursor,*delim,*separator;
  char modname[512],path[MAX_PATH],fullpathbuf[MAX_PATH],dirname[MAX_PATH];
  WIN32_FIND_DATA info;
  HANDLE dir;

  while(*pathcur){
    delim=strchr(pathcur,',');
    separator=strchr(pathcur,NB_MODULE_PATH_SEPARATOR);
    if(separator!=NULL && (delim==NULL || separator<delim)) delim=separator;
    if(delim){
      strncpy(dirname,pathcur,delim-pathcur);
      *(dirname+(delim-pathcur))=0;
      pathcur=delim+1;
      }
    else{
      strcpy(dirname,pathcur);
      pathcur+=strlen(pathcur);
      }
    outPut("\n  %s\n",dirname);
    if((dir=FindFirstFile(dirname,&info))!=INVALID_HANDLE_VALUE){
      do{
        cursor=info.cFileName;
        if(strncmp(cursor,"nb_",3)==0){
          cursor+=3;
          if(delim=strchr(cursor,'.')){
            strncpy(modname,cursor,delim-cursor);
            *(modname+(delim-cursor))=0;
            // 2012-03-10 eat - dropping NB_API_VERSION from module names
            //if(strncmp(delim,LT_MODULE_EXT,strlen(LT_MODULE_EXT))==0){
            if(strcmp(delim,LT_MODULE_EXT)==0){
              //cursor=delim+strlen(LT_MODULE_EXT);
              //if(*cursor=='.' && strcmp(cursor+1,NB_API_VERSION)==0){
              sprintf(path,"%s/%s",dirname,info.cFileName);
              if(GetFullPathName(path,sizeof(fullpathbuf),fullpathbuf,NULL)>0)
                fullpath=fullpathbuf;
              else fullpath=path;
              outPut("    %s -> %s\n",modname,fullpath);
              }
              //}
            }
          } 
        } while(FindNextFile(dir,&info));
      FindClose(dir);
      }
    }
  }
#else
void nbModuleShowPath(nbCELL context,char *pathcur){
  struct dirent *ent;
  char *fullpath,*cursor,*delim,*separator;
  char modname[512],path[1024],dirname[1024],fullpathbuf[PATH_MAX];
  DIR *dir;
  size_t len;

  while(*pathcur){
    delim=strchr(pathcur,',');
    separator=strchr(pathcur,NB_MODULE_PATH_SEPARATOR);
    if(separator!=NULL && (delim==NULL || separator<delim)) delim=separator;
    if(delim){
      len=delim-pathcur;
      if(len>=sizeof(dirname)) len=sizeof(dirname)-1;
      strncpy(dirname,pathcur,len);
      *(dirname+len)=0;
      pathcur=delim+1;
      }
    else{
      snprintf(dirname,sizeof(dirname),"%s",pathcur); //2012-01-31 dtl: replaced strcpy
      pathcur+=strlen(pathcur);
      }
    outPut("\n  %s\n",dirname);
    if((dir=opendir(dirname))){
      while((ent=readdir(dir))!=NULL){
        cursor=ent->d_name;
        if(strncmp(cursor,"nb_",3)==0){
          cursor+=3;
          if((delim=strchr(cursor,'.'))!=NULL){
            strncpy(modname,cursor,delim-cursor);
            *(modname+(delim-cursor))=0;
            // 2012-03-10 eat - dropping NB_API_VERSION from module names
            //if(strncmp(delim,LT_MODULE_EXT,strlen(LT_MODULE_EXT))==0){
            if(strcmp(delim,LT_MODULE_EXT)==0){
              //cursor=delim+strlen(LT_MODULE_EXT);
              //if(*cursor=='.' && strcmp(cursor+1,NB_API_VERSION)==0){
              len=snprintf(path,sizeof(path),"%s/%s",dirname,ent->d_name);
              fullpath=realpath(path,fullpathbuf);
              if(!fullpath) fullpath=path;
              outPut("    %s -> %s\n",modname,fullpath);
              }
              //}
            }
          }
        }
      closedir(dir);
      }
    }
  }
#endif

/*
*  Show version compatible modules
*/
void nbModuleShowInstalled(nbCELL context){
  char *pathcur;

  // 2013-03-10 eat - dropping NB_API_VERSION from module name
  //outPut("\nPattern: nb_<module>%s.%s\n",LT_MODULE_EXT,NB_API_VERSION);
  outPut("\nPattern: nb_<module>%s\n",LT_MODULE_EXT);
  pathcur=getenv("NB_MODULE_PATH");
  if(pathcur){
    outPut("\nNB_MODULE_PATH=%s\n",pathcur);
    nbModuleShowPath(context,pathcur);
    }
  outPut("\nDefaultPath=%s\n",NB_MODULE_PATH);
  pathcur=NB_MODULE_PATH;
  nbModuleShowPath(context,pathcur);
  }

//**********************************
// Exernal Skill API

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbSkillDeclare(nbCELL context,void *(*bindFunction)(),void *moduleHandle,char *moduleName,char *skillName,nbCELL arglist,char *text){
  NB_Skill *skill;
  char ident[256];
  int len;


  if(*moduleName==0) len=snprintf(ident,sizeof(ident),"%s",skillName);  // 2012-12-31 eat - VID 5044-0.8.13-1 
  else len=snprintf(ident,sizeof(ident),"%s.%s",moduleName,skillName);
  if(len<0 || len>=sizeof(ident)){
    outMsg(0,'L',"nbSkillDeclare;  Module and/or Skill name exceed limit of %d.",sizeof(ident)-1);
    return(-1);
    }
  if(NULL==(skill=nbSkillNew(ident,(NB_List *)arglist,text))){
    outMsg(0,'L',"nbSkillDeclare:  Unable to create skill \"%s.%s\".",moduleName,skillName);
    return(-1);
    }
  skill->term=nbTermNew(nb_SkillGloss,ident,skill);
  skill->handle=(*bindFunction)(context,moduleHandle,skill,arglist,text);
  skill->status=1;
  return(0);
  }

#if !defined(WIN32)
int nbSkillTraceAlert(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value);
int nbSkillTraceAssert(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value);
#endif


/*
* nbSkillGetFacet is used by the interpreter to lookup a facet for a given skill
*/
NB_Facet *nbSkillGetFacet(NB_Skill *skill,const char *ident){
  NB_Facet *facet;
  if(skill==NULL) return(NULL);
  for(facet=skill->facet;facet && strcmp(facet->ident->value,ident);facet=(NB_Facet *)facet->object.next);
  return(facet);
  }

/* 
* nbSkillFacet is used by node modules to define facets.
*   A facet is a set of skill methods.  A skill can have multiple facets so that a node
*   of a given type (node module skill) can have more than one set of operations.  When a node
*   has only a primary facet, only one type of assert (for example) is supported.  Additional
*   facets can be used to support more types of assertions.  This is true for commands and
*   evalations and other skill methods also. This function is used create a skill facet.
*   
*   We do not expect a facet to be redefined, but it is important that facet identifiers are
*   unique for a given skill.  So this function only creates new facets when the name is unique
*   and can therefore be used to lookup an existing facet.
*/
#if defined(WIN32)
_declspec (dllexport)
#else
extern
#endif
nbCELL nbSkillFacet(nbCELL context,nbCELL skillHandle,const char *ident){
  NB_Skill *skill=(NB_Skill *)skillHandle;
  NB_Facet *facet;
  
  //outMsg(0,'T',"nbSkillFacet: called with ident='%s'",ident);
  if(*ident==0) return((nbCELL)skill->facet); // 2013-12-07 eat - special case for now
  for(facet=skill->facet;facet && strcmp(facet->ident->value,ident);facet=(NB_Facet *)facet->object.next);
  if(!facet){
    facet=nbFacetNew(skill,ident);
    // 2013-12-07 eat - for now we'll keep the primary facet first - change later
    facet->object.next=skill->facet->object.next;
    skill->facet->object.next=(NB_Object *)facet;
    }
  //outMsg(0,'T',"nbSkillFacet: returning facet %p",facet);
  return((nbCELL)facet);
  }

// nbSkillMethod is used by node modules to define skill methods for facets.
#if defined(WIN32)
_declspec (dllexport)
#else
extern
#endif
int nbSkillMethod(nbCELL context,nbCELL facetHandle,int methodId,void *method){
  NB_Facet *facet=(NB_Facet *)facetHandle;
  //outMsg(0,'T',"nbSkillMethod: called with methodId=%d",methodId);
  if(nb_opt_shim){
    if(facet->shim==NULL){
      facet->shim=nbAlloc(sizeof(struct NB_FACET_SHIM));
      memset(facet->shim,0,sizeof(struct NB_FACET_SHIM));
      }
    switch(methodId){
      case NB_NODE_CONSTRUCT: facet->construct=method; break;
      case NB_NODE_DESTROY:   facet->destroy=method;   break;
      case NB_NODE_SHOW:      facet->show=method;      break;
      case NB_NODE_ENABLE:    facet->enable=method;    break;
      case NB_NODE_DISABLE:   facet->disable=method;   break;
      case NB_NODE_ASSERT:    // default alert to assert
#if defined(WIN32)
        facet->assert=method;
        facet->alert=method;
#else
        facet->shim->assert=method; facet->assert=nbSkillTraceAssert;
        facet->shim->alert=method;  facet->alert=nbSkillTraceAlert;
#endif
        break;
      case NB_NODE_EVALUATE:  facet->eval=method;      break;
      case NB_NODE_COMPUTE:   facet->compute=method;   break;
      case NB_NODE_SOLVE:     facet->solve=method;     break;
      case NB_NODE_COMMAND:   facet->command=method;   break;
      case NB_NODE_ALARM:     facet->alarm=method;     break;
      case NB_NODE_ALERT:     facet->alert=method;     break; // undocumented experiment
      default:
        outMsg(0,'L',"nbSkillSetMethod() called with unrecognized methodId - %d",methodId);
        return(-1);
        break;
      }
    }
  else{
    switch(methodId){
      case NB_NODE_CONSTRUCT: facet->construct=method; break;
      case NB_NODE_DESTROY:   facet->destroy=method;   break;
      case NB_NODE_SHOW:      facet->show=method;      break;
      case NB_NODE_ENABLE:    facet->enable=method;    break;
      case NB_NODE_DISABLE:   facet->disable=method;   break;
      case NB_NODE_ASSERT:    facet->assert=method;
                                facet->alert=method;     break; // default alert to assert
      case NB_NODE_EVALUATE:  facet->eval=method;      break;
      case NB_NODE_COMPUTE:   facet->compute=method;   break;
      case NB_NODE_SOLVE:     facet->solve=method;     break;
      case NB_NODE_COMMAND:   facet->command=method;   break;
      case NB_NODE_ALARM:     facet->alarm=method;     break;
      case NB_NODE_ALERT:     facet->alert=method;     break; // undocumented experiment
      default:
        outMsg(0,'L',"nbSkillSetMethod() called with unrecognized methodId - %d",methodId);
        return(-1);
        break;
      }
    }
  //outMsg(0,'T',"nbSkillMethod: returning for methodId=%d",methodId);
  return(0);
  }

// 2013-12-07 eat - nbSkillSetMethod is supported for compatibility with prior releases
//   Although facets were partially implemented long ago internally, a modification to the
//   module API was required to enable modules to specify skill methods for facets.
//   This function becomes an unnecessary shorthand for specifying skill methods for the
//   primary facet. 
#if defined(WIN32)
_declspec (dllexport)
#else
extern
#endif
int nbSkillSetMethod(nbCELL context,nbCELL skillHandle,int methodId,void *method){
  NB_Facet *facet=(NB_Facet *)nbSkillFacet(context,skillHandle,""); // find primary facet
  return(nbSkillMethod(context,(nbCELL)facet,methodId,method));
  }
