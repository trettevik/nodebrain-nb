/*
* Copyright (C) 2007-2009 The Boeing Company
*                         Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
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
*   webster.
*   
* Description:
* 
*   This module is intended for situations where a more complete interface
*   is not practical.  This would include situations where a web webster
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
*=====================================================================
*/
#include "config.h"
#include <nb.h>

#include <openssl/ssl.h>
#include <openssl/x509.h>

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
  sprintf(buf,"%s=%s",name,value);
  return(putenv(strdup(buf)));
  }
#endif

#endif

//=============================================================================
 
typedef struct NB_WEBSTER_URL{    // Webster URL Structure
  struct NB_TREE_NODE node;       // Binary tree node
  char             title[256];
  char             url[512];
  char             comment[1024];
  } nbURL;

typedef struct NB_WEBSTER_USER{   // Webster User Structure
  struct NB_TREE_NODE node;       // Binary tree node
  char             userid[32];    // user identifier
  char             role;          // 0 - read only, 1 - cgi user, 2 - administrator
  } nbUser;

typedef struct NB_MOD_WEBSTER{    // Webster Node Structure
  nbCELL           context;       // context term for this node
  struct IDENTITY *identity;      // identity
  char             idName[64];    // identity name
  char             address[16];   // address to bind
  unsigned short   port;          // port to listen on
  int              socket;        // socket we are listening on
  char            *rootdir;       // web site root directory
  char            *authenticate;  // "yes" | "certificate" | "password" | "no"
  SSL_CTX         *ctx;           // SSL context
  char             dir[1024];     // working directory path - NodeBrain caboodle
  nbUser          *userTree;      // List of authorized users
  char             cabTitle[64];  // Caboodle Title (Application)
  char             cabVersion[64];// Caboodle Version
  char             cabLink[256];  // Caboodle Link
  char             cabMenu[1024]; // Caboodle Menu
  } nbWebster;

typedef struct NB_WEBSTER_SESSION{// Webster Session Structure
  nbWebster       *webster;       // webster node structure
  NB_IpChannel    *channel;       // borrow from NBP
#if defined(WIN32)
  BIO             *sbio;          // socket bio (because windows socket is not fildes)
#endif
  SSL             *ssl;           // SSL object
  nbPROCESS        process;        // CGI process
  unsigned char    role;          // user role - see NB_WEBSTER_ROLE_* below
  char             method;        // request method - see NB_WEBSTER_METHOD_* below     
  char             keepAlive;     // keep connection for future request     
  char            *resource;      // requested resource - points into request
  char            *headerfields;  // http message header fields - points into request
  int              contentLength; // content length for CGI Post      
  char            *content;       // content for CGI Post
  char             reqcn[128];    // X509 certificate common name for valid certificate
  char             reqhost[512];  // request host
  char             reqauth[512];  // request authentication (basic - base64 encoded "user:password"
  char             page[NB_BUFSIZE];    // page buffer
  char            *pageend;       // end of buffer  - points to last byte in page
  char            *pagecur;       // page cursor    - points into page
  char             userid[32];    // user id
  char             request[NB_BUFSIZE]; // request buffer
  char             command[NB_BUFSIZE]; // medulla command buffer
  } nbSession;

#define NB_WEBSTER_ROLE_REJECT 0
#define NB_WEBSTER_ROLE_GUEST  1
#define NB_WEBSTER_ROLE_ADMIN  255

#define NB_WEBSTER_METHOD_GET   1
#define NB_WEBSTER_METHOD_POST  2

//==================================================================================
// Create new webster structure from webster specification
//
//    identity@address:port

static nbWebster *websterNew(nbCELL context,char *cursor,char *msg){
  nbWebster *webster;
  char *inCursor,*delim;
  FILE *configFile;
  char buffer[1024];

  webster=malloc(sizeof(nbWebster));
  webster->context=context;
  inCursor=webster->idName;
  while(*cursor==' ') cursor++;
  while(*cursor && *cursor!='@'){
    *inCursor=*cursor;
    inCursor++;
    cursor++;
    }
  *inCursor=0;
  if(*cursor!='@'){
    sprintf(msg,"Identity not found in webster specification - expecting identity@address:port");
    free(webster);
    return(NULL);
    }
  cursor++;
  webster->identity=nbIdentityGet(context,webster->idName);
  if(webster->identity==NULL){
    sprintf(msg,"Identity '%s' not defined",webster->idName);
    free(webster);
    return(NULL);
    }
  inCursor=webster->address; 
  while(*cursor && *cursor!=':'){
    *inCursor=*cursor; 
    inCursor++;
    cursor++;
    } 
  *inCursor=0;
  if(*cursor!=':'){
    sprintf(msg,"Address not found in webster specification - expecting identity@address:port");
    free(webster);
    return(NULL);
    }
  *cursor++;
  inCursor=cursor;
  while(*cursor>='0' && *cursor<='9') cursor++;
  if(*cursor!=0){
    sprintf(msg,"Port not numeric in webster specification - expecting identity@address:port");
    free(webster);
    return(NULL);
    }
  webster->port=atoi(inCursor);
  webster->socket=0;
  if(getcwd(webster->dir,sizeof(webster->dir))<0) *webster->dir=0;
  delim=webster->dir;
  while((delim=strchr(delim,'\\'))!=NULL) *delim='/',delim++; // Unix the path
  webster->rootdir=strdup(webster->dir);
  webster->ctx=NULL;
  webster->userTree=NULL;
  strcpy(webster->cabTitle,"MyCaboodle");
  *webster->cabVersion=0;
  strcpy(webster->cabLink,"http://nodebrain.org");
  strcpy(webster->cabMenu,"<a href=':page'>Webster</a>");
  if((configFile=fopen("config/caboodle.conf","r"))!=NULL){
    while(fgets(buffer,sizeof(buffer),configFile)){
      if(strncmp(buffer,"Title=\"",7)==0){
        delim=strchr(buffer+7,'"');
        if(delim && delim-buffer-7<sizeof(webster->cabTitle)) *delim=0,strcpy(webster->cabTitle,buffer+7);
        }
      else if(strncmp(buffer,"Version=\"",9)==0){
        delim=strchr(buffer+9,'"');
        if(delim && delim-buffer-9<sizeof(webster->cabVersion)) *delim=0,strcpy(webster->cabVersion,buffer+9);
        }
      else if(strncmp(buffer,"Link=\"",6)==0){
        delim=strchr(buffer+6,'"');
        if(delim && delim-buffer-6<sizeof(webster->cabLink)) *delim=0,strcpy(webster->cabLink,buffer+6);
        }
      else if(strncmp(buffer,"Menu=\"",6)==0){
        delim=strchr(buffer+6,'"');
        if(delim && delim-buffer-6<sizeof(webster->cabMenu)) *delim=0,strcpy(webster->cabMenu,buffer+6);
        }
      }
    fclose(configFile);
    }
  return(webster); 
  }

//========================================================================================
// Connection service routines

static int clearRead(int socket,char *buffer,size_t size){
  int len;
  len=recv(socket,buffer,size,0);
  while(len==-1 && errno==EINTR) len=recv(socket,buffer,size,0);
  return(len);
  }

static int clearWrite(int socket,char *buffer,size_t size){
  int len;
  len=send(socket,buffer,size,0);
  while(len==-1 && errno==EINTR) len=send(socket,buffer,size,0);
  return(len);
  }

// Send text to peer.
// 
static int websterWriteStr(nbCELL context,nbSession *session,char *buffer){
  int sent;
  if(session->ssl!=NULL) sent=SSL_write(session->ssl,buffer,strlen(buffer));
  else sent=clearWrite(session->channel->socket,buffer,strlen(buffer));
  return(sent);
  }

static void websterPut(nbCELL context,nbSession *session,char *text){
  if(text==NULL || strlen(text)>=session->pageend-session->pagecur){
    if(*session->page) websterWriteStr(context,session,session->page);
    session->pagecur=session->page;
    *session->page=0;
    }
  if(text!=NULL){
    strcpy(session->pagecur,text);
    session->pagecur+=strlen(session->pagecur); 
    }
  }

// Put chunked text - each chunk is prefixed with a length and a final chuck of 0 indicated the end.
static void websterPutChunk(nbCELL context,nbSession *session,char *text){
  int len=strlen(text);
  char chunksize[8];
  
  if(len==0){
    websterPut(context,session,"0\r\n\r\n");
    websterPut(context,session,NULL);
    return;
    }
  sprintf(chunksize,"%x\r\n",len);
  websterPut(context,session,chunksize);
  websterPut(context,session,text);
  }

static int websterRead(nbCELL context,nbSession *session){
  int len;
  NB_IpChannel *channel=session->channel;
  char *cursor,*buffer=(char *)channel->buffer;

  if(session->ssl!=NULL) len=SSL_read(session->ssl,buffer,sizeof(channel->buffer));
  else len=clearRead(session->channel->socket,buffer,sizeof(channel->buffer));
  if(len<0) return(len);
  if(len>NB_BUFSIZE-1) return(-1); /* that's too big */
  cursor=buffer+len;
  *cursor=0; cursor--;
  // why do we do this here?
  if(*cursor==10){*cursor=0; cursor--; len--;}
  if(*cursor==13){*cursor=0; cursor--; len--;}
  return(len);
  }

// Decode the command
// Returns: 0 - success, -1 error
//   
static int websterDecodeRequest(nbCELL context,nbSession *session,char *request,int reqlen){
  char *hexalpha="0123456789abcdefABCDEF";
  char *nibble;
  unsigned char n,l;
  char *reqcur=session->request;
  char *delim;
  int len;
  char *cursor=request;

  // initialize side effect values in case we bail out on error
  *reqcur=0;
  *session->reqauth=0;
  strcpy(session->reqhost,"?");
  // decode the URL
  if(strncmp(cursor,"GET /",5)==0){
    len=5;
    session->method=NB_WEBSTER_METHOD_GET;
    }
  else if(strncmp(cursor,"POST /",6)==0){
    len=6;
    session->method=NB_WEBSTER_METHOD_POST;
    }
  else return(1);
  strncpy(reqcur,cursor,len);
  session->resource=reqcur+len;  // save address of resource portion
  reqcur+=len;
  cursor+=len;
  while(*cursor && *cursor!=' '){
    if(*cursor=='+') *reqcur=' ';
    else if(*cursor=='%'){
      cursor++;
      nibble=strchr(hexalpha,*cursor);
      if(!nibble) return(-1);
      n=nibble-hexalpha;
      if(n>15) n-=6;
      cursor++;
      nibble=strchr(hexalpha,*cursor);
      if(!nibble) return(-1);
      l=nibble-hexalpha;
      if(l>15) l-=6;
      n=(n<<4)&l; 
      *reqcur=n;
      }
    else *reqcur=*cursor;
    cursor++;
    reqcur++;
    }
  *reqcur=0;
  reqcur++;
  memcpy(reqcur,cursor,reqlen-(cursor-request));
  *(reqcur+reqlen-(cursor-request))=0;
  session->headerfields=reqcur;
  nbLogMsg(context,0,'T',"Header fields:\n%s\n",session->headerfields);

  // process header entries

  cursor=strchr(session->headerfields,'\n');
  if(cursor==NULL) return(1);
  cursor++;
  while(cursor && *cursor){
     if(strncmp(cursor,"Host: ",6)==0){
       cursor+=6;
       delim=strchr(cursor,'\n');
       if(*(delim-1)=='\r') delim--;
       len=delim-cursor;
       strncpy(session->reqhost,cursor,len);
       *(session->reqhost+len)=0;
       }
     else if(strncmp(cursor,"Authorization: Basic ",21)==0){
       cursor+=21;
       delim=strchr(cursor,'\n');
       if(*(delim-1)=='\r') delim--;
       len=delim-cursor;
       strncpy(session->reqauth,cursor,len);
       *(session->reqauth+len)=0;
       }
     cursor=strchr(cursor,'\n');
     if(cursor) cursor++;
     }
  return(0);
  }

static void websterOutputHandler(nbCELL context,void *session,char *text){
  websterPut(context,(nbSession *)session,text);
  }

static void websterNbCmd(nbCELL context,nbSession *session,char *command){
  nbLogFlush(context);
  websterPut(context,session,"<pre><font size='+1'>\n");
  // register output handler
  nbLogHandlerAdd(context,session,websterOutputHandler);
  // issue command - need to change to use identity associated with certificate
  nbCmd(context,command,1);   
  // unregister output handler
  nbLogHandlerRemove(context,session,websterOutputHandler);
  websterPut(context,session,"</font></pre>\n");
  }

// Display help on URL's recognized by Webster

static void websterUrl(nbCELL context,nbSession *session){
  char *page=
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
  websterPut(context,session,page); 
  }

// Handle Request Error

static void websterError(nbCELL context,nbSession *session,char *problem,char *value){
  char html[1024];

  sprintf(html,
    "<p><table width='100%' border='1' bgcolor='pink'>"
    "<tr><td><b>%s</b></td></tr>"
    "<tr><td><pre>%s</pre></td></tr></table>\n",
    problem,value);
  websterPut(context,session,html);
  }
   
static void websterResourceError(nbCELL context,nbSession *session){
  websterError(context,session,"Resource not recognized.",session->resource);
  websterUrl(context,session);
  }

static void websterCmd(nbCELL context,nbSession *session,char *cursor){
  //char *cursor=session->resource+3;
  char text[NB_BUFSIZE];
  static char *html=
    "<p><h1>Command <a href=':help?command'><img src='webster/help.gif' border=0></a></h1>\n"
    "<p><form name='nb' action=':nb' method='get'>\n"
    "<input tabindex='1' type='text' name='arg' size='120' title='Enter NodeBrain Command'></form>\n"
    "";

  websterPut(context,session,html);
  if(*cursor){
    //cursor++;
    //if(strncmp(cursor,"arg=",4)==0) cursor+=4;  // accept ":nb?<command>" or ":nb?arg=<command>" - the second is from a form.
    sprintf(text,"<a href=':nb?%s'>%s</a>\n",cursor,cursor);
    websterPut(context,session,text);
    //websterPut(context,session,cursor);
    websterNbCmd(context,session,cursor);
    }
  }
 
static void websterLink(nbCELL context,nbSession *session,char *path);

static void websterBookmark(nbCELL context,nbSession *session,char *cursor){
  char buffer[NB_BUFSIZE];
  char filename[1024];
  char *delim,*curbuf=buffer;
  char *menu="";
  char *name="";
  char *note="";
  char *url="";
  struct stat filestat;      // file statistics
  FILE *file;

  //nbLogMsg(context,0,'T',"bookmark request: %s",cursor);
  //websterPut(context,session,"bookmark request:");
  //websterPut(context,session,cursor);
  while(*cursor!=0){
    if(strncmp(cursor,"menu=",5)==0){
      cursor+=5;
      delim=strchr(cursor,'&');
      if(delim==NULL) delim=strchr(cursor,0);
      menu=curbuf;
      strncpy(menu,cursor,delim-cursor);
      }
    else if(strncmp(cursor,"name=",5)==0){
      cursor+=5;
      delim=strchr(cursor,'&');
      if(delim==NULL) delim=strchr(cursor,0);
      name=curbuf;
      strncpy(name,cursor,delim-cursor);
      }
    else if(strncmp(cursor,"note=",5)==0){
      cursor+=5;
      delim=strchr(cursor,'&');
      if(delim==NULL) delim=strchr(cursor,0);
      note=curbuf;
      strncpy(note,cursor,delim-cursor);
      }
    else if(strncmp(cursor,"url=",4)==0){
      cursor+=4;
      delim=strchr(cursor,'&');
      if(delim==NULL) delim=strchr(cursor,0);
      url=curbuf;
      strncpy(url,cursor,delim-cursor);
      }
    else{
      websterResourceError(context,session);
      return;
      }
    curbuf+=delim-cursor;
    *curbuf=0;
    curbuf++; 
    cursor=delim;
    if(*cursor) cursor++;
    }
  //nbLogMsg(context,0,'T',"bookmark: menu='%s',name='%s',note='%s',url='%s'",menu,name,note,url);
  // check for existing entry
  sprintf(filename,"%s/webster/%s/%s",session->webster->rootdir,menu,name);
  if(stat(filename,&filestat)==0) websterError(context,session,"Bookmark already defined.",filename);
  else{
    if(*url==0){  // create directory
#if defined(WIN32)
	  if(mkdir(filename)==0){
#else
      if(mkdir(filename,S_IRWXU)==0){
#endif
        strcat(filename,"/.note");
        if((file=fopen(filename,"w"))==NULL) websterError(context,session,"Unable to open bookmark file.",filename);
        else{
          fprintf(file,"%s\n",note);
          fclose(file);
          }
        }
      else websterError(context,session,"Unable to create bookmark folder.",filename);
      }
    else{ // create file
      if((file=fopen(filename,"w"))==NULL) websterError(context,session,"Unable to open bookmark file.",filename);
      else{
        fprintf(file,"%s\n",url);
        fprintf(file,"%s\n",note);
        fclose(file);
        }
      }
    }
  websterLink(context,session,menu);
  }

/*
*  Display link menu
*/
static void websterLinkDir(nbCELL context,nbSession *session,char *path);

static void websterLink(nbCELL context,nbSession *session,char *path){
  nbWebster *webster=session->webster;
  char *head=
    "<p><h1>Bookmarks <a href=':help?bookmarks'><img src='webster/help.gif' border=0></a></h1>\n"
    "<p><table>\n";
  char *formHead=
    "</table>\n<table><tr><td>\n"
    "<p><form name='bookmark' action=':bookmark' method='get'>\n"
    "<input type='hidden' name='menu' value='";
  char *formTail=
    "'>\n<table>\n"
    "<tr><th>Name</th><td><input type='text' name='name' size='60' title='Enter name to appear in bookmark menu.'></td></tr>\n"
    "<tr><th>Note</th><td><input type='text' name='note' size='60' title='Enter short descriptive note.'></td></tr>\n"
    "<tr><th>URL</th><td><input type='text' name='url'  size='60' title='Enter optional URL.  Use copy and paste to avoid typos.'></td></tr>\n"
    "</table>\n"
    "<p><input type='submit' value='Bookmark'>\n"
    "</form></td></tr></table>\n";
  websterPut(context,session,head);
  if(*path==0) path="Bookmarks"; // this should be a configurable option
  websterLinkDir(context,session,path); 
  websterPut(context,session,formHead);
  websterPut(context,session,path);
  websterPut(context,session,formTail);
  }
 
// Display a regular file

static void websterFile(nbCELL context,nbSession *session,char *name){
  nbWebster *webster=session->webster;
  char text[NB_BUFSIZE+1],*end,*filename;
  char buffer[NB_BUFSIZE],*bufcur,*bufend;
  int fildes,len,maxsub=6;
  unsigned char *cursor;

  filename=name+strlen(name)-1;
  while(filename>name && *filename!='/') filename--;
  if(*filename=='/') filename++;
  sprintf(text,"<p><b>File: %s</b><p><pre>\n",filename);
  websterPut(context,session,text);
  if((fildes=open(name,O_RDONLY))<0){
    sprintf(text,"<p><b>Open failed: %s</b>\n",strerror(errno));
    websterPut(context,session,text);
    return;
    }
  while((len=read(fildes,text,sizeof(text)-1))>0){
    end=text+len;
    *end=0;
    cursor=text;
    bufcur=buffer;
    bufend=buffer+sizeof(buffer)-maxsub;   // stay back enough to make largest substitution
    while(*cursor){
      if(*cursor>127){
        *bufcur=0;
        websterPut(context,session,buffer);
        websterPut(context,session,"</pre><p><b>*** File contains binary data. ***</b>\n"); 
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
        websterPut(context,session,buffer);
        bufcur=buffer;
        bufend=buffer+sizeof(buffer)-maxsub;
        }
      }
    *bufcur=0;
    if(*buffer) websterPut(context,session,buffer);
    }
  close(fildes);
  websterPut(context,session,"</pre>\n");
  }

// Display a note file

static void websterNote(nbCELL context,nbSession *session,char *name){
  nbWebster *webster=session->webster;
  char text[4097],*end;
  int fildes,len;

  if((fildes=open(name,O_RDONLY))<0){
    sprintf(text,"<b>Open failed: %s</b>\n",strerror(errno));
    websterPut(context,session,text);
    return;
    }
  while((len=read(fildes,text,sizeof(text)-1))>0){
    end=text+len;
    *end=0;
    websterPut(context,session,text);
    }
  close(fildes);
  }

#if defined(WIN32)

static char *websterGetFileType(int mode){
  if(mode&_S_IFREG) return("");
  else if(mode&_S_IFDIR) return("dir");
  else return("?");
  }

#else // Unix/Linux

static char *websterGetFileType(mode_t mode){
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

static char *websterLinkDirRow(nbCELL context,nbSession *session,char *class,int row,char *path){
  char text[1024];
  char *name=path+strlen(path)-1;
  while(name>=path && *name!='/') name--;
  name++;
  sprintf(text,"<tr class='%s'><td>%d</td><td><table cellspacing=0 cellpadding=0><tr><td><img src='webster/folder.gif'></td><td>&nbsp;<a href=':menu?%s'>%s</a></td></tr></table></td><td>\n",class,row,path,name);
  websterPut(context,session,text);
  sprintf(text,"%s/webster/%s/.note",session->webster->rootdir,path);
  websterNote(context,session,text);
  websterPut(context,session,"</td></tr>\n");
  }

#if defined(WIN32)
static void websterLinkDir(nbCELL context,nbSession *session,char *path){
  }

#else  // Unix/Linux

// Display a link directory
static void websterLinkDir(nbCELL context,nbSession *session,char *path){
  nbWebster *webster=session->webster;
  DIR *dir;
  struct dirent *ent;
  struct stat filestat;      // file statistics
  int i,n,len;
  char text[NB_BUFSIZE];
  char *class,*cursor,*delim;
  char *filename,*extension;
  struct FILE_ENTRY{
    struct FILE_ENTRY *next;
    char   name[512];
    };
  struct FILE_ENTRY *rootent;
  struct FILE_ENTRY *curent;
  struct FILE_ENTRY *nextent;

  rootent=malloc(sizeof(struct FILE_ENTRY));
  *rootent->name=0;
  rootent->next=NULL;
  sprintf(text,"%s/webster/%s",webster->rootdir,path);
  dir=opendir(text);
  if(dir==NULL){
    sprintf(text,"<p><b>*** Unable to open directory - %s ***</b>\n",strerror(errno));
    websterPut(context,session,text);
    return;
    }
  while((ent=readdir(dir))!=NULL){
    if(*ent->d_name!='.'){
      nextent=malloc(sizeof(struct FILE_ENTRY));
      strncpy(nextent->name,ent->d_name,sizeof(nextent->name)-1);
      *(nextent->name+sizeof(nextent->name)-1)=0;  // make sure we have a null char delimiter when truncated
      for(curent=rootent;curent->next!=NULL && strcasecmp(curent->next->name,ent->d_name)<0;curent=curent->next);
      nextent->next=curent->next;
      curent->next=nextent;
      }
    }
  closedir(dir);
  websterPut(context,session,"<p><table cellspacing=1 cellpadding=2 width='100%'><tr align='left'><th width='30'>&nbsp</th><th with=10%>Name</th><th>Note</th></tr>\n");
  cursor=path;
  i=0;
  while((delim=strchr(cursor,'/'))!=NULL){
    len=delim-path;
    strncpy(text,path,len);
    *(text+len)=0;
    //class=i%2 ? "odd" : "even";  // alternate row background
    websterLinkDirRow(context,session,"odd",i,text);
    cursor=delim+1;
    i++;
    }
  websterLinkDirRow(context,session,"marker",i,path);
  //websterPut(context,session,"<tr><th colspan=3><span style='font-size: 3px;'>&nbsp;</span></th></tr>\n");
  //i=0;
  for(curent=rootent->next;curent!=NULL;curent=curent->next){
    sprintf(text,"%s/webster/%s/%s",webster->rootdir,path,curent->name);
    if(stat(text,&filestat)!=0){
      sprintf(text,"<tr><td colspan=5>Stat: %s</td></tr>\n",strerror(errno));
      websterPut(context,session,text);
      }
    i++;
    //class=i%2 ? "odd" : "even";  // alternate row background
    class="even";
    if(S_ISDIR(filestat.st_mode)){
      sprintf(text,"<tr class='%s'><td>%d</td><td><table cellspacing=0 cellpadding=0><tr><td><img src='webster/folder.gif'></td><td>&nbsp;<a href=':menu?%s/%s'>%s</a></td></tr></table></td><td>\n",class,i,path,curent->name,curent->name);
      websterPut(context,session,text);
      sprintf(text,"%s/webster/%s/%s/.note",webster->rootdir,path,curent->name);
      websterNote(context,session,text);
      websterPut(context,session,"</td></tr>\n");
      }
    else if(S_ISREG(filestat.st_mode)){
      int fildes;
      sprintf(text,"%s/webster/%s/%s",webster->rootdir,path,curent->name);
      if((fildes=open(text,O_RDONLY))<0){
        sprintf(text,"<b>Open failed: %s</b>\n",strerror(errno));
        websterPut(context,session,text);
        return;
        }
      sprintf(text,"<tr class='%s'><td>%d</td><td><table cellspacing=0 cellpadding=0><tr><td><img src='webster/link.gif'></td><td>&nbsp;<a href='",class,i);
      websterPut(context,session,text); 
      len=read(fildes,text,sizeof(text)-1);
      if(len>0) *(text+len)=0;
      if(len>0 && (cursor=strchr(text,'\n'))!=NULL){
        *cursor=0;
        websterPut(context,session,text); 
        websterPut(context,session,"'>");
        websterPut(context,session,curent->name);
        websterPut(context,session,"</a></td></tr></table></td><td>\n");
        cursor++;
        nbLogMsg(context,0,'T',"cursor:%s\n",cursor);
        if(*cursor!=0) websterPut(context,session,cursor); 
        while((len=read(fildes,text,sizeof(text)-1))>0){
          *(text+len)=0;
          websterPut(context,session,text);
          }
        }
      else{ 
        sprintf(text,"<tr class='%s'><td>%d</td><td colspan=2>Unable to read: \"%s\"\n",class,i,curent->name);
        }
      close(fildes);
      websterPut(context,session,"</td></tr>\n");
      }
    }
  websterPut(context,session,"<tr><th colspan=3><span style='font-size: 3px;'>&nbsp;</span></th></tr>\n");
  websterPut(context,session,"</table>\n");
  // free list
  for(curent=rootent->next;curent!=NULL;curent=nextent){
    nextent=curent->next;
    free(curent);
    }
  }

#endif

// Display a directory
//   NOTE: we use opendir,readdir,closedir instead of scandir for better portability

static void websterDir(nbCELL context,nbSession *session,char *name){
  nbWebster *webster=session->webster;
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
  int i,n,len;
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
  struct FILE_ENTRY *rootdocent,*curdocent,*nextdocent;

  rootent=malloc(sizeof(struct FILE_ENTRY));
  *rootent->name=0;
  rootent->next=NULL;
  rootdocent=malloc(sizeof(struct FILE_ENTRY));
  *rootdocent->name=0;
  rootdocent->next=NULL;
  curdocent=rootdocent;

  filename=name+strlen(name)-1;
  while(filename>name && *filename!='/') filename--;
  if(*filename=='/') filename++;
  sprintf(text,"<p><b>Directory: %s</b>\n",filename);
  websterPut(context,session,text);
#if defined(WIN32)
  sprintf(findname,"%s/*",name);
  if((dir=FindFirstFile(findname,&info))==INVALID_HANDLE_VALUE){
    sprintf(text,"<p><b>*** Unable to open directory - errcode=%d ***</b>\n",GetLastError());
#else
  dir=opendir(name);
  if(dir==NULL){
    sprintf(text,"<p><b>*** Unable to open directory - %s ***</b>\n",strerror(errno));
#endif
	websterPut(context,session,text);
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
          curdocent->next=malloc(sizeof(struct FILE_ENTRY));
          curdocent=curdocent->next;
          curdocent->next=NULL;
          strncpy(curdocent->name,filename+1,len-9); // get just "NAME" of ".NAME.webster"
          *(curdocent->name+len-9)=0;  // terminate string
          nbLogMsg(context,0,'T',"Doc entry: %s\n",curdocent->name);
          }
        }
      }
    else{
      nextent=malloc(sizeof(struct FILE_ENTRY));
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
  websterPut(context,session,"<p><table cellspacing=1 cellpadding=1><tr align='left'><th>&nbsp</th><th>Modified</th><th align='right'>Size</th><th>Type</th><th>File</th><th>Note</th></tr>\n");
  i=0;
  for(curent=rootent->next;curent!=NULL;curent=curent->next){
    havenote=0;
    sprintf(text,"%s/%s",name,curent->name);
    if(stat(text,&filestat)==0){
      filetype=websterGetFileType(filestat.st_mode);
      tm=localtime(&filestat.st_mtime);
      sprintf(filetime,"<table cellpadding=0><tr><td>%4.4d-%2.2d-%2.2d</td><td>&nbsp;%2.2d:%2.2d</td></tr></table>\n",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min);
      sprintf(filesize,"%d",filestat.st_size);
      for(curdocent=rootdocent->next;curdocent!=NULL && strcmp(curent->name,curdocent->name)!=0;curdocent=curdocent->next);
      if(curdocent!=NULL) havenote=1;
      } 
    else{
      sprintf(text,"<tr><td colspan=5>Stat: %s</td></tr>\n",strerror(errno));
      websterPut(context,session,text);
      filetype="?";
      strcpy(filetime,"yyyy-mm-dd"); 
      strcpy(filesize,"?");
      }
    i++;
    class=i%2 ? "odd" : "even";  // alternate row background
    sprintf(text,"<tr class='%s'><td>%d</td><td>%s</td><td align='right'>&nbsp;%s</td><td>&nbsp;%s</td><td><a href=':file?%s/%s'>%s</a></td><td>\n",class,i,filetime,filesize,filetype,name,curent->name,curent->name);
    websterPut(context,session,text);
    if(havenote){
      sprintf(notefile,"%s/.%s.webster",name,curent->name);
      websterNote(context,session,notefile);
      }
    websterPut(context,session,"</td></tr>\n");
    }
  websterPut(context,session,"</table>\n");
  // free list
  for(curent=rootent->next;curent!=NULL;curent=nextent){
    nextent=curent->next;
    free(curent);
    }
  for(curent=rootdocent->next;curent!=NULL;curent=nextent){
    nextent=curent->next;
    free(curent);
    }
  }

static char *websterGetLinkedPath(char *html,char *path){
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

static void websterPath(nbCELL context,nbSession *session,char *name){
  struct stat filestat;
  char heading[512+32];
  char text[NB_BUFSIZE],whtml[NB_BUFSIZE];
  char filesize[16];
  char filetime[17];
  char *filetype;
  struct tm *tm;
  static char *html=
    "<p><h1>Directory <a href=':help?directory'><img src='webster/help.gif' border=0></a></h1>\n";

  websterPut(context,session,html);
  sprintf(text,
    "<p><form name='file' action=':file' method='get'>\n"
    "<input type='text' name='arg' size='120' value='%s' title='Enter path and press enter key.'></form>\n",
    name);
  websterPut(context,session,text);

  if(stat(name,&filestat)!=0){
    websterError(context,session,"File not found.",name);
    return;
    }

  // display statistics
  
  filetype=websterGetFileType(filestat.st_mode);
  if(!*filetype) filetype="regular";
  tm=localtime(&filestat.st_mtime);
  sprintf(filetime,"%4.4d-%2.2d-%2.2d %2.2d:%2.2d\n",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min);
  sprintf(filesize,"%d",filestat.st_size);
  sprintf(text,
    "<p><table>"
    "<tr><th>Modified</th><th>Size</th><th>Type</th><th>Path</th></tr>\n"
    "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n"
    "</table>",
    filetime,filesize,filetype,websterGetLinkedPath(whtml,name));

  websterPut(context,session,text);

// Display contents of regular files and directories
#if defined(WIN32)
  if(filestat.st_mode&_S_IFREG) websterFile(context,session,name);
  else if(filestat.st_mode&_S_IFDIR) websterDir(context,session,name);
#else
  if(S_ISREG(filestat.st_mode)) websterFile(context,session,name);
  else if(S_ISDIR(filestat.st_mode)) websterDir(context,session,name);
#endif
  }

/*
*  Display help page  - don't want to rely on the configuration for these help pages
*
*    These help pages cover Webster usage topics.  The goal is to provide built-in help sufficient for
*    navigating the user interface.  User's can provide help pages for their own application in at least
*    three different ways: 1) creating bookmarks for help pages, 2) creating help pages linked off the
*    home page, and 3) creating help pages in web content displayed by Webster as a standard web server.
*/

static void websterHelp(nbCELL context,nbSession *session,char *topic){
  nbWebster *webster=session->webster;
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
  else if(strcmp(topic,"url")==0){
    websterUrl(context,session);    // The websterURL text can be moved to this function
    return;
    }
  else html=htmlError;
  websterPut(context,session,html);
  }

static void websterContentHeading(nbCELL context,nbSession *session,char *code,char *type,char *subtype,int length){
  nbWebster *webster=session->webster;
  char buffer[1024];
  char ctimeCurrent[32],ctimeExpires[32];
  time_t currentTime;
 
  session->keepAlive=1;  // set keepAlive flag
  time(&currentTime);
  //asctime_r(gmtime(&currentTime),ctimeCurrent);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  currentTime+=24*60*60; // add one day
  //asctime_r(gmtime(&currentTime),ctimeExpires);
  strncpy(ctimeExpires,asctime(gmtime(&currentTime)),sizeof(ctimeExpires));
  *(ctimeExpires+strlen(ctimeExpires)-1)=0;

  sprintf(buffer,
    "HTTP/1.1 %s\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster 0.7.0\r\n"
    "Last-Modified: %s\r\n"
    "Expires: %s\r\n"
    "Connection: keep-alive\r\n"
    "Accept-Ranges: none\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: %s/%s\r\n\r\n",
    code,ctimeCurrent,ctimeCurrent,ctimeExpires,length,type,subtype);
  websterPut(context,session,buffer);
  websterPut(context,session,NULL);
  nbLogMsg(context,0,'T',buffer);
  }

static void websterHeading(nbCELL context,nbSession *session){
  nbWebster *webster=session->webster;
  char ctimeCurrent[32],ctimeExpires[32];
  time_t currentTime;
  static char *staticHeading=
    "<link rel='shortcut icon' href='nb.ico'>\n"
    "<link rel='stylesheet' title='webster_style' href='style/webster.css' type='text/css'>\n"
    "<meta http-equiv='Default-Style' content='webster_style'>\n"
    "</head>"
    "<body marginwidth='0' marginheight='0' leftmargin='0' topmargin='0'>\n"
    "<table width='100%' cellspacing=0 border=0 cellpadding=0 bgcolor='#000000'>\n"
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
    "</td>\n";
  char variableHeading[2048];

  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  sprintf(variableHeading,
    "HTTP/1.1 200 OK\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster 0.7.0\r\n"
    "Last-Modified: %s\r\n"
    "Expires: %s\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html\r\n\r\n"
    "<html>\n"
    "<head>\n"
    "<title>%s Webster</title>\n",
    ctimeCurrent,ctimeCurrent,ctimeCurrent,webster->cabTitle);
  websterPut(context,session,variableHeading);
  websterPut(context,session,staticHeading);
  sprintf(variableHeading,
    "<td align='center' valign='middle'>\n"
    "<a class='plain' href='%s'>\n"
    "<span class='cabTitle'>%s<span></a>\n"
    "<span class='cabVersion'>%s<span>\n"
    "</td>\n"
    "<td align='center' valign='middle'>\n"
    "<span style='font-size: 10px; color: white'>\n"
    "N o d e B r a i n &nbsp; C a b o o d l e &nbsp; K i t &nbsp; 0.7.5\n"
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
    webster->cabLink,webster->cabTitle,webster->cabVersion,session->reqhost,webster->dir,webster->cabMenu);
  websterPut(context,session,variableHeading);
  }

static void websterFooting(nbCELL context,nbSession *session){
  static char *html=
   "\n</td></tr></table>\n<hr>\n<span class='foot'>&nbsp;NodeBrain Webster 0.7.5 OpenSSL Server</span>\n</body>\n</html>\n";
  websterPut(context,session,html); 
  }

static void websterDisplayHomePageForm(nbCELL context,nbSession *session){
  static char *html=
    "<p><h1>Webster Home Page Form</h1>\n"
    "<form name='homepage' action=':create?home' method='post'>"
    "<p><b>Title</b>\n"
    "<p>Enter a short desciptive title for this site. \n"
    "It should identify the NodeBrain agent or application you managed with this site. \n"
    "If you have instances of the same application on multiple servers, you may else to use the same title and description on each.  \n"
    "You can tell which server you are accessing by the host name at the top right portion of each page.  \n"
    "<p><input type='text' size='60' name='title' title='Site title'>\n"
    "<p><b>Description</b>\n"
    "<p>Enter a one or two paragraph description of the agent or application.\n"
    "<p><textarea rows=10 cols=100 name='title' title='Enter one paragraph description.'></textarea>"
    "<p><input type=submit value='Submit'>";
    "</form>";
  websterPut(context,session,html); 
  }

/*
*  Display a page from the Webster content directory.
*
*    This content is designed to appear within a webster page, so must only provide content appropriate
*    within the body of an HTML page.
*/
static void websterPage(nbCELL context,nbSession *session,char *path){
  nbWebster *webster=session->webster;
  char filepath[1024];
  int fildes,len;
  char text[NB_BUFSIZE];

  websterPut(context,session,"<p>\n");
  if(*path==0) path="index.ht";
  //sprintf(filepath,"%s/webster/%s",webster->dir,path);  // subdirectory should be configurable
  sprintf(filepath,"%s/webster/%s",webster->rootdir,path);  // subdirectory should be configurable
  if((fildes=open(filepath,O_RDONLY))<0){
    websterError(context,session,"Unable to open page content file.",filepath);
    websterHelp(context,session,"home");
    return;
    }
  while((len=read(fildes,text,sizeof(text)-1))>0){
    *(text+len)=0;
    websterPut(context,session,text);
    }
  close(fildes);
  }

static void websterEndSession(nbCELL context,nbSession *session){
  websterPut(context,session,NULL); // flush buffer
  if(session->ssl!=NULL){
    nbLogMsg(session->webster->context,0,'T',"websterEndSession called");
    SSL_shutdown(session->ssl);
    SSL_free(session->ssl);
    }
  nbIpClose(session->channel);
  nbIpFree(session->channel);
  free(session);
  }

static void websterRequest(nbCELL context,int socket,void *handle);

static void websterEndResponse(nbCELL context,nbSession *session){
  if(!session->keepAlive){  // keep alive after sending for fixed length objects
    websterEndSession(context,session);
    return;
    }
  websterPut(context,session,NULL); // flush buffer
  nbListenerAdd(context,session->channel->socket,session,websterRequest);
  nbLogMsg(context,0,'I',"Listening for requests on socket %u",session->channel->socket);
  }

static int cgiCloser(nbPROCESS process,int pid,void *processSession){
  nbSession *session=processSession;
  nbLogMsg(session->webster->context,0,'T',"cgiCloser called");
  websterEndResponse(session->webster->context,session);
  return(0);
  }

// read a stderr line and write it to the log
static int cgiErrReader(nbPROCESS process,int pid,void *processSession,char *msg){
  nbSession *session=processSession;
  nbLogMsg(session->webster->context,0,'W',"%s",msg);
  return(0);
  }

// Pass cgi output to the client
static int cgiReader(nbPROCESS process,int pid,void *processSession,char *msg){
  nbSession *session=processSession;
  nbWebster *webster=session->webster;
  static int lineno=3;  // silly switch while debugging heading

  if(strncmp(msg,"Content-type:",13)==0) lineno=0;
  lineno++; 
  if(lineno<3) return(0);
  //nbLogMsg(webster->context,0,'T',"cgiReader [%d,%d] %s",pid,session->channel->socket,msg);
  websterPut(webster->context,session,msg);
  websterPut(webster->context,session,"\n");
  return(0);
  }

// message writer 
static int cgiWriter(nbPROCESS process,int pid,void *processSession){
  nbSession *session=processSession;
  int len;
  nbLogMsg(session->webster->context,0,'T',"cgiWriter called - expect=%d",session->contentLength);
  if(session->contentLength<=0) return(1);
/*
  if(session->content){
    nbLogMsg(session->webster->context,0,'T',"Sending content:\n%s\n",session->content);
    nbMedullaProcessPut(session->process,session->content);
    session->contentLength-=strlen(session->content);
    session->content=NULL;
    }
  else{
*/
    nbLogMsg(session->webster->context,0,'T',"Checking for more content");
    len=websterRead(session->webster->context,session);
    if(len<=0) return(1);
    nbLogMsg(session->webster->context,0,'T',"Sending content:\n%s\n",(char *)session->channel->buffer);
    nbMedullaProcessPut(session->process,(char *)session->channel->buffer);
    session->contentLength-=len;
//    }
  nbLogMsg(session->webster->context,0,'T',"contentLength=%d\n",session->contentLength);
  return(0);
  }

static int websterCgi(nbCELL context,nbSession *session,char *file,char *queryString){
  nbWebster *webster=session->webster;
  FILE *pipe;
  char buf[NB_BUFSIZE],dir[2048],*cursor,*delim,value[512];
  int  rc;
  
  // set environment variables for the cgi program
  setenv("SERVER_SOFTWARE","NodeBrain Webster/0.7.0",1);
  setenv("GATEWAY_INTERFACE","CGI/1.1",1);
  setenv("SERVER_PROTOCOL","HTTP/1.1",1);
  setenv("SSL_CLIENT_S_DN_CN",session->userid,1);
  setenv("QUERY_STRING",queryString,1);
  strcpy(dir,file);
  delim=dir+strlen(dir)-1;
  while(delim>dir && *delim!='/') delim--;
  if(delim>dir){
    *delim=0;
    chdir(dir);
    getcwd(dir,sizeof(dir));
    nbLogMsg(context,0,'T',"During pwd=%s",dir);
    }
  if(session->method==NB_WEBSTER_METHOD_GET){
    setenv("REQUEST_METHOD","GET",1);
    sprintf(session->command,"=|:$ %s/%s",webster->rootdir,file); // convert to medula command with full path
    nbLogMsg(context,0,'T',"CGI GET request: %s",session->command);
    session->process=nbMedullaProcessOpen(NB_CHILD_TERM|NB_CHILD_SESSION,session->command,"",session,cgiCloser,NULL,cgiReader,cgiErrReader,buf);
    if(session->process==NULL){
      nbLogMsg(context,0,'E',"%s",buf);
      return(-1);
      }
    }
  else if(session->method==NB_WEBSTER_METHOD_POST){
    char *buffer=(char *)session->channel->buffer;
    int len,expect;
    setenv("REQUEST_METHOD","POST",1);
    nbLogMsg(context,0,'T',"CGI Post request: %s",session->command);
/*
    len=websterRead(context,session);
    nbLogMsg(context,0,'T',"Len=%d %s",len,buffer);
    if(strncmp(buffer,"Content-Type: ",14)!=0){
      nbLogMsg(context,0,'E',"Expected Content-Type not received");
      return(-1);
      }
    cursor=buffer+14;
    delim=strchr(cursor,'\r');
    if(delim!=NULL) *delim=0;
    delim=strchr(cursor,'\n');
    if(delim!=NULL) *delim=0;
    setenv("CONTENT_TYPE",cursor,1);
*/
    setenv("CONTENT_TYPE","application/x-www-form-urlencoded",1);
    cursor=session->headerfields;
    nbLogMsg(context,0,'T',"Header fields:\n%s\n",cursor);
    while(!session->contentLength && NULL!=(delim=strchr(cursor,'\n'))){
      cursor=delim+1;
      if(strncmp(cursor,"Content-Length: ",16)==0){
        nbLogMsg(context,0,'T',"Content-Length found at: %s\n",cursor);
        cursor+=16;
        delim=strchr(cursor,'\n');
        if(delim){
          len=delim-cursor;
          strncpy(value,cursor,len);
          *(value+len)=0;
          delim=strchr(value,'\r');
          if(delim) *delim=0;
          setenv("CONTENT_LENGTH",value,1);
          session->contentLength=atoi(value);
          }
        }
      }
    if(!session->contentLength){
      nbLogMsg(context,0,'E',"Expected Content-Length not received");
      return(-1);
      }
    session->content=NULL;
    while(!session->content && NULL!=(delim=strchr(cursor,'\n'))){
      cursor=delim+1;
      nbLogMsg(context,0,'T',"checking for empty line at: %s\n",cursor);
      if(strncmp(cursor,"\r\n",2)==0) session->content=cursor+2;
      }     
    sprintf(session->command,"|=|:$ %s/%s",webster->rootdir,file); // convert to medula command with full path
    nbLogMsg(context,0,'T',"Post request: %s",session->command);
    session->process=nbMedullaProcessOpen(NB_CHILD_TERM|NB_CHILD_SESSION,session->command,"",session,cgiCloser,cgiWriter,cgiReader,cgiErrReader,buf);
    if(session->process==NULL){
      nbLogMsg(context,0,'E',"%s",buf);
      return(-1);
      }
    if(session->content){
      nbLogMsg(context,0,'T',"Sending content:\n%s\n",session->content);
      nbMedullaProcessPut(session->process,session->content);
      session->contentLength-=strlen(session->content);
      session->content=NULL;
      }
    }
  nbLogMsg(context,0,'T',"Process %d started",nbMedullaProcessPid(session->process));
  return(0);
  }

/*
* Little Web Server
*
* Here we handle requests as if Webster were a normal web server.  We
* don't pretend that it is a fully compliant web server, but provide enough
* basic functionality to support web content and cgi scripts included
* in NodeBrain kits.  The goal of Webster is to provide a web interface
* to a NodeBrain agent.  Web server functionality is included only for
* convenient integration with NodeBrain applications when the use of 
* an industrial web server is not convenient. 
*/
static void websterLittleWebServer(nbCELL context,nbSession *session){
  nbWebster *webster=session->webster;
  char *filename;
  struct stat filestat;      // file statistics
  int fildes,len;
  char fullname[1024],text[NB_BUFSIZE],content[NB_BUFSIZE],*cursor,*delim,*queryString;
  int contentLength;
   
  filename=session->resource;
  if(!*filename) filename="index.html";  // could be configurable, but keep it simple for now
  strcpy(fullname,filename);
  //if(*filename!='/') sprintf(fullname,"%s/%s",webster->rootdir,filename); // compose full path name for relative names
  if(*(fullname+strlen(fullname)-1)=='/') strcat(fullname,"index.html");
  // check for cgi script query string
  queryString=strchr(fullname,'?');
  if(queryString){
    *queryString=0;
    queryString++;
    }
  cursor=fullname+strlen(fullname)-4;
  if(cursor>fullname && strncmp(cursor,".cgi",4)==0){
    if(!queryString) queryString="";
    }
  if(queryString){
    if(0>websterCgi(context,session,fullname,queryString)){
      nbLogMsg(context,0,'T',"Error returned by websterCgi");
      sprintf(content,
        "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
        "<html>\n<head>\n"
        "<title>500 Internal Server Error</title>\n"
        "</head>\n<body>\n"
        "<b><big>500 Internal Server Error</big></b>\n"
        "<p>The server encountered an internal error and was unable to complete your request.</p>\n"
        "<p>If you think the resource name <i><b>%s</b></i> is valid, please contact the webmaster\n"
        "<hr>\n"
        "<i>NodeBrain Webster 0.7.0 OpenSSL Server at %s</i>\n"
        "</body>\n</html>\n",
        fullname,session->reqhost);
      contentLength=strlen(content);
      sprintf(text,
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Date: Thu, 16 Aug 2007 04:04:33 GMT\r\n"
        "Server: NodeBrain Webster 0.7.0\r\n"
        "Location: https://%s/%s\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: text/html; charset=iso-8859-1\r\n\r\n"
        "%s",
        session->reqhost,fullname,contentLength,content);
      nbLogMsg(context,0,'T',"Returning content:\n%s\n",text);
      websterPut(context,session,text);
      websterEndResponse(context,session);
      }
    return;
    }
#if defined(WIN32)
  else if(stat(fullname,&filestat)==0 && (filestat.st_mode&_S_IFDIR)){
#else
  else if(stat(fullname,&filestat)==0 && S_ISDIR(filestat.st_mode)){
#endif
    sprintf(content,
      "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
      "<html><head>\n"
      "<title>301 Moved Permanently</title>\n"
      "</head><body>\n"
      "<h1>Moved Permanently</h1>\n"
      "<p>The document has moved <a href='https://%s/%s/index.html'>here</a>.</p>\n"
      "<hr/>\n"
      "<address>NodeBrain Webster 0.7.0 OpenSSL Server at %s</address>\n"
      "</body>\n</html>\n",
      session->reqhost,fullname,session->reqhost);
    contentLength=strlen(content);
    sprintf(text,
      "HTTP/1.1 301 Moved Permanently\r\n"
      "Date: Thu, 16 Aug 2007 04:04:33 GMT\r\n"
      "Server: NodeBrain Webster 0.7.0\r\n"
      "Location: https://%s/%s/index.html\r\n"
      "Connection: close\r\n"
      "Content-Length: %d\r\n"
      "Content-Type: text/html; charset=iso-8859-1\r\n\r\n"
      "%s",
      session->reqhost,fullname,contentLength,content);
    websterPut(context,session,text);
    }
#if defined(WIN32)
  else if((fildes=open(fullname,O_RDONLY|O_BINARY))<0){
#else
  else if((fildes=open(fullname,O_RDONLY))<0){
#endif
    sprintf(content,
      "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
      "<html><head>"
      "<title>404 Not Found</title>"
      "</head><body>"
      "<h1>Not Found</h1>"
      "<p>The requested URL /%s was not found on this server.</p>"
      "<hr>"
      "<address>NodeBrain Webster 0.7.0 OpenSSL Server at %s</address>"
      "</body></html>",
      filename,session->reqhost);
    contentLength=strlen(content);
    sprintf(text,
      "HTTP/1.1 404 Not Found\r\n"
      "Date: Thu, 16 Aug 2007 04:04:33 GMT\r\n"
      "Server: NodeBrain Webster 0.7.0\r\n"
      "Location: https://%s/%s/index.html\r\n"
      "Connection: close\r\n"
      "Content-Length: %d\r\n"
      "Content-Type: text/html; charset=iso-8859-1\r\n\r\n"
      "%s",
      session->reqhost,fullname,contentLength,content);
    websterPut(context,session,text);
    nbLogMsg(context,0,'T',"%s",text);
    }
  else{
    cursor=fullname;                  // look for file ext
    delim=strchr(cursor,'.');
    while(delim!=NULL){
      cursor=delim+1;
      delim=strchr(cursor,'.');
      }
    // This needs to be cleaned up using a lookup tree
    nbLogMsg(context,0,'T',"File extension: %s",cursor);
    if(strcmp(cursor,"gif")==0 || strcmp(cursor,"png")==0 ||strcmp(cursor,"jpg")==0 || strcmp(cursor,"pdf")==0 || strcmp(cursor,"ico")==0 || strcmp(cursor,"text")==0 || strcmp(cursor,"txt")==0 || strcmp(cursor,"jar")==0 || strcmp(cursor,"class")==0 || strcmp(cursor,"js")==0 || strcmp(cursor,"css")==0){  // check file type
      if(strcmp(cursor,"pdf")==0) websterContentHeading(context,session,"200 OK","application","pdf",filestat.st_size);
      else if(strcmp(cursor,"jar")==0) websterContentHeading(context,session,"200 OK","application","java-archive",filestat.st_size);
      else if(strcmp(cursor,"class")==0) websterContentHeading(context,session,"200 OK","application","java-byte-code",filestat.st_size);
      else if(strcmp(cursor,"js")==0) websterContentHeading(context,session,"200 OK","application","x-javascript",filestat.st_size);
      else if(strcmp(cursor,"text")==0 || strcmp(cursor,"txt")==0)
        websterContentHeading(context,session,"200 OK","text","plain",filestat.st_size);
      else if(strcmp(cursor,"ico")==0) websterContentHeading(context,session,"200 OK","image","vnd.microsoft.icon",filestat.st_size);
      else if(strcmp(cursor,"css")==0) websterContentHeading(context,session,"200 OK","txt","css",filestat.st_size);
      else websterContentHeading(context,session,"200 OK","image",cursor,filestat.st_size);
      while((len=read(fildes,content,sizeof(content)))>0){
        if(session->ssl!=NULL) SSL_write(session->ssl,content,len);
        else clearWrite(session->channel->socket,content,len);
        nbLogMsg(context,0,'T',"Sent %u bytes",len);
        }
      }
    else{
      while((len=read(fildes,text,sizeof(text)-1))>0){
        *(text+len)=0;
        websterPut(context,session,text);
        }
      }
    close(fildes);
    }
  websterEndResponse(context,session);
  }

// Provide a response telling the client we need a user and password

static void websterRequirePassword(nbCELL context,nbSession *session){
  char html[NB_BUFSIZE];
  char header[1024];

  sprintf(html,
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
    "<HTML><HEAD>\n"
    "<TITLE>401 Authorization Required</TITLE>\n"
    "</HEAD><BODY>\n"
    "<H1>Authorization Required</H1>\n"
    "This server could not verify that you\n"
    "are authorized to access the document\n"
    "requested.  Either you supplied the wrong\n"
    "credentials (e.g., bad password), or your\n"
    "browser doesn't understand how to supply\n"
    "the credentials required.<P>\n"
    "<HR>\n"
    "<ADDRESS>NodeBrain Webster 0.7.0 OpenSSL Server at %s</ADDRESS>\n"
    "</BODY></HTML>\n\n",
    session->reqhost);
  sprintf(header,
    "HTTP/1.1 401 Authorization Required\r\n"
    "Date: Fri, 17 Aug 2007 17:49:03 GMT\r\n"
    "Server: NodeBrain Webster 0.7.0 OpenSSL\r\n"
    "WWW-Authenticate: Basic realm=\"Webster\"\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: text/html; charset=iso-8859-1\r\n\r\n",
    strlen(html));
  websterPut(context,session,header);
  websterPut(context,session,html);
  websterPut(context,session,NULL);
  }

static int getRoleByPassword(nbCELL context,nbSession *session){
  nbUser *userNode;
  nbCELL key;
  key=nbCellCreateString(context,session->reqauth);
  userNode=nbTreeFind(key,(NB_TreeNode *)session->webster->userTree);
  if(userNode!=NULL){
    nbLogMsg(context,0,'T',"userid=%s found=%s role=%d",session->userid,userNode->userid,userNode->role);
    return(userNode->role);
    } 
  else nbLogMsg(context,0,'I',"userid '%s' not found",session->userid);
  return(NB_WEBSTER_ROLE_REJECT);
  }

static int getRoleByCertificate(nbCELL context,nbSession *session){
  X509 *clientCertificate;
  char *x509str;
  char buffer[2048];
  char *cursor,*delim;
  int  len;
  nbUser *userNode;
  nbCELL key;

  *session->userid=0;
  clientCertificate=SSL_get_peer_certificate(session->ssl);
  if(clientCertificate==NULL) nbLogMsg(context,0,'T',"Client certificate not found");
  else{
    nbLogMsg(context,0,'T',"Client cerfiticate found");
    x509str=X509_NAME_oneline(X509_get_subject_name(clientCertificate),buffer,sizeof(buffer));
    if(*x509str) nbLogPut(context,"  subject: %s\n",buffer);
    cursor=strstr(buffer,"/CN=");
    if(cursor){
      cursor+=4;
      delim=strchr(cursor,'/');
      if(!delim) len=strlen(cursor);
      else len=delim-cursor;
      strncpy(session->userid,cursor,len);
      *(session->userid+len)=0; 
      }
    x509str=X509_NAME_oneline(X509_get_issuer_name(clientCertificate),buffer,sizeof(buffer));
    if(x509str) nbLogPut(context,"  issuer: %s\n",x509str);
    X509_free(clientCertificate);
    }
  nbLogMsg(context,0,'T',"userid=%s",session->userid);
  key=nbCellCreateString(context,session->userid);
  userNode=nbTreeFind(key,(NB_TreeNode *)session->webster->userTree);
  if(userNode!=NULL){
    nbLogMsg(context,0,'T',"userid=%s found=%s role=%d",session->userid,userNode->userid,userNode->role);
    return(userNode->role);
    } 
  else nbLogMsg(context,0,'I',"userid '%s' not found",session->userid);
  return(NB_WEBSTER_ROLE_REJECT);
  }

// Handle Request
//
//   Any GET request starting with colon ':' is handled with built-in content or functionality that might
//   appear to be coming from a CGI script (but isn't).  While this functionality is the real purpose for this
//   module, using the colon to invoke it leaves open the possibility of acting like a small personal web
//   server in response to normal URL's.
//
static void websterRequest(nbCELL context,int socket,void *handle){
  nbSession *session=(nbSession *)handle;
  nbWebster *webster=session->webster;
  static int count=0;
  int len;
  char *cursor;
  char strbuf[512];

  // initialize some stuff for this request
  session->role=NB_WEBSTER_ROLE_REJECT;   // we may not need to redo this for every request
  session->keepAlive=0;
  *session->reqauth=0;
  *session->request=0;
  *session->page=0;
  session->pageend=session->page+sizeof(session->page)-1;
  session->pagecur=session->page;
  *session->userid=0;

  nbListenerRemove(context,session->channel->socket);
  if((len=websterRead(context,session))>0){  // get request
    nbLogMsg(context,0,'T',"Request received.");
    }
  else{
    nbLogMsg(context,0,'E',"Read error - %s",strerror(errno));
    websterEndSession(context,session);
    return;
    }
  count++;
  nbLogPut(context,"]%s\n",session->channel->buffer);

  if(websterDecodeRequest(context,session,(char *)session->channel->buffer,len)!=0){
    websterHeading(context,session);
    websterError(context,session,"Request not recognized by parser.",(char *)session->channel->buffer);
    websterFooting(context,session); 
    websterEndSession(context,session);
    return;
    }
  // now let's figure what to do
  //websterPut(context,session,session->request);     // use to inspect the client request
  session->role=NB_WEBSTER_ROLE_GUEST;
  if(strcmp(webster->authenticate,"no")!=0){
    if(strcmp(webster->authenticate,"password")==0) session->role=getRoleByPassword(context,session);
    else session->role=getRoleByCertificate(context,session);
    if(session->role==NB_WEBSTER_ROLE_REJECT){
      websterRequirePassword(context,session);  // tell client we need a password
      websterEndResponse(context,session);
      return;
      }
    }
  // If the user is authenticated and authorized as administrator then support some builtin functions
  if(strncmp(session->request,"GET /:",6)==0 && session->role==NB_WEBSTER_ROLE_ADMIN){
    websterHeading(context,session);
    cursor=session->request+6;
    if(strncmp(cursor,"page",4)==0){
      cursor+=4;
      if(*cursor==0) websterPage(context,session,cursor);
      else if(*cursor=='?'){
        cursor++;
        if(strncmp(cursor,"arg=",4)==0) cursor+=4;
        websterPage(context,session,cursor);
        }
      else websterResourceError(context,session);
      }
    else if(strncmp(cursor,"nb",2)==0){
      cursor+=2;
      if(*cursor==0) websterCmd(context,session,cursor);
      else if(*cursor=='?'){
        cursor++;
        if(strncmp(cursor,"arg=",4)==0) cursor+=4;
        websterCmd(context,session,cursor);
        }
      else websterResourceError(context,session); 
      }
    else if(strncmp(cursor,"file",4)==0){
      cursor+=4;
      if(*cursor==0) websterPath(context,session,webster->dir);
      else if(*cursor=='?'){
        cursor++;
        if(strncmp(cursor,"arg=",4)==0) cursor+=4;
        websterPath(context,session,cursor);
        }
      else websterResourceError(context,session); 
      }
    else if(strncmp(cursor,"menu",4)==0){
      cursor+=4;
      if(*cursor==0) websterLink(context,session,cursor);
      else if(*cursor=='?'){
        cursor++;
        if(strncmp(cursor,"arg=",4)==0) cursor+=4;
        websterLink(context,session,cursor);
        }
      else websterResourceError(context,session);
      }
    else if(strncmp(cursor,"help",4)==0){
      cursor+=4;
      if(*cursor==0) websterHelp(context,session,cursor);
      else if(*cursor=='?') websterHelp(context,session,cursor+1);
      else websterResourceError(context,session);
      }
    else if(strncmp(cursor,"bookmark?",9)==0) websterBookmark(context,session,cursor+9);
    else websterResourceError(context,session);
    websterFooting(context,session);
    websterEndResponse(context,session);
    return;
    }
  // ok, just act like a little web server
  nbLogMsg(context,0,'T',"Request: %s",session->request);
  chdir(webster->rootdir);
  websterLittleWebServer(context,session);
  chdir(webster->dir);
  }


// Shut down a connection

static void websterShutdown(nbCELL context,nbSession *session,char *error){
  if(error) nbLogMsg(context,0,'E',error);
  websterEndSession(context,session);
  }

//==================================================================================
//
// Handle connection requests
//
static void websterAccept(nbCELL context,int websterSocket,void *handle){
  nbWebster *webster=handle;
  NB_IpChannel *channel;
  static time_t now,until=0;
  static long count=0,max=10;  /* accept 10 connections per second */
  SSL *ssl;
  X509 *clientCertificate;
  int len,rc;
  char *buffer;
  char *x509str;
  nbSession *session;

  channel=nbIpAlloc();  // get a channel so we can use chaccept for now
  buffer=(char *)channel->buffer;
  if(nbIpAccept(channel,(int)webster->socket)<0){
    if(errno!=EINTR){
      nbLogMsg(context,0,'E',"websterAccept: chaccept failed");
      return;
      }
    nbLogMsg(context,0,'E',"websterAccept: chaccept interupted by signal.");
    nbIpFree(channel);
    return;
    }
  nbLogMsg(context,0,'I',"Request on port %s:%d from %s",webster->address,webster->port,channel->ipaddr);
  //websterServe(context,webster,channel);
  session=malloc(sizeof(nbSession));
  memset(session,0,sizeof(nbSession));
  session->webster=webster;
  session->channel=channel;
#if defined(WIN32)
  session->sbio=NULL;
#endif
  session->ssl=NULL;
  session->role=NB_WEBSTER_ROLE_REJECT;
  *session->userid=0;
  session->contentLength=0;
  session->content=NULL;

  if(webster->ctx!=NULL){
    ssl=SSL_new(webster->ctx);
    if(ssl==NULL){
      nbLogMsg(context,0,'E',"Unable to create SSL object");
      return;
      }
    session->ssl=ssl;
    nbLogMsg(context,0,'T',"Initializing the SSL structure");
#if defined(WIN32)
	session->sbio=BIO_new_socket(session->channel->socket,BIO_NOCLOSE);
	SSL_set_bio(ssl,session->sbio,session->sbio);
#else
    SSL_set_fd(ssl,session->channel->socket);
#endif

    SSL_set_ex_data(ssl,0,session);  // make session structure available to verify callback function

    nbLogMsg(context,0,'T',"Issuing SSL_accept on socket %d",session->channel->socket);
    if((rc=SSL_accept(ssl))!=1){
	  int sslError;
	  char msg[256];
      sslError=SSL_get_error(ssl,rc);
      ERR_error_string(sslError,msg);
	  nbLogMsg(context,0,'T',"SSL_accept rc=%d code=%d msg: %s",rc,sslError,msg);
      nbLogFlush(context);  
	  //ERR_print_errors_fp(stderr);
      websterShutdown(context,session,"Connection not accepted");
      return;
      }
    nbLogMsg(context,0,'I',"SSL connection using %s\n",SSL_get_cipher(ssl));
    }

  nbListenerAdd(context,session->channel->socket,session,websterRequest);
  nbLogMsg(context,0,'I',"Listening for requests on socket %u",session->channel->socket);
  }

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
  char *cursor=text;
  int type,trace=0,dump=0,format=0,null=0;
  char *websterSpec;
  char msg[1024];
  //char cmd[1024];

  //nbLogMsg(context,0,'T',"websterConstruct: called");
  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Expecting string webster specification as first parameter - identity@address:port");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Expecting string webster specification as first parameter - identity@address:port");
    return(NULL);
    }
  websterSpec=nbCellGetString(context,cell);
  webster=websterNew(context,websterSpec,msg);
  if(webster==NULL){
    nbLogMsg(context,0,'E',msg);
    return(NULL);
    }
  nbCellDrop(context,cell);
  
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  //nbLogMsg(context,0,'T',"websterConstruct: returning");
  return(webster);
  }

// Check to see if X509 certificate authentication was successful
//   We always make it successful by returning 1, but will request a password via HTTP if necessary

static int websterVerify(int preverify_ok,X509_STORE_CTX *ctx){
  SSL *ssl;
  X509 *clientCertificate;
  nbWebster *webster;
  nbSession *session;
  char *x509str;
  nbCELL context;
  char buffer[2048];

  ssl=X509_STORE_CTX_get_ex_data(ctx,SSL_get_ex_data_X509_STORE_CTX_idx());
  session=SSL_get_ex_data(ssl,0);  // note: we believe we know the index number here---be careful
  webster=session->webster;
  context=webster->context;
  if(preverify_ok){
    //clientCertificate=SSL_get_peer_certificate(ssl);
    //if(clientCertificate==NULL){
    //  websterShutdown(context,session,"Client certificate not found.");
    //  return(0);
    //  }
    // Fiddle with client certificate - eventually verify
    //nbLogMsg(context,0,'I',"Client certificate:");
    //x509str=X509_NAME_oneline(X509_get_subject_name(clientCertificate),buffer,sizeof(buffer));
    //if(*x509str) nbLogPut(context,"  subject: %s\n",buffer);
    //x509str=X509_NAME_oneline(X509_get_issuer_name(clientCertificate),buffer,sizeof(buffer));
    //if(x509str) nbLogPut(context,"  issuer: %s\n",x509str);
    //X509_free(clientCertificate);
    // save common name and email address
    strcpy(session->reqcn,"X509");
    }
  else *session->reqauth=0;
  return(1);  
  }

/*
*  Get option specified as string term in context
*
*  name:  Option name
*  value: Default value returned if term not found
*/
static char *getOption(nbCELL context,char *name,char *defaultValue){
  char *value=defaultValue;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL
    && (cell=nbTermGetDefinition(context,cell))!=NULL
    && nbCellGetType(context,cell)==NB_TYPE_STRING) value=nbCellGetString(context,cell);
  nbLogMsg(context,0,'T',"%s=%s",name,value);
  return(value);
  }

/* 
*  Load access list
*/
static int websterLoadAccessList(nbCELL context,nbWebster *webster,char *filename){
  FILE *accessFile;
  char buffer[1024],*cursor,*delim;
  char role;
  nbUser *userNode;
  int len,maxUserLen=31;
  NB_TreePath path;
  nbCELL key;

  if((accessFile=fopen(filename,"r"))==NULL){
    nbLogMsg(context,0,'E',"Unable to read access list file '%s'",filename); 
    return(-1);
    }
  while(fgets(buffer,sizeof(buffer),accessFile)){
    if(*buffer!='#'){
      cursor=buffer;
      if(*cursor=='a') role=NB_WEBSTER_ROLE_ADMIN;
      else role=NB_WEBSTER_ROLE_GUEST;
      cursor++;
      if(*cursor!=','){
        nbLogMsg(context,0,'E',"Access list '%s' contains invalid field separator: %s",filename,buffer);
        break;
        }
      cursor++;
      delim=strchr(cursor,';');
      if(delim==NULL){
        nbLogMsg(context,0,'E',"Access list '%s' contains entry without terminating ';'",filename);
        break;
        }
      *delim=0;
      len=strlen(cursor);
      if(len>maxUserLen){
        nbLogMsg(context,0,'E',"Access list '%s' contains userid longer than %d characters: %s",filename,maxUserLen,buffer);
        break;
        }
      key=nbCellCreateString(context,cursor);
      if(nbTreeLocate(&path,key,(NB_TreeNode **)&webster->userTree)==NULL){
        userNode=malloc(sizeof(nbUser)); 
        userNode->node.left=NULL;
        userNode->node.right=NULL;
        strncpy(userNode->userid,cursor,maxUserLen+1);
        userNode->node.key=&userNode->userid;
        userNode->role=role;
        nbTreeInsert(&path,(NB_TreeNode *)userNode);
        }
      }
    }
  fclose(accessFile);
  return(0);
  }

/*
*  enable() method
*
*    enable <node>
*/
static int websterEnable(nbCELL context,void *skillHandle,nbWebster *webster){
  SSL_METHOD *method;
  SSL_CTX    *ctx=NULL;
  char ctxContext[1024];
  int dataIndex;
  char *transport;
  char *protocol="HTTPS";
  char rootdir[2048];
  char *accessListFile;

  // get options 
  webster->rootdir=strdup(getOption(context,"DocumentRoot","web"));
  if(*webster->rootdir!='/'){
    sprintf(rootdir,"%s/%s",webster->dir,webster->rootdir);
    free(webster->rootdir);
    webster->rootdir=strdup(rootdir);
    }
  nbLogMsg(context,0,'T',"DocumentRoot=%s",webster->rootdir);
  transport=strdup(getOption(context,"Protocol","HTTPS"));
  if(strcmp(transport,"HTTP")==0){
    protocol="HTTP";
    webster->authenticate=strdup(getOption(context,"Authenticate","no"));
    }
  else{  // change to function call ctx=getSSLContext(context);

  webster->authenticate=strdup(getOption(context,"Authenticate","yes"));

  // Set up the SSL context
  if(strcmp(transport,"HTTPS")==0)        method=SSLv23_server_method();        // Supports SSLv2, SSLv3, TLSv1
  else if(strcmp(transport,"TLSv1")==0) method=TLSv1_server_method();
  else if(strcmp(transport,"SSLv3")==0) method=SSLv3_server_method();    
  else if(strcmp(transport,"SSLv2")==0) method=SSLv2_server_method();    
  else{
    nbLogMsg(context,0,'E',"Secure option '%s' not recognized",transport);
    return(1);
    }
  ctx=SSL_CTX_new(method);  // use the SSL context pointer as the skill handle
  if(!ctx){
    nbLogMsg(context,0,'E',"Unable to establish SSL context");
    return(1);
    }

  // consider including hostname in the context
  //sprintf(ctxContext,"Webster:%s",webster->dir);
  sprintf(ctxContext,"Webster:%s","testing");
  //nbLogMsg(context,0,'T',"ctxContext=%s",ctxContext);
  if(!SSL_CTX_set_session_id_context(ctx,ctxContext,strlen(ctxContext))){
    nbLogMsg(context,0,'E',"Unable to set the session id context");
    return(1);
    }
  dataIndex=SSL_get_ex_new_index(0,"webster",NULL,NULL,NULL);
  //nbLogMsg(context,0,'I',"dataIndex=%d",dataIndex);

  // Set option so webster will request certificate from the client
  // Just require a recognized certificate
  if(strcmp(webster->authenticate,"certificate")==0)        // Require a certificate
    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,websterVerify);
  else if(strcmp(webster->authenticate,"yes")==0)           // Accept, but do not require a certificate---can check for HTTP user and password later
    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE,websterVerify);
  else if(strcmp(webster->authenticate,"password")==0)      // Don't request a certificate - request a user and password
    SSL_CTX_set_verify(ctx,SSL_VERIFY_NONE,websterVerify);
  else if(strcmp(webster->authenticate,"no")==0)            // Don't verify at all 
    SSL_CTX_set_verify(ctx,SSL_VERIFY_NONE,NULL);
  else{
    nbLogMsg(context,0,'E',"Authenticate option '%s' not recognized",webster->authenticate);
    return(1);
    }

  // As in Apache conf SSLCertificateChainFile
  if((strcmp(webster->authenticate,"certificate")==0 || strcmp(webster->authenticate,"yes")==0) && SSL_CTX_load_verify_locations(ctx,getOption(context,"TrustedCertificates","security/TrustedCertificates.pem"),NULL)<1){
    nbLogMsg(context,0,'E',"Unable to load trusted certificates file.\n");
    SSL_CTX_free(ctx);
    return(1);
    }

  // As in Apache conf SSLCertificateFile
  if(SSL_CTX_use_certificate_file(ctx,getOption(context,"ServerCertificate","security/ServerCertificate.pem"),SSL_FILETYPE_PEM)<1){
    nbLogMsg(context,0,'E',"Unable to load SSL certificate file");
    SSL_CTX_free(ctx);
    return(1);
    }

  // As in Apache conf SSLCertificateKeyFile
  //if(SSL_CTX_use_PrivateKey_file(ctx,"cipher.web.boeing.com.key",SSL_FILETYPE_PEM)<1){
  if(SSL_CTX_use_PrivateKey_file(ctx,getOption(context,"ServerKey","security/ServerKey.pem"),SSL_FILETYPE_PEM)<1){
    nbLogMsg(context,0,'E',"Unable to load SSL key file");
    SSL_CTX_free(ctx);
    return(1);
    }
  if(!SSL_CTX_check_private_key(ctx)){
    nbLogMsg(context,0,'E',"Private key does not match the certificate public key\n");
    SSL_CTX_free(ctx);
    return(1);
    }
  }

  webster->ctx=ctx;

  // Load authorized user tree
  accessListFile=getOption(context,"AccessList","security/AccessList.conf");
  websterLoadAccessList(context,webster,accessListFile);
  
  // Set up listener
  // we should use an API function here
  if((webster->socket=nbIpListen(webster->address,webster->port))<0){
    nbLogMsg(context,0,'E',"Unable to listen on %s:%u",webster->address,webster->port);
    return(1);
    }
  nbListenerAdd(context,webster->socket,webster,websterAccept);
  nbLogMsg(context,0,'I',"Listening for %s connections as %s@%s:%u",protocol,webster->idName,webster->address,webster->port);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
static int websterDisable(nbCELL context,void *skillHandle,nbWebster *webster){
  nbListenerRemove(context,webster->socket);
#if defined(WIN32)
  closesocket(webster->socket);
#else
  close(webster->socket);
#endif
  webster->socket=0;
  free(webster->rootdir);
  free(webster->authenticate);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
static int *websterCommand(nbCELL context,void *skillHandle,nbWebster *webster,nbCELL arglist,char *text){
  /* process commands here */
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
  if(webster->socket!=0) websterDisable(context,skillHandle,webster);
  free(webster);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *websterBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  SSL_load_error_strings();   // readable error messages
  SSL_library_init();         // initialize library

  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,websterConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,websterDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,websterEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,websterCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,websterDestroy);
  return(NULL);
  }
