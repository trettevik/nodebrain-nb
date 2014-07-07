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
*
* Name:   nb_webster.c
*
* Title:  NodeBrain Webster Module 
*
* Function:
*
*   This program is a NodeBrain skill module that enables the use of
*   a web browser as a client without requiring a full featured web
*   server.
*   
* Description:
* 
*   This module is intended for situations where a more complete interface
*   is not practical.  This would include situations where a web server
*   based interface is not configured or is currently down and a NodeBrain
*   Console is not available.  In the future, it may be used as a way to
*   download a NodeBrain Console to get started.
*
*   Initially Webster will only provide access to the NodeBrain command
*   line interface and web based file editing.
*
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2007-07-27 Ed Trettevik - original prototype version 0.6.8
* 2007-12-25 eat 0.6.8 - Implemented support for CGI scripts
* 2007-12-26 eat 0.6.8 - Using medulla to support concurrent CGI scripts
* 2008-06-15 eat 0.7.0 - Included menu support and file notes
* 2009-02-03 eat 0.7.4 - Stopped looking for TrustedCertificates file if not needed
* 2009-02-15 eat 0.7.5 - Modified for windows
* 2011-05-18 eat 0.8.6 - Moved generic web server functionality out
*            The generic web server functionality is now provided by
*            a NodeBrain Webster API, making it available to other
*            modules needing a web interface.
*
*            Until this new version is fully verified by use, the old
*            version is still available as the "old" skill.  Later,
*            the old version will be removed, eliminating 2/3 of the
*            code in this file.
*
* 2011-10-22 eat 0.8.6 - Included support for server specific config file
*            This enables multiple Webster nodes in a common caboodle
*            to have unique configuration files.  The configuration files
*            probably eventually be replaced by nodebrain variables.
*            However, we still have CGI scripts sharing a common
*            configuration file.
*
* 2011-10-22 eat 0.8.6 - removed old code from before Webster API
* 2012-02-07 dtl 0.8.7  Checker updates
* 2012-04-21 eat 0.8.7  removed some obsolete SSL references
* 2012-08-31 dtl 0.8.12 handled err
* 2012-10-13 eat 0.8.12 Replace malloc/free with nbAlloc/nbFree
* 2012-12-15 eat 0.8.13 Checker updates
* 2013-01-21 eat 0.8.13 Checker updates
* 2013-04-06 eat 0.8.14 Checker updates
*=====================================================================
*/
#include "config.h"
#include <nb/nb.h>

#if defined(WIN32)

#define strcasecmp _stricmp  // map unix strcasecmp to win stricmp

int setenv(char *name,char *value,int option){  // wrap win function
  if(SetEnvironmentVariable(name,value)) return(0);
  else return(-1);
  }

#else

#if !defined(HAVE_SETENV)
int setenv(char *name,char *value,int option){
  char buf[1024];
  char *var;
  sprintf(buf,"%s=%s",name,value);
  var=strdup(buf);
  if(!var){
    nbLogMsg(context,0,'E',"Out of memory - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  return(putenv(var));
  }
#endif

#endif

//==================================================================================

/*
*  Get option specified as string or text cell in context
*
*  name:  Option name
*  value: Default value returned if term not found
*/
static char *getOption(nbCELL context,char *name,char *defaultValue){
  char *value=defaultValue;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL &&
    (cell=nbTermGetDefinition(context,cell))!=NULL &&
    ((nbCellGetType(context,cell)==NB_TYPE_STRING && (value=nbCellGetString(context,cell))) ||
    (nbCellGetType(context,cell)==NB_TYPE_TEXT && (value=nbCellGetText(context,cell)))));
  nbLogMsg(context,0,'T',"%s=%s",name,value);
  value=strdup(value);
  if(!value) nbExit("getOption: out of memory");
  return(value);
  }

/*
*  Get configuration file option.
*
*    The caller is responsible to have option pointing
*    to a default string that can be released with a call
*    to free.
*/
static void getConfigOption(char **option,char *cursor){
  char *delim;
  delim=strchr(cursor,'"');        //find 2nd "
  if(delim){
    *(delim)=0;
    free(*option);
    *option=strdup(cursor);
    if(!*option) nbExit("getConfigOption: out of memory");
    }
  }

//======================================================================================
// Webster Server
// 2011-05-18 eat 0.8.6 - new version based on NodeBrain Webster API
//======================================================================================

typedef struct NB_MOD_WEBSTER{    // Webster Node Structure
  nbCELL           context;       // context term for this node
  struct IDENTITY *identity;      // identity
  char             idName[64];    // identity name
  char             address[16];   // address to bind
  unsigned short   port;          // port to listen on
  int              socket;        // socket we are listening on
  char            *rootdir;       // web site root directory
  char            *authenticate;  // "yes" | "certificate" | "password" | "no"
  char             dir[1024];     // working directory path - NodeBrain caboodle
  char            *cabTitle;      // Caboodle Title (Application)
  char            *cabVersion;    // Caboodle Version
  char            *cabLink;       // Caboodle Link
  char            *cabMenu;       // Caboodle Menu
  nbWebServer     *webserver;     // web server
  } nbWebster;

//======================================================================================
// Handler support functions
//======================================================================================

/*
* Send common heading for webster pages
*
*   Note: this can be enhanced by using nbWebsterPutWhere and nbWebsterProduced calls
*/
static void webHeading(nbCELL context,nbWebSession *session){
  nbWebster *webster=nbWebsterGetHandle(context,session);
  char text[4096];

  nbLogMsg(context,0,'T',"webHeading: called");
  snprintf(text,sizeof(text),
    "<html>\n"
    "<head>\n"
    "<title>%s Webster</title>\n"
    "<link rel='shortcut icon' href='nb.ico'>\n"
    "<link rel='stylesheet' title='webster_style' href='style/webster.css' type='text/css'>\n"
    "<meta http-equiv='Default-Style' content='webster_style'>\n"
    "</head>"
    "<body marginwidth='0' marginheight='0' leftmargin='0' topmargin='0'>\n"
    "<table width='100%%' cellspacing=0 border=0 cellpadding=0 bgcolor='#000000'>\n"
    "<tr><td width='46' valign='middle' bgcolor='#000000'>\n"
    "<table cellpadding='0' bgcolor='#000000'>\n"
    "<tr><td colspan=2><span style='font-size: 2px;'>&nbsp;</span></td></tr>\n"
    "<tr><td><span style='font-size: 6px;'>&nbsp;</span></td><td valign='middle'><img src='/nb32.gif' align='absmiddle' height='32' width='32'></td></tr>\n"
    "<tr><td colspan=2><span style='font-size: 2px;'>&nbsp;</span></td></tr>\n"
    "</table>\n"
    "</td>\n"
    "<td valign='middle'>\n"
    "<table cellpadding=0 cellspacing=0>\n"
    "<tr>\n"
    "<td><span style='font-size: 21px; font-family: arial, sans-serif; color: white'>NodeBrain</td>\n"
    "<td valign='top'><span style='font-size: 8px; color: white'>TM</td>\n"
    "<td valign='bottom'><span style='font-size: 16px; color: white'>Webster<span></td>\n"
    "</tr>\n"
    "</table>\n"
    "</td>\n"
    "<td align='center' valign='middle'>\n"
    "<a class='plain' href='%s'>\n"
    "<span class='cabTitle'>%s<span></a>\n"
    "<span class='cabVersion'>%s<span>\n"
    "</td>\n"
    "<td align='center' valign='middle'>\n"
    "<span style='font-size: 10px; color: white'>\n"
    "C a b o o d l e &nbsp; K i t &nbsp; 0.8.15\n"
    "</span>\n"
    "<span style='font-size: 3px; color: white'><br><br></span>\n"
    "<span style='font-size: 10px; color: white'>\n"
    "%s%s\n"
    "</span>\n"
    "</td>\n"
    "</tr>\n"
    "<tr><td class='navbar' colspan=3>\n"
    "&nbsp; <a href=':page'>Home</a> | "
    "<a href=':menu'>Bookmarks</a> | "
    "<a href=':file'>Directory</a> | "
    "<a href=':nb'>Command</a> | "
    "<a href=':help'>Help</a>\n"
    "<td class='navbar' align='center'>%s</tr>\n"
    "<tr><td height=1 colspan=4 bgcolor='#000000'/></tr>\n"
    "</table><table><tr><td>\n",
    webster->cabTitle,webster->cabLink,webster->cabTitle,webster->cabVersion,session->reqhost,webster->dir,webster->cabMenu);
  *(text+sizeof(text)-1)=0;
  nbLogMsg(context,0,'T',"webHeading: calling nbWebsterPutText len=%d",strlen(text));
  nbWebsterPutText(context,session,text);
  nbLogMsg(context,0,'T',"webHeading: returning");
  }

static void webFooting(nbCELL context,nbWebSession *session){
  static char *html=
   "\n</td></tr></table>\n<hr>\n<span class='foot'>&nbsp;NodeBrain Webster Server</span>\n</body>\n</html>\n";
  nbWebsterPutText(context,session,html);
  }

// Handle Request Error

static void webError(nbCELL context,nbWebSession *session,char *problem,char *value){
  char html[1024];

  sprintf(html,
    "<p><table width='100%%' border='1' bgcolor='pink'>"
    "<tr><td><b>%s</b></td></tr>"
    "<tr><td><pre>%s</pre></td></tr></table>\n",
    problem,value);
  nbWebsterPutText(context,session,html);
  }

// Handle NodeBrain Commands

static void webOutputHandler(nbCELL context,void *session,char *text){
  nbWebsterPutText(context,(nbWebSession *)session,text);
  }

static void webNbCmd(nbCELL context,nbWebSession *session,char *command){
  nbLogFlush(context);
  nbWebsterPutText(context,session,"<pre><font size='+1'>\n");
  // register output handler
  nbLogHandlerAdd(context,session,webOutputHandler);
  // issue command - need to change to use identity associated with certificate
  nbCmd(context,command,1);
  // unregister output handler
  nbLogHandlerRemove(context,session,webOutputHandler);
  nbWebsterPutText(context,session,"</font></pre>\n");
  }

#if defined(WIN32)
static char *webGetFileType(int mode){
  if(mode&_S_IFREG) return("");
  else if(mode&_S_IFDIR) return("dir");
  else return("?");
  }
#else // Unix/Linux
static char *webGetFileType(mode_t mode){
  if(S_ISREG(mode)) return("");
  else if(S_ISDIR(mode)) return("dir");
  else if(S_ISLNK(mode)) return("link");
  else if(S_ISFIFO(mode)) return("pipe");
  else if(S_ISSOCK(mode)) return("socket");
  else if(S_ISCHR(mode)) return("char");
  else if(S_ISBLK(mode)) return("block");
  else return("?");
  }
#endif

static char *webGetLinkedPath(char *html,char *path){
  char *hcur=html,*pcur=path,*delim;

  if(*pcur=='/'){
    *hcur='/';
    hcur++;
    pcur++;
    }
  while((delim=strchr(pcur,'/'))!=NULL){
    *delim=0;
    sprintf(hcur,"<a href=':file?%s'>%s</a>",path,pcur);
    hcur+=strlen(hcur);
    *hcur='/';
    hcur++;
    *delim='/';
    pcur=delim+1;
    }
  if(strlen(pcur)){
    strcpy(hcur,pcur);
    hcur+=strlen(hcur);
    }
  *hcur=0;
  return(html);
  }

// Display a note file

static void webNote(nbCELL context,nbWebSession *session,char *name){
  //char text[4097],*end;
  char text[4097];
  int fildes,len;

  if((fildes=open(name,O_RDONLY))<0){
    sprintf(text,"<b>Open '%s' failed: %s</b>\n",name,strerror(errno));
    nbWebsterPutText(context,session,text);
    return;
    }
  while((len=read(fildes,text,sizeof(text)-1))>0){
    *(text+len)=0;
    //end=text+len;  // 2012-12-15 eat - CID 751630
    //*end=0;
    nbWebsterPutText(context,session,text);
    }
  close(fildes);
  }

// Display a regular file

static void webFile(nbCELL context,nbWebSession *session,char *name){
  //char text[NB_BUFSIZE+1],*end,*filename;
  char text[NB_BUFSIZE+1],*filename;
  char buffer[NB_BUFSIZE],*bufcur,*bufend;
  int fildes,len,maxsub=6;
  unsigned char *cursor;

  filename=name+strlen(name)-1;
  while(filename>name && *filename!='/') filename--;
  if(*filename=='/') filename++;
  sprintf(text,"<p><b>File: %s</b><p><pre>\n",filename);
  nbWebsterPutText(context,session,text);
  if((fildes=open(name,O_RDONLY))<0){
    sprintf(text,"<p><b>Open failed: %s</b>\n",strerror(errno));
    nbWebsterPutText(context,session,text);
    return;
    }
  while((len=read(fildes,text,sizeof(text)-1))>0){
    *(text+len)=0;
    //end=text+len;  // 2012-12-15 eat - CID 751629
    //*end=0;
    cursor=(unsigned char *)text;
    bufcur=buffer;
    bufend=buffer+sizeof(buffer)-maxsub;   // stay back enough to make largest substitution
    while(*cursor){
      if(*cursor>127){
        *bufcur=0;
        nbWebsterPutText(context,session,buffer);
        nbWebsterPutText(context,session,"</pre><p><b>*** File contains binary data. ***</b>\n");
        close(fildes); // 2012-12-15 eat - CID 751619
        return;
        }
      switch(*cursor){
        case '&': strcpy(bufcur,"&amp;"); bufcur+=5; break;
        case '<': strcpy(bufcur,"&lt;"); bufcur+=4; break;
        case '>': strcpy(bufcur,"&gt;"); bufcur+=4; break;
        default:  *bufcur=*cursor; bufcur++;
        }
      cursor++;
      if(bufcur>=bufend){
        *bufcur=0;
        nbWebsterPutText(context,session,buffer);
        bufcur=buffer;
        bufend=buffer+sizeof(buffer)-maxsub;
        }
      }
    *bufcur=0;
    if(*buffer) nbWebsterPutText(context,session,buffer);
    }
  close(fildes);
  nbWebsterPutText(context,session,"</pre>\n");
  }

// Display a directory
//   NOTE: we use opendir,readdir,closedir instead of scandir for better portability

static void webDir(nbCELL context,nbWebSession *session,char *name){
#if defined(WIN32)
  HANDLE dir;
  WIN32_FIND_DATA info;
  char findname[1024];
#else
  DIR *dir;
  struct dirent *ent;
#endif
  struct stat filestat;      // file statistics
  char filesize[16];
  char filetime[128];
  char *filetype;
  struct tm *tm;
  int i,len;
  char text[1024];
  char *class;
  char *filename,*extension;
  int havenote;               // flag indicating we have a note for a file
  char notefile[1024];
  struct FILE_ENTRY{
    struct FILE_ENTRY *next;
    char   name[512];
    };
  struct FILE_ENTRY *rootent;
  struct FILE_ENTRY *curent;
  struct FILE_ENTRY *nextent;
  struct FILE_ENTRY *rootdocent,*curdocent;

  rootent=nbAlloc(sizeof(struct FILE_ENTRY));
  *rootent->name=0;
  rootent->next=NULL;
  rootdocent=nbAlloc(sizeof(struct FILE_ENTRY));
  *rootdocent->name=0;
  rootdocent->next=NULL;
  curdocent=rootdocent;
  filename=name+strlen(name)-1;
  while(filename>name && *filename!='/') filename--;
  if(*filename=='/') filename++;
  sprintf(text,"<p><b>Directory: %s</b>\n",filename);
  nbWebsterPutText(context,session,text);
#if defined(WIN32)
  sprintf(findname,"%s/*",name);
  if((dir=FindFirstFile(findname,&info))==INVALID_HANDLE_VALUE){
    sprintf(text,"<p><b>*** Unable to open directory - errcode=%d ***</b>\n",GetLastError());
#else
  dir=opendir(name);
  if(dir==NULL){
    sprintf(text,"<p><b>*** Unable to open directory - %s ***</b>\n",strerror(errno));
#endif
    nbWebsterPutText(context,session,text);
    return;
    }
#if defined(WIN32)
  do{
    filename=info.cFileName;
#else
  while((ent=readdir(dir))!=NULL){
    filename=ent->d_name;
#endif
    if(*filename=='.'){          // check hidden files for webster notes
      len=strlen(filename);
      if(len>8 && len<sizeof(curdocent->name)+9){
        extension=filename+len-8;
        if(strcmp(extension,".webster")==0){
          curdocent->next=nbAlloc(sizeof(struct FILE_ENTRY));
          curdocent=curdocent->next;
          curdocent->next=NULL;
          strncpy(curdocent->name,filename+1,len-9); // get just "NAME" of ".NAME.webster"
          *(curdocent->name+len-9)=0;  // terminate string
          nbLogMsg(context,0,'T',"Doc entry: %s\n",curdocent->name);
          }
        }
      }
    else{
      nextent=nbAlloc(sizeof(struct FILE_ENTRY));
      strncpy(nextent->name,filename,sizeof(nextent->name)-1);
      *(nextent->name+sizeof(nextent->name)-1)=0;  // make sure we have a null char delimiter when truncated
      for(curent=rootent;curent->next!=NULL && strcasecmp(curent->next->name,filename)<0;curent=curent->next);
      nextent->next=curent->next;
      curent->next=nextent;
      }
#if defined(WIN32)
    } while(FindNextFile(dir,&info));
  FindClose(dir);
#else
        }
  closedir(dir);
#endif
  nbWebsterPutText(context,session,"<p><table cellspacing=1 cellpadding=1><tr align='left'><th>&nbsp</th><th>Modified</th><th align='right'>Size</th><th>Type</th><th>File</th><th>Note</th></tr>\n");
  i=0;
  for(curent=rootent->next;curent!=NULL;curent=curent->next){
    havenote=0;
    sprintf(text,"%s/%s",name,curent->name);
    if(stat(text,&filestat)==0){
      filetype=webGetFileType(filestat.st_mode);
      tm=localtime(&filestat.st_mtime);
      sprintf(filetime,"<table cellpadding=0><tr><td>%4.4d-%2.2d-%2.2d</td><td>&nbsp;%2.2d:%2.2d</td></tr></table>\n",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min);
      sprintf(filesize,"%d",(int)filestat.st_size);
      for(curdocent=rootdocent->next;curdocent!=NULL && strcmp(curent->name,curdocent->name)!=0;curdocent=curdocent->next);
      if(curdocent!=NULL) havenote=1;
      }
    else{
      sprintf(text,"<tr><td colspan=5>Stat: %s</td></tr>\n",strerror(errno));
      nbWebsterPutText(context,session,text);
      filetype="?";
      strcpy(filetime,"yyyy-mm-dd");
      strcpy(filesize,"?");
      }
    i++;
    class=i%2 ? "odd" : "even";  // alternate row background
    sprintf(text,"<tr class='%s'><td>%d</td><td>%s</td><td align='right'>&nbsp;%s</td><td>&nbsp;%s</td><td><a href=':file?%s/%s'>%s</a></td><td>\n",class,i,filetime,filesize,filetype,name,curent->name,curent->name);
    nbWebsterPutText(context,session,text);
    if(havenote){
      sprintf(notefile,"%s/.%s.webster",name,curent->name);
      webNote(context,session,notefile);
      }
    nbWebsterPutText(context,session,"</td></tr>\n");
    }
  nbWebsterPutText(context,session,"</table>\n");
  // free list
  for(curent=rootent->next;curent!=NULL;curent=nextent){
    nextent=curent->next;
    nbFree(curent,sizeof(struct FILE_ENTRY));
    }
  for(curent=rootdocent->next;curent!=NULL;curent=nextent){
    nextent=curent->next;
    nbFree(curent,sizeof(struct FILE_ENTRY));
    }
  }

/*
*  Display help page  - don't want to rely on the configuration for these help pages
*
*    These help pages cover Webster usage topics.  The goal is to provide built-in help sufficient for
*    navigating the user interface.  User's can provide help pages for their own application in at least
*    three different ways: 1) creating bookmarks for help pages, 2) creating help pages linked off the
*    home page, and 3) creating help pages in web content displayed by Webster as a standard web server.
*/

static void webHelpTopic(nbCELL context,nbWebSession *session,char *topic){
  char *html;
  static char *htmlHelp=
    "<p><h1>Help <a href=':help?help'><img src='webster/help.gif' border=0></a></h1>\n"
    "<ul>\n"
    "<li><p><a href=':help?intro'>Introduction</a> - Webster overview\n"
    "<li><p>Menu Options\n"
    "<ul>\n"
    "<li><p><a href=':help?home'>Home</a> - User configured pages\n"
    "<li><p><a href=':help?bookmarks'>Bookmarks</a> - User configured links\n"
    "<li><p><a href=':help?directory'>Directory</a> - File system browsing\n"
    "<li><p><a href=':help?command'>Command</a> - Using NodeBrain commands\n"
    "<li><p><a href=':help?help'>Help</a> - Help facility\n"
    "</ul>\n"
    "<li><p><a href=':help?url'>URL Format</a> - Linking from other applications\n"
    "</ul>\n";
  static char *helpHelp=
    "<p><h1>Help <a href=':help?help'><img src='webster/help.gif' border=0></a> - Help</h1>\n"
    "<p>This is a small help facility internal to the Webster node module. \n"
    "&nbsp;Additional information on Webster is provided in a section of the <i>NodeBrain Module Reference</i> and a section of the <i>NodeBrain Tutorial. \n"
    "&nbsp;For questions on NodeBrain command syntax, see the <i>NodeBrain Language Reference</i>. \n"
    "";
  static char *helpIntro=
    "<p><h1>Help <a href=':help?help'><img src='webster/help.gif' border=0></a> - Introduction</h1>\n"
    "This is a minimal web server for the administration of NodeBrain applications. \n"
    "&nbsp;It enables the use of a web browser to securely perform the following functions. \n"
    "<ol>\n"
    "<li>Poke around the file system to browse logs and rule files.\n"
    "<li>Check application processes using shell commands.\n"
    "<li>Send alerts and assertions to NodeBrain agents.\n"
    "<li>Quickly hop from one agent's Webster node to another.\n"
    "<li>Edit rule files and reload them into agents. (Requires NodeBrain Caboodle Kit)\n"
    "</ol>\n"
    "<p>You may think this functionality is provided by a CGI script on a secure web server. \n"
    "&nbsp;It certaining behaves like a CGI script. &nbsp;Actually, the CGI like functionality is embedded \n"
    "within a small web server. &nbsp;Or perhaps the small web server is embedded within a small program \n"
    "that acts like a CGI script. &nbsp;In any case, your browser is communicating directly with a \n"
    "NodeBrain agent with a plug-in called the \"Webster Module\". \n"
    "&nbsp;We packaged this module with NodeBrain just to make sure everyone has a simple tool for small applications \n"
    "that they can use if they don't already have a better alternative. \n"
    "&nbsp;A NodeBrain application does not depend on the use of this tool. \n"
    "<p>The OpenSSL library is used for X.509 certificate authentication and encryption. \n"
    "&nbsp;Connection and command specific authority is granted by an administrator, which is most likely you. \n"
    "&nbsp;We say this because Webster is designed to be a personal tool, used for small applications. \n"
    "<p>We still anticipate a Java-based NodeBrain Console in the future, but think there may still \n"
    "be cases where a light-weight interface like Webster is more convenient. \n"
    "&nbsp;For example, in response to a rule condition a NodeBrain agent can send an email message containing Webster URL's \n"
    "to an authorized decision maker. \n"
    "&nbsp;The recipient may then simply click on the appropriate URL to notify the NodeBrain agent of the appropriate choice via an alert or assertion. \n"
    "This way the decision maker doesn't have to learn some funny new tool. \n"
    "<p>You can find information about the Webster module in the <i>NodeBrain Module Reference</i> "
    "available at <a href='http://www.nodebrain.org'>NodeBrain.org</a>. \n";
  static char *helpHome=
    "<p><h1>Help <a href=':help?help'><img src='webster/help.gif' border=0></a> - Home</h1>\n"
    "<p>The <i>home</i> option displays a page providing a short description of your NodeBrain application\n"
    "caboodle and may provide links to related pages.  The content of the home page is at <i>caboodle</i>/web/webster/index.ht.\n"
    "This file is a partial HTML file---a part that might logically fall between &lt;body&gt; and &lt;/body&gt;.\n"
    "Webster inserts this HTML file between it's own heading and footing.\n"
    "You may create additional <i>page</i>.ht files and reference them as :page?<i>page</i> in the resource\n"
    "portion of a Webster URL.\n"
    "\n";
  static char *helpBookmarks=
    "<p><h1>Help <a href=':help?help'><img src='webster/help.gif' border=0></a> - Bookmarks</h1>\n"
    "<p>This is a list of links for use with this Webster node. \n"
    "&nbsp;You may manage this list independently on each node, maintain a master list and copy it to each node, \n"
    "identify a master node and only register the master on the other nodes, \n"
    "or ignore this page and keep your links somewhere else. \n"
    "&nbsp;It is a good idea to maintain a complete list on each node. \n"
    "\n";
  static char *helpDirectory=
    "<p><h1>Help <a href=':help?help'><img src='webster/help.gif' border=0></a> - Directory</h1>\n"
    "<p>This page is used to display files. \n"
    "&nbsp;The path to the working directory of the Webster server is shown in the top right portion of the page. \n"
    "&nbsp;This is the starting point for relative path names. \n"
    "&nbsp;There are multiple ways to select a path name. \n"
    "&nbsp;You may enter a path name in the input field below and press enter, \n"
    "select a path by clicking on a link below, or edit the URL in the location field of your browser. \n"
    "\n";
  static char *helpCommand=
    "<p><h1>Help <a href=':help?help'><img src='webster/help.gif' border=0></a> - Command</h1>\n"
    "<p>This is a simple NodeBrain command interpreter interface enabling the use of a web browser as a secure client. \n"
    "&nbsp;Command syntax is described in the <i>NodeBrain Language Reference</i>. \n"
    "&nbsp;Before using this interface you should become familiar with the content of the <i>NodeBrain Tutorial</i>. \n"
    "&nbsp;These and other documents can be found at <a href='http://www.nodebrain.org'>NodeBrain.org</a>. \n";
  static char *helpUrl=
    "<p><big><b>Help  <a href=':help?help'><img src='webster/help.gif' border=0></a> - URL Format</b></big>\n"
    "<p>A Universal Resource Locator (URL) has several parts.\n"
    "The <i>protocol</i>, <i>hostname</i>, and optional <i>port</i> are what brought you to this site. \n"
    "The <i>resource</i> is what brought you to this page. \n"
    "<ul><i>protocol</i>://<i>hostname</i>[:<i>port</i>][/<i>resource</i>[#<i>bookmark</i>]]</ul>\n"
    "<p>Examples:<ul>"
    "<li>http://www.nodebrain.org/license.html"
    "<li>https://myhost.mydomain.com:10443/index.html"
    "</ul>"
    "<p>Webster has built-in resources identified by a colon ':' in the first character. \n"
    "&nbsp;These resources are available only to authenticated users granted special priviledges. \n"
    "&nbsp;Under these conditions the following resource names are recognized. \n"
    "<p><table><tr><th>Resource</th><th>Description</th></tr>\n"
    "<tr class='even'><td>"
    ":bookmark?<br>&nbsp; menu=<i>path</i>&<br>&nbsp; name=<i>name</i>&<br>&nbsp; note=<i>note</i>[&url=<i>url</i>]</td>\n<td>"
    "Request to create a new bookmark. \n"
    "&nbsp;The <i>path</i> must identify an existing menu. \n"
    "&nbsp;If <i>url</i> is not specified, a new menu is created. \n"
    "&nbsp;If <i>url</i> is specified, a new link is created. \n"
    "</td></tr>\n"
    "<tr class='even'><td>"
    ":file[?[arg=]<i>path</i>]</td>\n<td>"
    "Request to view a file specified by </i>pathname</i>. \n"
    "&nbsp;The path may be an absolute path to any location within the file system. \n"
    "&nbsp;You are only restricted by the file permissions of the user account executing Webster. \n"
    "&nbsp;Webster access should only be granted to other users with the understanding that the account becomes a shared account. \n"
    "<p>If <i>path</i> identifies a directory, the directory is displayed with links to the files within it. \n"
    "&nbsp;If <i>path</i> identifies a text file, it is displayed. \n"
    "&nbsp;For any other type of file, only statistics like size, type and last modified time are displayed. \n"
    "</td></tr>\n"
    "<tr class='even'><td>"
    ":help[?[arg=]<i>topic</i>]</td><td>"
    "Request for help on using the Webster interface. \n"
    "&nbsp;If <i>topic</i> is not provided, a top level help page is displayed. \n"
    "&nbsp;You are not expected to know the valid <i>topic</i> values. \n"
    "&nbsp;They are provided by navigating the help system. \n"
    "&nbsp;Start by selecting the 'Help' link from the menu at the top of the page. \n"
    "</td></tr>\n"
    "<tr class='even'><td>"
    ":menu[?[arg=]<i>path</i>]</td>\n<td>"
    "Request to display a menu of bookmarked links. \n"
    "&nbsp;The <i>path</i> is specified relative to the agent's working directory (caboodle). \n"
    "&nbsp;If <i>path</i> is not specified, a configured default path is used. \n"
    "</td></tr>\n"
    "<tr class='even'><td>"
    ":nb[?[arg=]<i>command</i>]</td>\n<td>"
    "Request to execute a NodeBrain command and display the response. \n"
    "&nbsp;The <i>command</i> is issued within the context of the node using the Webster module (plug-in). \n"
    "&nbsp;You may use the '-' command to issue shell commands, if granted permission. \n"
    "&nbsp;This should only be allowed when Webster is running under your account, or a shared account you are authorized to use. \n"
    "</td></tr>\n"
    "<tr class='even'><td>"
    ":page[?[arg=]<i>path</i>]</td>\n<td>"
    "Request for a user created *.ht page. \n"
    "&nbsp;The <i>path</i> must have a '.ht' suffix. \n"
    "&nbsp;If <i>path</i> is not specified, 'home.ht' is assumed. \n"
    "</td></tr>\n"
    "</table>\n"
    "<p>Here are some examples of valid URL's peculiar to Webster that request built-in resources. \n"
    "<ul><li>https://myhost.mydomain.com:10443/:help\n"
    "<li>https://myhost.mydomain.com:12443/:nb?show /c\n"
    "<li>https://myhost.mydomain.com:12443/:nb?assert a=1\n"
    "<li>https://myhost.mydomain.com:9443/:file?/tmp\n"
    "<li>https://myhost.mydomain.com:9443/:file?config\n"
    "<li>https://myhost.mydomain.com:9443/:menu?Bookmarks/Systems\n"
    "</ul>"
    "<p>Webster also allows the use of fully qualified resource names. \n"
    "&nbsp;For obvious security reasons, most servers don't allow this. \n"
    "&nbsp;But Webster is a personal web server. \n"
    "&nbsp;It is an alternative to logging in via SSH. \n"
    "&nbsp;So there is no reason to prevent you from having the same premissions you have using an SSH client. \n"
    "<ul>"
    "<li>https://myhost.mydomain.com:9443//home/fred/htdocs\n"
    "<li>https://myhost.mydomain.com:10443//opt/mypackage/doc/web\n"
    "</ul>"
    "Webster pretends to be a standard web server when resources not starting with ':' are requested. \n"
    "&nbsp;It is used in this way to support NodeBrain related tools, where it is convenient to have \n"
    "a web server dedicated to each NodeBrain configuration (caboodle). \n";
  char *htmlError=
    "<p><h1>Unrecognized Help Topic</h1>"
    "Help is not available on the topic requested. \n"
    "&nbsp;Select 'Help' from the menu above to navigate to the desired topic.\n";
  if(*topic==0) html=htmlHelp;
  else if(strcmp(topic,"help")==0) html=helpHelp;
  else if(strcmp(topic,"intro")==0) html=helpIntro;
  else if(strcmp(topic,"home")==0) html=helpHome;
  else if(strcmp(topic,"bookmarks")==0) html=helpBookmarks;
  else if(strcmp(topic,"directory")==0) html=helpDirectory;
  else if(strcmp(topic,"command")==0) html=helpCommand;
  else if(strcmp(topic,"url")==0) html=helpUrl;
  else html=htmlError;
  nbWebsterPutText(context,session,html);
  }

void webLinkDirRow(nbCELL context,nbWebSession *session,char *class,int row,char *path){
  char text[1024];
  char *name=path+strlen(path)-1;
  while(name>=path && *name!='/') name--;
  name++;
  snprintf(text,sizeof(text),"<tr class='%s'><td>%d</td><td><table cellspacing=0 cellpadding=0><tr><td><img src='webster/folder.gif'></td><td>&nbsp;<a href=':menu?%s'>%s</a></td></tr></table></td><td>\n",class,row,path,name);
  nbWebsterPutText(context,session,text);
  snprintf(text,sizeof(text),"%s/webster/%s/.note",session->webster->rootdir,path);
  webNote(context,session,text);
  nbWebsterPutText(context,session,"</td></tr>\n");
  }

#if defined(WIN32)
static void webLinkDir(nbCELL context,nbWebSession *session,char *path){
  }

#else  // Unix/Linux

// Display a link directory
static void webLinkDir(nbCELL context,nbWebSession *session,char *path){
  nbWebster *webster=(nbWebster *)session->webster->handle;
  DIR *dir;
  struct dirent *ent;
  struct stat filestat;      // file statistics
  int i,len;
  char text[NB_BUFSIZE];
  char *class,*cursor,*delim;
  struct FILE_ENTRY{
    struct FILE_ENTRY *next;
    char   name[512];
    };
  struct FILE_ENTRY *rootent;
  struct FILE_ENTRY *curent;
  struct FILE_ENTRY *nextent;

  sprintf(text,"%s/webster/%s",webster->rootdir,path);
  dir=opendir(text);
  if(dir==NULL){
    sprintf(text,"<p><b>*** Unable to open directory - %s ***</b>\n",strerror(errno));
    nbWebsterPutText(context,session,text);
    return;
    }
  rootent=nbAlloc(sizeof(struct FILE_ENTRY));
  *rootent->name=0;
  rootent->next=NULL;
  while((ent=readdir(dir))!=NULL){
    if(*ent->d_name!='.'){
      nextent=nbAlloc(sizeof(struct FILE_ENTRY));
      strncpy(nextent->name,ent->d_name,sizeof(nextent->name)-1);
      *(nextent->name+sizeof(nextent->name)-1)=0;  // make sure we have a null char delimiter when truncated
      for(curent=rootent;curent->next!=NULL && strcasecmp(curent->next->name,ent->d_name)<0;curent=curent->next);
      nextent->next=curent->next;
      curent->next=nextent;
      }
    }
  closedir(dir);
  nbWebsterPutText(context,session,"<p><table cellspacing=1 cellpadding=2 width='100%'><tr align='left'><th width='30'>&nbsp</th><th with=10%>Name</th><th>Note</th></tr>\n");
  cursor=path;
  i=0;
  while((delim=strchr(cursor,'/'))!=NULL){
    len=delim-path;
    strncpy(text,path,len);
    *(text+len)=0;
    //class=i%2 ? "odd" : "even";  // alternate row background
    webLinkDirRow(context,session,"odd",i,text);
    cursor=delim+1;
    i++;
    }
  webLinkDirRow(context,session,"marker",i,path);
  //websterPut(context,session,"<tr><th colspan=3><span style='font-size: 3px;'>&nbsp;</span></th></tr>\n");
  //i=0;
  for(curent=rootent->next;curent!=NULL;curent=curent->next){
    sprintf(text,"%s/webster/%s/%s",webster->rootdir,path,curent->name);
    if(stat(text,&filestat)!=0){
      sprintf(text,"<tr><td colspan=5>Stat: %s</td></tr>\n",strerror(errno));
      nbWebsterPutText(context,session,text);
      }
    i++;
    //class=i%2 ? "odd" : "even";  // alternate row background
    class="even";
    if(S_ISDIR(filestat.st_mode)){
      sprintf(text,"<tr class='%s'><td>%d</td><td><table cellspacing=0 cellpadding=0><tr><td><img src='webster/folder.gif'></td><td>&nbsp;<a href=':menu?%s/%s'>%s</a></td></tr></table></td><td>\n",class,i,path,curent->name,curent->name);
      nbWebsterPutText(context,session,text);
      sprintf(text,"%s/webster/%s/%s/.note",webster->rootdir,path,curent->name);
      webNote(context,session,text);
      nbWebsterPutText(context,session,"</td></tr>\n");
      }
    else if(S_ISREG(filestat.st_mode)){
      int fildes;
      sprintf(text,"<tr class='%s'><td>%d</td>",class,i);
      nbWebsterPutText(context,session,text);
      sprintf(text,"%s/webster/%s/%s",webster->rootdir,path,curent->name);
      if((fildes=open(text,O_RDONLY))<0){
        sprintf(text,"<td colspan=2>Unable to open: \"%s\" - %s</td></tr>\n",curent->name,strerror(errno));
        nbWebsterPutText(context,session,text);
        }
      else{
        len=read(fildes,text,sizeof(text)-1);
        if(len<0){
          sprintf(text,"<td colspan=2>Unable to read: \"%s\" - %s</td></tr>\n",curent->name,strerror(errno));
          nbWebsterPutText(context,session,text);
          }
        else if(len>0) *(text+len)=0;
        if(len>0 && (cursor=strchr(text,'\n'))!=NULL){
          *cursor=0;
          nbWebsterPutText(context,session,"<td><table cellspacing=0 cellpadding=0><tr><td><img src='webster/link.gif'></td><td>&nbsp;<a href='");
          nbWebsterPutText(context,session,text);
          nbWebsterPutText(context,session,"'>");
          nbWebsterPutText(context,session,curent->name);
          nbWebsterPutText(context,session,"</a></td></tr></table></td><td>\n");
          cursor++;
          nbLogMsg(context,0,'T',"cursor:%s\n",cursor);
          if(*cursor!=0) nbWebsterPutText(context,session,cursor);
          while((len=read(fildes,text,sizeof(text)-1))>0){
            *(text+len)=0;
            nbWebsterPutText(context,session,text);
            }
          nbWebsterPutText(context,session,"</td></tr>\n");
          }
        else{
          sprintf(text,"<td colspan=2>Unable to parse: \"%s\"</td></tr>\n",curent->name);
          nbWebsterPutText(context,session,text);
          }
        close(fildes);
        }
      }
    }
  nbWebsterPutText(context,session,"<tr><th colspan=3><span style='font-size: 3px;'>&nbsp;</span></th></tr></table>\n");
  // free list
  for(curent=rootent->next;curent!=NULL;curent=nextent){
    nextent=curent->next;
    nbFree(curent,sizeof(struct FILE_ENTRY));
    }
  nbFree(rootent,sizeof(struct FILE_ENTRY));  // 2012-12-25 eat - AST 43
  }
#endif

static void webLink(nbCELL context,nbWebSession *session,char *path){
  char *head=
    "<p><h1>Bookmarks <a href=':help?bookmarks'><img src='webster/help.gif' border=0></a></h1>\n"
    "<p><table>\n";
  char *formHead=
    "</table>\n<table><tr><td>\n"
    "<p><form name='bookmark' action=':bookmark' method='post'>\n"
    "<input type='hidden' name='menu' value='";
  char *formTail=
    "'>\n<table>\n"
    "<tr><th>Name</th><td><input type='text' name='name' size='60' title='Enter name to appear in bookmark menu.'></td></tr>\n"
    "<tr><th>Note</th><td><input type='text' name='note' size='60' title='Enter short descriptive note.'></td></tr>\n"
    "<tr><th>URL</th><td><input type='text' name='url'  size='60' title='Enter optional URL.  Use copy and paste to avoid typos.'></td></tr>\n"
    "</table>\n"
    "<p><input type='submit' value='Bookmark'>\n"
    "</form></td></tr></table>\n";
  nbWebsterPutText(context,session,head);
  if(*path==0) path="Bookmarks"; // this should be a configurable option
  webLinkDir(context,session,path);
  nbWebsterPutText(context,session,formHead);
  nbWebsterPutText(context,session,path);
  nbWebsterPutText(context,session,formTail);
  }


//======================================================================================
// Handlers
//======================================================================================

static int webMenu(nbCELL context,nbWebSession *session,void *handle){
  char *menu;

  nbLogMsg(context,0,'T',"webMenu: called");
  webHeading(context,session);
  menu=nbWebsterGetParam(context,session,"menu");
  if(!menu) menu=nbWebsterGetParam(context,session,"arg");
  if(!menu) menu=nbWebsterGetQuery(context,session);
  webLink(context,session,menu);
  webFooting(context,session);
  nbLogMsg(context,0,'T',"webMenu: returning");
  return(0);
  }

static int webIsMenu(const char *word){
  const char *cursor=word;
  while(*cursor && (isalnum(*cursor) || *cursor=='/')) cursor++;
  if(*cursor) return(0);
  return(1);
  }

static int webIsName(const char *word){
  const char *cursor=word;
  while(*cursor && isalnum(*cursor)) cursor++;
  if(*cursor) return(0);
  return(1);
  }

static int webBookmark(nbCELL context,nbWebSession *session,void *handle){
  char filename[1024];
  char *menu,*name,*note,*url;
  struct stat filestat;      // file statistics
  FILE *file;

  nbLogMsg(context,0,'T',"webBookmark: called");
  webHeading(context,session);
  menu=nbWebsterGetParam(context,session,"menu");
  if(!menu) menu="";
  name=nbWebsterGetParam(context,session,"name");
  if(!name) name="";
  note=nbWebsterGetParam(context,session,"note");
  if(!note) note="";
  url=nbWebsterGetParam(context,session,"url");
  if(!url) url="";
  if(!webIsMenu(menu) || !webIsName(name)){  // 2013-01-21 eat - VID 5591,5609-0.8.13-5 - scrub input impacting file name
    webError(context,session,"Menu and name must be alphanumeric","");
    return(0); 
    }
  nbLogMsg(context,0,'T',"bookmark: menu='%s',name='%s',note='%s',url='%s'",menu,name,note,url);
  // check for existing entry
  sprintf(filename,"%s/webster/%s/%s",session->webster->rootdir,menu,name);
  if(stat(filename,&filestat)==0) webError(context,session,"Bookmark already defined.",filename);
  else{
    if(*url==0){  // create directory
#if defined(WIN32)
          if(mkdir(filename)==0){
#else
      if(mkdir(filename,S_IRWXU)==0){
#endif
        strcat(filename,"/.note");
        if((file=fopen(filename,"w"))==NULL) webError(context,session,"Unable to open bookmark file.",filename);
        else{
          fprintf(file,"%s\n",note);
          fclose(file);
          }
        }
      else webError(context,session,"Unable to create bookmark folder.",filename);
      }
    else{ // create file
      if((file=fopen(filename,"w"))==NULL) webError(context,session,"Unable to open bookmark file.",filename);
      else{
        fprintf(file,"%s\n",url);
        fprintf(file,"%s\n",note);
        fclose(file);
        }
      }
    }
  webLink(context,session,menu);
  webFooting(context,session);
  nbLogMsg(context,0,'T',"webBookmark: returning");
  return(0);
  }

// Display directory or file

static int webPath(nbCELL context,nbWebSession *session,void *handle){
  nbWebster *webster=(nbWebster *)handle;
  struct stat filestat;
  char text[NB_BUFSIZE],whtml[NB_BUFSIZE];
  char filesize[16];
  char filetime[17];
  char *filetype;
  struct tm *tm;
  char *name;

  //nbLogMsg(context,0,'T',"webPath: called");
  webHeading(context,session);
  //nbLogMsg(context,0,'T',"webPath: back from heading");
  name=nbWebsterGetParam(context,session,"name");
  if(!name) name=nbWebsterGetParam(context,session,"arg");
  if(!name) name=nbWebsterGetQuery(context,session);
  if(!name) name=webster->dir;
  else if(!*name) name=webster->dir; // 2012-12-15 eat - CID 751555
  snprintf(text,sizeof(text),
    "<p><h1>Directory <a href=':help?directory'><img src='webster/help.gif' border=0></a></h1>\n"
    "<p><form name='file' action=':file' method='get'>\n"
    "<input type='text' name='arg' size='120' value='%s' title='Enter path and press enter key.'></form>\n",
    name);
  nbWebsterPutText(context,session,text);

  if(stat(name,&filestat)!=0){
    webError(context,session,"File not found.",name);
    return(0);
    }

  // display statistics

  filetype=webGetFileType(filestat.st_mode);
  if(!*filetype) filetype="regular";
  tm=localtime(&filestat.st_mtime);
  // 2013-04-06 eat - VID 7073-Plugin-1.1.0-4-R612 - removed newline character from format string
  snprintf(filetime,sizeof(filetime),"%4.4d-%2.2d-%2.2d %2.2d:%2.2d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min);
  snprintf(filesize,sizeof(filesize),"%d",(int)filestat.st_size);
  snprintf(text,sizeof(text),
    "<p><table>"
    "<tr><th>Modified</th><th>Size</th><th>Type</th><th>Path</th></tr>\n"
    "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n"
    "</table>",
    filetime,filesize,filetype,webGetLinkedPath(whtml,name));
  nbWebsterPutText(context,session,text);

// Display contents of regular files and directories
#if defined(WIN32)
  if(filestat.st_mode&_S_IFREG) webFile(context,session,name);
  else if(filestat.st_mode&_S_IFDIR) webDir(context,session,name);
#else
  if(S_ISREG(filestat.st_mode)) webFile(context,session,name);
  else if(S_ISDIR(filestat.st_mode)) webDir(context,session,name);
#endif
  webFooting(context,session);
  nbLogMsg(context,0,'T',"webPath: returning");
  return(0);
  }

static int webCommand(nbCELL context,nbWebSession *session,void *handle){
  char plain[1024];
  char encoded[2048];
  char text[NB_BUFSIZE];
  char *cmd=NULL;
  static char *html=
    "<p><h1>Command <a href=':help?command'><img src='webster/help.gif' border=0></a></h1>\n"
    "<p><form name='nb' action=':nb' method='post'>\n"
    "<input tabindex='1' type='text' name='cmd' size='120' title='Enter NodeBrain Command'></form>\n"
    "<script type='text/javascript'>document.nb.cmd.focus();</script>\n"
    "";

  nbLogMsg(context,0,'T',"webCmd: called");
  // role=nbWebsterGetRole(context,session);  // get user role
  // if(!role || strcmp(role,"admin")){
  //   return(webAuthError(context,session));
  //   }
  //nbWebsterGetQuery(context,session);
  webHeading(context,session);
  //nbWebsterGetQuery(context,session);
  cmd=nbWebsterGetParam(context,session,"cmd");
  //nbWebsterGetQuery(context,session);
  if(!cmd) cmd=nbWebsterGetParam(context,session,"arg");
  // 2011-10-23 eat - we need an option for decoding
  if(!cmd){
    char *queryString;
    queryString=nbWebsterGetQuery(context,session);
    if(queryString) cmd=nbWebsterParameterDecode(context,session,queryString,plain,sizeof(plain));
    }
  nbWebsterPutText(context,session,html);
  if(cmd && *cmd){
    char *queryString=nbWebsterParameterEncode(context,session,cmd,encoded,sizeof(encoded));
    if(queryString){
      sprintf(text,"<a href=':nb?%s'>%s</a>\n",queryString,cmd);  // cmd should be html encoded
      nbWebsterPutText(context,session,text);
      webNbCmd(context,session,cmd);
      }
    }
  webFooting(context,session);
  nbLogMsg(context,0,'T',"webCommand: returning");
  return(0);
  }

static int webPage(nbCELL context,nbWebSession *session,void *handle){
  nbWebster *webster=(nbWebster *)handle;
  char text[NB_BUFSIZE];
  char *path;
  int fildes,len;
  char filepath[1024];

  nbLogMsg(context,0,'T',"webPage: called");
  webHeading(context,session);
  path=nbWebsterGetParam(context,session,"path");
  if(!path) path=nbWebsterGetParam(context,session,"arg");
  if(!path) path=nbWebsterGetQuery(context,session); 
  nbWebsterPutText(context,session,"<p>\n");
  if(*path==0) path="index.ht";
  snprintf(filepath,sizeof(filepath),"%s/webster/%s",webster->rootdir,path);  // subdirectory should be configurable
  if((fildes=open(filepath,O_RDONLY))<0){
    webError(context,session,"Unable to open page content file.",filepath);
    webHelpTopic(context,session,"home");
    return(0);
    }
  while((len=read(fildes,text,sizeof(text)-1))>0){
    *(text+len)=0;
    nbWebsterPutText(context,session,text);
    }
  close(fildes);
  webFooting(context,session);
  nbLogMsg(context,0,'T',"webPage: returning");
  return(0);
  }

/*
*  Display help page  - see webHelpTopic function for text.
*/
static int webHelp(nbCELL context,nbWebSession *session,void *handle){
  char *topic;
  nbLogMsg(context,0,'T',"webHelp: called");
  webHeading(context,session);
  topic=nbWebsterGetParam(context,session,"topic");
  if(!topic) topic=nbWebsterGetParam(context,session,"arg");
  if(!topic) topic=nbWebsterGetQuery(context,session);
  webHelpTopic(context,session,topic);
  webFooting(context,session);
  nbLogMsg(context,0,'T',"webHelp: returning");
  return(0);
  }

//======================================================================================
// Methods using Webster API
//======================================================================================

/*
*  construct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*
*    define webwebster node https.webster("<identity>@<address>:port");
*/
static void *websterConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbWebster *webster;
  nbCELL cell=NULL;
  nbSET argSet;
  char *delim;
  char *id="default";  // use default identity by default

  nbLogMsg(context,0,'T',"websterConstruct: called");
  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell){
    if(nbCellGetType(context,cell)!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"Expecting identity name as first parameter");
      return(NULL);
      }
    id=nbCellGetString(context,cell);
    }
  webster=nbAlloc(sizeof(nbWebster));
  memset(webster,0,sizeof(nbWebster));
  webster->context=context;
  strncpy(webster->idName,id,sizeof(webster->idName));
  *(webster->idName+sizeof(webster->idName)-1)=0;
  webster->identity=nbIdentityGet(context,webster->idName);
  if(webster->identity==NULL){
    nbLogMsg(context,0,'E',"Identity '%s' not defined",webster->idName);
    nbFree(webster,sizeof(nbWebster));
    return(NULL);
    }
  webster->webserver=nbWebsterOpen(context,context,webster,NULL);
  if(!webster->webserver){
    nbLogMsg(context,0,'E',"Unable to open web server");
    nbFree(webster,sizeof(nbWebster));
    return(NULL);
    }
  //if(getcwd(webster->dir,sizeof(webster->dir))<0) *webster->dir=0;  // 2012-12-15 eat - CID 751514
  if(getcwd(webster->dir,sizeof(webster->dir))==NULL) *webster->dir=0;
  delim=webster->dir;
  while((delim=strchr(delim,'\\'))!=NULL) *delim='/',delim++; // Unix the path
  webster->rootdir=strdup(webster->dir);
  if(!webster->rootdir){ //2012-08-31 dtl handled err
    nbLogMsg(context,0,'E',"Out of memory - terminating");
    exit(NB_EXITCODE_FAIL);
    } 
  // 2012-12-30 eat 0.8.13 - changed to pointers obtained using getOption in websterEnable
  //strcpy(webster->cabTitle,"MyCaboodle");
  //*webster->cabVersion=0;
  //strcpy(webster->cabLink,"http://nodebrain.org");
  //strcpy(webster->cabMenu,"<a href=':page'>Webster</a>");
  nbCellDrop(context,cell);
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  nbLogMsg(context,0,'T',"websterConstruct: returning");
  return(webster);
  }

static int websterEnable(nbCELL context,void *skillHandle,nbWebster *webster){
  //char rootdir[2048];
  FILE *configFile;
  char *configName;
  char buffer[1024];
  int rc;

  webster->webserver=nbWebsterOpen(context,context,webster,NULL);
  if(!webster->webserver){
    nbLogMsg(context,0,'E',"Unable to open web server");
    return(-1);
    }
  // Register Resources
  nbWebsterRegisterResource(context,webster->webserver,":page",webster,webPage);
  nbWebsterRegisterResource(context,webster->webserver,":bookmark",webster,webBookmark);
  nbWebsterRegisterResource(context,webster->webserver,":menu",webster,webMenu);
  nbWebsterRegisterResource(context,webster->webserver,":file",webster,webPath);
  nbWebsterRegisterResource(context,webster->webserver,":nb",webster,webCommand);
  nbWebsterRegisterResource(context,webster->webserver,":help",webster,webHelp);
  // get options
  //webster->rootdir=strdup(getOption(context,"DocumentRoot","web"));
  //if(!webster->rootdir) nbExit("websterEnable: Out of memory - terminating");
  //if(*webster->rootdir!='/'){
  //  sprintf(rootdir,"%s/%s",webster->dir,webster->rootdir);
  //  free(webster->rootdir);
  //  webster->rootdir=strdup(rootdir);
  //  if(!webster->rootdir) nbExit("websterEnable: Out of memory - terminating");
  //  }
  //nbLogMsg(context,0,'T',"DocumentRoot=%s",webster->rootdir);
  rc=nbWebsterEnable(context,webster->webserver);
  if(rc){
    nbLogMsg(context,0,'E',"Unable to enable web server");
    return(-1);
    }
  if(webster->rootdir) free(webster->rootdir); // Get new DocumentRoot
  webster->rootdir=nbWebsterGetRootDir(context,webster->webserver);
  if(!webster->rootdir){
    nbLogMsg(context,0,'E',"Unable to get DocumentRoot");
    return(-1);
    }
  // Load configuration
  webster->cabTitle=getOption(context,"Title","MyCaboodle");
  webster->cabVersion=getOption(context,"Version","");
  webster->cabLink=getOption(context,"Link","https://nodebrain.org");
  webster->cabMenu=getOption(context,"Menu","<a href=':page'>Webster</a>");
  configName=nbWebsterGetConfig(context,webster->webserver);
  nbLogMsg(context,0,'T',"websterEnable: configName=%s",configName);
  if(!configName || !*configName) configName="config/caboodle.conf";
  nbLogMsg(context,0,'T',"websterEnable: configName=%s",configName);
  if((configFile=fopen(configName,"r"))!=NULL){
    while(fgets(buffer,sizeof(buffer),configFile)){
      if(strncmp(buffer,"Title=\"",7)==0) getConfigOption(&webster->cabTitle,buffer+7);
      else if(strncmp(buffer,"Version=\"",9)==0) getConfigOption(&webster->cabVersion,buffer+9);
      else if(strncmp(buffer,"Link=\"",6)==0) getConfigOption(&webster->cabLink,buffer+6);
      else if(strncmp(buffer,"Menu=\"",6)==0) getConfigOption(&webster->cabMenu,buffer+6);
      }
    fclose(configFile);
    }
  return(rc);
  }

/*
*  disable method
*
*    disable <node>
*/
static int websterDisable(nbCELL context,void *skillHandle,nbWebster *webster){
  if(webster->webserver) nbWebsterDisable(context,webster->webserver);
  if(webster->rootdir) free(webster->rootdir);
  if(webster->cabTitle) free(webster->cabTitle);
  if(webster->cabVersion) free(webster->cabVersion);
  if(webster->cabLink) free(webster->cabLink);
  if(webster->cabMenu) free(webster->cabMenu);
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*
*/
static int websterDestroy(nbCELL context,void *skillHandle,nbWebster *webster){
  nbLogMsg(context,0,'T',"websterDestroy called");
  if(webster->webserver) nbWebsterClose(context,webster->webserver);
  nbFree(webster,sizeof(nbWebster));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *websterBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,websterConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,websterDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,websterEnable);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,websterDestroy);
  return(NULL);
  }
