/*
* Copyright (C) 2007-2014 The Boeing Company
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbwebster.c
*
* Title:    NodeBrain Webster HTTP/HTTPS API
*
* Function:
*
*   This file provides a small general purpose web server with an API that
*   enables NodeBrain modules to implement internal resources.
*
* Synopsis:
*
*   nbWeb *nbWebsterOpen - initialize a web server structure and load configuration
*   int nbWebSetHandler - register handler (internal routine like a CGI program)
*   int nbWebsterEnable
*   int nbWebsterDisable
*   int nbWebsterClose
*
*   int nbWebGetHeader - get header parameter
*   int nbWebSetHeader - set header parameter
*   int nbWebGetParam - get cgi parameter
*   int nbWebConsumer - set consumer exit (IO level)
*   int nbWebProducer - set producer exit (IO level)
*   int nbWebReply - start reply (write header fields and then call producer)
*
* Exits:
*
*   // get the group for a userId
*   char *getGroup(context,web,session,char *userId);
*   // authorize a user for a given request
*   char *authorize(context,web,session,char *userId,char *group);
*   // fill a buffer with data
*   int produce(context,web,session,buffer,buflen);
*   // process a buffer of data - buflen is zero at end
*   int consume(context,web,session,buffer,buflen);
*   // start a transaction and return a transaction handle
*   void *handler(context,web,session,buffer,buflen);
*   
* Description:
*
*   The design goal for this API is to provide a reusable software layer for
*   web applications that is specific to the NodeBrain environment, taking
*   advantage of the NodeBrain Medulla for "threads" and the NodeBrain
*   interpreter for configuration.
*
*   As a small general purpose web server, this facility enables a node to
*   use a standard approach to providing web content for a user interface.
*   Static HTML pages and CGI scripts are supported.  However, beyond that,
*   this facility deviates from the conventional web server based application
*   development approach.  Instead of plugging application components into a
*   powerful web server, the web server is treated as a small subordinate
*   component controlled by a NodeBrain plug-in and related configuration
*   files.
*
*   The following concepts are important.
*
*     Client authentication
*       Certificate based (trusted certificates)
*
*     Client authorization to use a webster port
*       IP address based (iptables)
*       Certificate based (subject and issuer)
*       URL Resource based (path and parameters)
*
*     User authentication
*       Certificate based (trusted certificates subject mapped to userId)
*       Password based (userId is entered)
*       Application exit (e.g. OTP)
*
*     User authorization
*       UserId maps to single group
*       Group, path and parameters
*       
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2007/07/27 Ed Trettevik (version 0.6.8 prototype in Webster module)
* 2011/05/06 eat 0.8.5 - restructured as API for use by other modules
* 2011/10/16 eat 0.8.6 - included resource filter
* 2011/10/22 eat 0.8.6 - config file and NB_WEBSTER_CONFIG environment variable
*            This enables multiple websters servers in a common caboodle
*            to identify unique configuration files.  Previously all CGI
*            programs and the Webster plugin assumed the configuration file
*            was config/caboodle.conf.
* 2012-01-03 eat 0.8.6 - fixed proxy feature to shutdown connection to server
*            When a client disconnects the backend connection to the server
*            is now shutdown.  These connections where previously accumulating
*            by mistake.
* 2012-02-12 eat 0.8.7 - changed version and updated "not found" message
* 2012-02-16 eat 0.8.7 - included trace option to reduce log volume
*            Need to make more use of it.  Right now it was include to avoid
*            displaying requests which may contain something sensitive.  This
*            can still be done with a "set websterTrace" command.
* 2012-02-18 eat 0.8.7 - leading and trailing spaces removed from parameter values
*            This will be a problem for any application that needs to support
*            parameters with leading or trailing spaces.  However, the advantage
*            of relieving application from the task of stripping them seems
*            worth risking a problem for others.  We can add an option if 
*            necessary.
* 2012-03-03 eat 0.8.7 - Fixed defect on first packet shorter than verbs GET|POST
* 2012-04-22 eat 0.8.8 - version change
* 2012-05-11 eat 0.8.9 - Implemented support for "Connection: close" header option
* 2012-05-16 eat 0.8.9 - Fixed some defects in forwarding logic
*            1) Stopped loading openssl context on every forward connection to
*               improve performance and stop related memory leak
*            2) No longer put data to forward proxy after full request assembled.
*               Instead, let the forward proxy producer pull the data when the
*               forward proxy handshake is complete and ready for write.
*            3) Wait to create forward proxy connection until full request is
*               assembled and passes the filter. This protects the forward
*               site from connects for bad requests, however it reduces
*               concurrency, adding a slight delay in response time.  This will
*               be improved by maitaining a pool of connections to the target
*               sight---but that will require converting "Connection: close"
*               to "Connection: keep-alive" or some other mechanism (e.g. port
*               option) to tell the server not to close connections from the
*               proxy.
*            Note: There is still a memory leak when forwarding.
* 2012-08-26 eat 0.8.10 - Stopped echo of URL not found to avoid XSS vulnerability
* 2012-09-17 eat 0.8.11 - Fixed some buffer overflows
* 2012-10-13 eat 0.8.12 - Replaced malloc/free with nbAlloc/nbFree
* 2012-12-15 eat 0.8.13 - Checker updates
* 2012-12-25 eat 0.8.13 - Appliation Security Tickets (AST)
* 2012-12-25 eat 0.8.13 - AST 49,50,51 - removed Webster versions from Server header
* 2012-12-25 eat 0.8.13 - AST 8 - switch charset to local character set (e.g. UTC-8)
* 2012-12-27 eat 0.8.13 - Checker updates
* 2013-01-01 eat 0.8.13 - Checker updates
* 2014-01-12 eat 0.9.00 - Added checks for chdir and getcwd return codes
*==============================================================================
*/
#include <nb/nbi.h>

int nb_websterTrace;          // debugging trace flag for webster routines

static int nbWebsterShutdownProducer(nbCELL context,nbProxy *proxy,void *handle);
//=============================================================================
// Routines for managing GET and POST parameters 
//=============================================================================

/*
*  Convert parameter tab string to buffer for easy
*  searching and stepping
*
*/
static int nbWebsterParamBuffer(char *pTab,char *pBuf,int pBuflen){
  char *cursor=pBuf,*curEnd=pBuf+pBuflen-2;
  char *delim;
  int len;
  int step;

  while(*pTab){
    delim=strchr(pTab,'=');
    if(delim){
      //fprintf(stderr,"nbWebsterParamBuffer: param=%s\n",pTab);
      len=delim-pTab;
      if(cursor+len>=curEnd) return(-2);
      *cursor=len+2;
      cursor++;
      strncpy(cursor,pTab,len);
      cursor+=len;
      *cursor=0;
      cursor++;
      pTab=delim;
      pTab++;
      }
    else{
      //fprintf(stderr,"nbWebsterParamBuffer: returning with *pTab=%2.2x\n",*pTab);
      return(-1);
      }
    delim=strchr(pTab,'\t');
    if(!delim) delim=strchr(pTab,0);
    //fprintf(stderr,"nbWebsterParamBuffer: value=%s\n",pTab);
    len=delim-pTab;
    while(len>0 && *pTab==' ') pTab++,len--;
    while(len>0 && *(pTab+len-1)==' ') len--;
    if(cursor+len>=curEnd) return(-2);
    step=len+2;   // 1 less than actual step because we inc cursor in picking up 2 byte length
    *cursor=step>>8;
    cursor++;
    *cursor=step&0xff;
    cursor++;
    strncpy(cursor,pTab,len);
    cursor+=len;
    *cursor=0;
    cursor++;
    pTab=delim;
    if(*pTab) pTab++;
    }
  *cursor=0;
  //fprintf(stderr,"nbWebsterParamBuffer: returning nicely\n");
  return(0);
  }

/*
*  Parameter  Encode
*
*    This is for encoding strings for a query.  The intent is for cases
*    where
*/
char *nbWebsterParameterEncode(nbCELL context,nbWebSession *session,char *plain,char *encoded,int len){
  char *ecur=encoded,*eend=encoded+len;
  char *cursor=plain;

  nbLogMsg(context,0,'T',"nbWebsterParameterEncode: plain:%s",plain);
  while(*cursor && ecur<eend-4){
    if(*cursor==' '){strcpy(ecur,"%20");ecur+=2;}
    else if(*cursor=='#'){strcpy(ecur,"%23");ecur+=2;}
    else if(*cursor=='%'){strcpy(ecur,"%25");ecur+=2;}
    else if(*cursor=='&'){strcpy(ecur,"%26");ecur+=2;}
    else if(*cursor=='+'){strcpy(ecur,"%2b");ecur+=2;}
    else if(*cursor==';'){strcpy(ecur,"%3b");ecur+=2;}
    else *ecur=*cursor;
    cursor++;
    ecur++;
    }
  if(*cursor) return(NULL); // didn't have enough room in encoding buffer
  *ecur=0;
  nbLogMsg(context,0,'T',"nbWebsterParameterEncode: encoded:%s",encoded);
  return(encoded);
  }

/*
*  Parameter Decode
*
*    This is different from a query decode (nbWebsterParamDecode).  Here we assume we
*    are operating on a single parameter and do not intepret ";" or "&" as parameter
*    separators.
*/
char *nbWebsterParameterDecode(nbCELL context,nbWebSession *session,char *encoded,char *plain,int len){
  char *hexalpha="0123456789ABCDEF";
  char *h,*l;
  unsigned char n;
  char *dcur=plain,*dend=plain+len;
  char *cursor=encoded;

  nbLogMsg(context,0,'T',"nbWebsterParameterDecode: encoded:%s",encoded);
  while(*cursor && dcur<dend-1){
    if(*cursor=='+') *dcur=' ';
    else if(*cursor=='%' && (h=strchr(hexalpha,*(cursor+1))) && (l=strchr(hexalpha,*(cursor+2)))){
      cursor+=2;
      n=(h-hexalpha)<<4;
      n+=l-hexalpha;
      *dcur=n;
      }
    else *dcur=*cursor;
    cursor++;
    dcur++;
    }
  *dcur=0;   // 2012-12-25 eat - AST 5 - reversed order of this an next statement
  if(*cursor) return(NULL); // didn't have enough room in plain buffer
  nbLogMsg(context,0,'T',"nbWebsterParameterDecode: plain:%s",plain);
  return(plain);
  }

static int nbWebsterParamDecode(nbCELL context,nbWebSession *session,char *encoded,char *pBuf,int pBuflen){
  char decoded[NB_BUFSIZE];
  int len=NB_BUFSIZE;
  char *hexalpha="0123456789ABCDEF";
  char *h,*l;
  unsigned char n;
  char *dcur=decoded,*dend=decoded+len;
  char *cursor=encoded;

  //nbLogMsg(context,0,'T',"nbWebsterParamDecode: encoded -  %s",encoded);
  while(*cursor && dcur<dend-1){
    if(*cursor==';') *dcur='\t';      // 2011-05-16 eat - alternative to '&'
    else if(*cursor=='&') *dcur='\t';
    else if(*cursor=='+') *dcur=' ';
    else if(*cursor=='%' && (h=strchr(hexalpha,*(cursor+1))) && (l=strchr(hexalpha,*(cursor+2)))){
      cursor+=2;
      n=(h-hexalpha)<<4;
      n+=l-hexalpha;
      *dcur=n;
      }
    else *dcur=*cursor;
    cursor++;
    dcur++;
    }
  if(*cursor) return(-1); // didn't have enough room in decoded buffer
  *dcur=0;
  //nbLogMsg(context,0,'T',"Parameters decoded: %s",decoded);
  return(nbWebsterParamBuffer(decoded,pBuf,pBuflen));
  }

//=============================================================================
// Old functions from Webster module
//=============================================================================

/*
* Decode the command
*
* The request is broken up into parts, each terminated by a null character.
*
*   request   GET or POST
*   query     GET only
*   headers   GET or POST
*   content   POST only
*
* Returns:
*  -1 - error
*   0 - success
*   1 - don't have full request try again
*
* this is used by both blocking and non-blocking versions
*/
static int nbWebsterDecodeRequest(nbCELL context,nbWebSession *session,char *request,int reqlen){
  char *cursor,*delim,*end=request+reqlen;
  int len;
  char value[512];

  if(nb_websterTrace) nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: called - len=%d",reqlen);
  if(reqlen>=sizeof(session->request)){
    nbLogMsg(context,0,'E',"Request is too large for buffer");
    return(-1);
    }
  // 2012-05-11 - make sure we have a full request before processing
  if(reqlen<6) return(1); // 2012-03-03 eat - wait for more data if too short to check verb
  delim=memchr(request,'\n',reqlen);
  if(!delim) return(1);
  cursor=delim+1;
  while(cursor<end && *cursor!='\r' && *cursor!='\n'){
    delim=memchr(cursor,'\n',reqlen);
    if(!delim) return(1);
    cursor=delim+1;
    }
  if(cursor>=end) return(1);
  if(*cursor=='\r'){
    cursor++;
    if(cursor>end || *cursor!='\n') return(-1);
    }

  memcpy(session->request,request,reqlen);
  *(session->request+reqlen)=0;
  cursor=session->request;

  // initialize side effect values in case we bail out on error
  *session->reqauth=0;
  session->content=NULL;
  session->contentLength=-1;
  strcpy(session->reqhost,"?");
  session->resource="?";
  if(nb_websterTrace) nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: get resource");
  // decode the URL
  if(strncmp(cursor,"GET /",5)==0){
    len=5;
    session->method=NB_WEBSTER_METHOD_GET;
    }
  else if(strncmp(cursor,"POST /",6)==0){
    len=6;
    session->method=NB_WEBSTER_METHOD_POST;
    }
  else return(-1);
  if(nb_websterTrace) nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: have method=%d get=%d post=%d len=%d",session->method,NB_WEBSTER_METHOD_GET,NB_WEBSTER_METHOD_POST,len);
  cursor+=len;
  session->resource=cursor;  // save address of resource portion
  while(*cursor && *cursor!='?' && *cursor!=' ') cursor++;
  if(*cursor=='?'){
    *cursor=0;
    cursor++;
    session->queryString=cursor;
    while(*cursor && *cursor!=' ') cursor++;
    }
  else session->queryString=NULL;
  *cursor=0;  // end of resource or queryString
  cursor++;
  if(*session->resource==0){
    session->resource=session->webster->indexPage;
    session->queryString=session->webster->indexQuery;
    }
  session->headerfields=cursor;
  if(nb_websterTrace) nbLogMsg(context,0,'T',"Header fields:\n%s\n",session->headerfields);

  // process header entries until an empty line;

  if(session->cookiesIn){
    free(session->cookiesIn);
    session->cookiesIn=NULL; // 2011-08-13 eat - this was missing
    }
  cursor=strchr(cursor,'\n');
  if(cursor==NULL) return(-1);
  cursor++;
  while(cursor && *cursor && *cursor!='\r' && *cursor!='\n'){
    delim=strchr(cursor,'\n');
    if(!delim) delim=strchr(cursor,0);
    else if(delim>cursor && *(delim-1)=='\r') delim--;
    if(strncasecmp(cursor,"Host: ",6)==0){
      cursor+=6;
      len=delim-cursor;
      if(len>=sizeof(session->reqhost)) len=sizeof(session->reqhost)-1;  // 2012-12-25 eat - AST 63 // 2012-12-27 eat - CID 761995
      strncpy(session->reqhost,cursor,len);
      *(session->reqhost+len)=0;
      }
    else if(strncasecmp(cursor,"Authorization: Basic ",21)==0){
      cursor+=21;
      len=delim-cursor;
      if(len>=sizeof(session->reqauth)) len=sizeof(session->reqauth)-1; // 2012-12-25 eat - AST 63 // 2012-12-27 eat - CID 761996
      strncpy(session->reqauth,cursor,len);
      *(session->reqauth+len)=0;
      }
    else if(strncasecmp(cursor,"Connection: close",17)==0){
      cursor+=17;
      session->close=1;
      }
    else if(strncasecmp(cursor,"Content-Length: ",16)==0){
      cursor+=16;
      len=delim-cursor;
      strncpy(value,cursor,len);
      *(value+len)=0;
      session->contentLength=atoi(value);
      nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: Content-Length is %d",session->contentLength);
      }
    else if(strncasecmp(cursor,"Cookie: ",8)==0){
      cursor+=8;
      len=delim-cursor;
      session->cookiesIn=strdup(cursor);
      nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: Cookies received - %s",session->cookiesIn);
      }
    cursor=delim;
    if(*cursor=='\r') cursor++;
    if(*cursor=='\n') cursor++;
    }
  if(!cursor || !*cursor){
    nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: did not find empty line terminating header - cursor=%p",cursor);
    if(cursor) nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: did not find empty line terminating header - *cursor=%s",cursor);
    return(1); // we didn't find empty line
    }
  if(*cursor=='\r') cursor++;
  if(*cursor=='\n') cursor++;
  if(session->contentLength>0){
    int consumed=cursor-session->request;
    nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: consumed=%d of %d leaving %d of %d content",consumed,reqlen,reqlen-consumed,session->contentLength);
    if(session->contentLength>reqlen-consumed){
      nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: do not have all the content yet");
      return(1); // don't have all the content
      }
    }
  else if(session->contentLength<0){
    if(session->method==NB_WEBSTER_METHOD_POST){
      nbLogMsg(context,0,'T',"nbWebsterDecodeRequest: post requires content length - waiting for more");
      return(1);
      }
    else session->contentLength=0;  // default to zero
    }
  session->content=cursor;
  return(0);
  }

// try it using the nbProxy routines
static void nbWebsterError(nbCELL context,nbWebSession *session,char *text){
  char content[1024];
  int contentLength;
  nbProxyPage *page;
  void *data;
  size_t size;
  int len;
  const char *html=    // 2012-12-25 eat - AST 7 - removed client supplied resource from reply html
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
    "<html>\n<head>\n"
    "<title>500 Internal Server Error</title>\n"
    "</head>\n<body>\n"
    "<b><big>500 Internal Server Error</big></b>\n"
    "<p>The server encountered an internal error and was unable to complete your request.</p>\n"
    "<p>If you think the request is valid, please contact the webmaster\n"
    "<hr>\n%s\n"
    "<i>NodeBrain Webster Server</i>\n"
    "</body>\n</html>\n";
  const char *response=   // 2012-12-25 eat - AST 27 - removed client supplied resource from reply html
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster\r\n"
    "Location: https://%s/%s\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: text/html; charset=%s\r\n\r\n"
    "%s";
  time_t currentTime;
  char ctimeCurrent[32];
  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  page=nbProxyPageOpen(context,&data,&size);
  nbLogMsg(context,0,'T',"Internal server error");
  len=snprintf(content,sizeof(content),html,text);
  if(len>=sizeof(content)) sprintf(content,html,session->resource,""); // 2012-12-25 eat - AST 6
  contentLength=strlen(content);
  len=snprintf((char *)data,size,response,ctimeCurrent,session->reqhost,session->resource,contentLength,nb_charset,content);
  if(len>=size) sprintf((char *)data,response,"","",contentLength,content); // 2012-12-25 eat - AST 26
  nbLogMsg(context,0,'T',"Returning:\n%s\n",data);
  nbProxyPageProduced(context,page,strlen((char *)data));
  nbProxyPutPage(context,session->client,page);
  }

// try it using the nbProxy routines
static void nbWebsterBadRequest(nbCELL context,nbWebSession *session,char *text){
  nbProxyPage *page;
  void *data;
  size_t size;
  int   len;
  const char *html=    // 2012-12-25 eat - AST 28 - removed client supplied request from the reply html
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
    "<html>\n<head>\n"
    "<title>400 Bad Request</title>\n"
    "</head>\n<body>\n"
    "<b><big>400 Bad Request</big></b>\n"
    "<p>The server encountered an unsupported request.</p>\n"
    "<hr>\n"
    "<i>NodeBrain Webster Server</i>\n"
    "</body>\n</html>\n";
  const char *response=  // 2012-12-25 eat - AST 31 - removed client supplied request from the reply html
    "HTTP/1.1 400 Bad Request\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster\r\n"
    "Location: https://%s/%s\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: text/html; charset=%s\r\n\r\n"
    "%s";
  time_t currentTime;
  char ctimeCurrent[32];
  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  page=nbProxyPageOpen(context,&data,&size);
  nbLogMsg(context,0,'T',"Internal server error");
  len=snprintf((char *)data,size,response,ctimeCurrent,session->reqhost,session->resource,strlen(html),nb_charset,html); // 2012-12-27 eat CID 761997
  if(len>=size) sprintf((char *)data,response,ctimeCurrent,"","",strlen(html),nb_charset,html);  // 2012-12-25 eat - AST 29,30 // 2012-12-27 eat CID 761998
  nbLogMsg(context,0,'T',"Returning:\n%s\n",data);
  nbProxyPageProduced(context,page,strlen((char *)data));
  nbProxyPutPage(context,session->client,page);
  }


static void webContentHeading(nbCELL context,nbWebSession *session,char *code,char *type,char *subtype,int length){
  char ctimeCurrent[32],ctimeExpires[32];
  time_t currentTime;
  nbProxyPage *page;
  void *data;
  size_t size;
  char *connection="keep-alive";
  int len;

  if(session->close) connection="close";
  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  currentTime+=24*60*60; // add one day
  strncpy(ctimeExpires,asctime(gmtime(&currentTime)),sizeof(ctimeExpires));
  *(ctimeExpires+strlen(ctimeExpires)-1)=0;

  page=nbProxyPageOpen(context,&data,&size);
  len=snprintf(data,size,
    "HTTP/1.1 %s\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster\r\n"
    "Last-Modified: %s\r\n"
    "Expires: %s\r\n"
    "Connection: %s\r\n"
    "Accept-Ranges: none\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: %s/%s\r\n\r\n",
    code,ctimeCurrent,ctimeCurrent,ctimeExpires,connection,length,type,subtype);
  if(len>=size) *((char *)data+size-1)=0; // 2012-12-25 eat - AST 9 
  nbLogMsg(context,0,'T',"webContentHeading:");
  nbLogPut(context,data);
  nbProxyPageProduced(context,page,strlen((char *)data));
  nbProxyPutPage(context,session->client,page); 
  }

static int nbWebsterCgiCloser(nbPROCESS process,int pid,void *processSession){
  nbWebSession *session=processSession;
  nbCELL context=session->webster->context;
  char ctimeCurrent[32];
  time_t currentTime;
  nbProxyPage *page;
  void *data;
  size_t size;
  int length=0;
  char *code="200 OK";
  char *type="text"; // need to get his from the cgi returned header
  char *subtype="html"; // need to get this from the cgi returned header
  char msg[256];
  char *connection="keep-alive";

  nbLogMsg(context,0,'T',"nbWebsterCgiCloser: called exitcode=%d",process->exitcode);
  session->process=NULL;  // 2012-05-12 eat - remove the process from the session
  if(session->close) connection="close";
  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;

  // we're taking a bad approach here to prototype 
  for(page=session->book.readPage;page;page=page->next){
    length+=page->dataLen;
    } 
  nbLogMsg(context,0,'T',"nbWebsterCgiCloser: content length=%d",length);
  if(length==0 && process->exitcode!=0){
    snprintf(msg,sizeof(msg),"CGI program terminated - exit code=%d",process->exitcode);
    nbWebsterError(context,session,msg);
    return(0);
    }
  page=nbProxyPageOpen(context,&data,&size);
  snprintf(data,size,
    "HTTP/1.1 %s\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster\r\n"
    "Last-Modified: %s\r\n"
    "Connection: %s\r\n"
    "Accept-Ranges: none\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: %s/%s\r\n\r\n",
    code,ctimeCurrent,ctimeCurrent,connection,length,type,subtype);
  nbLogMsg(context,0,'T',"webContentHeading:");
  nbLogPut(context,data);
  nbProxyPageProduced(context,page,strlen((char *)data));
  // slip this page ahead - should be nbProxyBookPreface function
  page->next=session->book.readPage;
  session->book.readPage=page;
  // move this book to the proxy and let it go
  memcpy(&session->client->obook,&session->book,sizeof(nbProxyBook));
  memset(&session->book,0,sizeof(nbProxyBook));
  nbProxyProduced(context,session->client,0); // make it schedule
  return(0);
  }

// read a stderr line and write it to the log
static int nbWebsterCgiErrReader(nbPROCESS process,int pid,void *processSession,char *msg){
  nbWebSession *session=processSession;
  nbLogMsg(session->webster->context,0,'W',"%s",msg);
  return(0);
  }

// Pass cgi output to the client
static int nbWebsterCgiReader(nbPROCESS process,int pid,void *processSession,char *msg){
  nbWebSession *session=processSession;
  nbWebServer *webster=session->webster;
  nbCELL context=webster->context;
  static int lineno=3;  // silly switch while debugging heading
  void *data;
  int   size;
  int   len;

  // need to read header lines a save for later
  if(strncmp(msg,"Content-type:",13)==0) lineno=0;
  lineno++;
  if(lineno<3) return(0);
  //nbLogMsg(context,0,'T',"nbWebsterCgiReader: [%d,%d] %s",pid,session->client->tls->socket,msg);
  // this little block could be a new function nbProxyBookPutString
  len=strlen(msg);
  size=nbProxyBookWriteWhere(context,&session->book,&data);
  while(len+1>size){
    strncpy((char *)data,msg,size);
    msg+=size;
    len-=size;
    nbProxyBookProduced(context,&session->book,size);
    size=nbProxyBookWriteWhere(context,&session->book,&data);
    }
  strncpy((char *)data,msg,len);
  *((char *)data+len)='\n';
  nbProxyBookProduced(context,&session->book,len+1); 
  return(0);
  }

// message writer
static int nbWebsterCgiWriter(nbPROCESS process,int pid,void *processSession){
  nbWebSession *session=processSession;
  nbCELL context=session->webster->context;
  void *data;
  int   len;

  nbLogMsg(session->webster->context,0,'T',"cgiWriter called - expect=%d",session->contentLength);
  if(session->contentLength<=0) return(1);
  nbLogMsg(session->webster->context,0,'T',"Checking for more content");
  len=nbProxyBookReadWhere(context,&session->client->ibook,&data);
  if(len==0) return(1);
  *(((char *)data)+len)=0;  // this is goofy
  nbLogMsg(session->webster->context,0,'T',"Sending content:\n%s\n",(char *)data);
  nbMedullaProcessPut(session->process,(char *)data);
  nbProxyConsumed(context,session->client,len);
  session->contentLength-=len;
  nbLogMsg(session->webster->context,0,'T',"contentLength=%d\n",session->contentLength);
  return(0);
  }

static int nbWebsterCgi(nbCELL context,nbWebSession *session,char *file,char *queryString){
  nbWebServer *webster=session->webster;
  char buf[NB_BUFSIZE],dir[2048],*delim,value[512];
  char msg[NB_MSGSIZE];
  int len;

  nbProxyProducer(context,session->client,session,NULL); // remove producer
  nbProxyBookClose(context,&session->book);  // make sure we have a closed book
  // set environment variables for the cgi program
  setenv("SERVER_SOFTWARE","NodeBrain Webster/0.8.13",1);
  setenv("GATEWAY_INTERFACE","CGI/1.1",1);
  setenv("SERVER_PROTOCOL","HTTP/1.1",1);
  setenv("SSL_CLIENT_S_DN_CN",session->userid,1);
  if(*session->email) setenv("SSL_CLIENT_S_DN_Email",session->email,1);
  setenv("QUERY_STRING",queryString,1);
  setenv("NB_WEBSTER_CONFIG",webster->config,1);
  if(strlen(file)>=sizeof(dir)){
    nbLogMsg(context,0,'E',"Directory too large",buf);
    return(-1);
    }
  delim=file+strlen(file)-1;
  while(delim>file && *delim!='/') delim--;
  if(delim>file){
    if(delim-file>=sizeof(dir)){
      nbLogMsg(context,0,'E',"Directory too large",buf);
      return(-1);
      }
    strncpy(dir,file,delim-file);
    *(dir+(delim-file))=0;
    if(chdir(dir)<0){
      nbLogMsg(context,0,'E',"Unable to chdir to %s - %s",dir,strerror(errno));
      return(-1);
      }
    if(getcwd(dir,sizeof(dir))==NULL){
      nbLogMsg(context,0,'E',"Unable to obtain current working directory - %s",strerror(errno));
      return(-1);
      }
    nbLogMsg(context,0,'T',"During pwd=%s",dir);
    }
  if(session->method==NB_WEBSTER_METHOD_GET){
    setenv("REQUEST_METHOD","GET",1);
    len=snprintf(session->command,sizeof(session->command),"=|:$ %s/%s",webster->rootdir,file); // convert to medula command with full path
    if(len>=sizeof(session->command)){  // 2012-12-25 eat - AST 34
      nbLogMsg(context,0,'E',"Command too large for buffer",buf);
      return(-1);
      }
    nbLogMsg(context,0,'T',"CGI GET request: %s",session->command);
    session->process=nbMedullaProcessOpen(NB_CHILD_TERM|NB_CHILD_SESSION,session->command,"",session,nbWebsterCgiCloser,NULL,nbWebsterCgiReader,nbWebsterCgiErrReader,msg,sizeof(msg));
    if(session->process==NULL){
      nbLogMsg(context,0,'E',"%s",msg);
      return(-1);
      }
    }
  else if(session->method==NB_WEBSTER_METHOD_POST){
    setenv("REQUEST_METHOD","POST",1);
    nbLogMsg(context,0,'T',"CGI Post request: %s",session->command);
    setenv("CONTENT_TYPE","application/x-www-form-urlencoded",1);
    if(session->content){
      nbLogMsg(context,0,'T',"Post not having to scan for content length");
      sprintf(value,"%d",session->contentLength);
      setenv("CONTENT_LENGTH",value,1); 
      }
    len=snprintf(session->command,sizeof(session->command),"|=|:$ %s/%s",webster->rootdir,file); // convert to medula command with full path
    if(len>=sizeof(session->command)){  // 2012-12-25 eat - AST 35
      nbLogMsg(context,0,'E',"Command too large for buffer",buf);
      return(-1);
      }
    nbLogMsg(context,0,'T',"Post request: %s",session->command);
    session->process=nbMedullaProcessOpen(NB_CHILD_TERM|NB_CHILD_SESSION,session->command,"",session,nbWebsterCgiCloser,nbWebsterCgiWriter,nbWebsterCgiReader,nbWebsterCgiErrReader,msg,sizeof(msg));
    if(session->process==NULL){
      nbLogMsg(context,0,'E',"%s",msg);
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

static int nbWebsterFileProducer(nbCELL context,nbProxy *proxy,void *handle){
  nbWebSession *session=(nbWebSession *)handle;
  int len;
  nbProxyPage *page;
  void *data;
  size_t size;

  if(!session->fd){
    nbLogMsg(context,0,'L',"nbWebsterFileProducer: called without session->fd set");
    return(-1);
    }
  page=nbProxyPageOpen(context,&data,&size);
  len=read(session->fd,data,size);
  if(len){
    nbProxyPageProduced(context,page,len);
    nbProxyPutPage(context,session->client,page);
    }
  else{
    close(session->fd);
    session->fd=0;
    if(session->close) return(2);  // 2012-05-12 eat - think we should return 2 here
    }
  return(0);
  }

static void nbWebsterResourceNotFound(nbCELL context,nbWebServer *webster,nbWebSession *session){
  nbProxyPage *page;
  void *data;
  size_t size;
  int   len;
  const char *html=   // 2012-12-25 eat - AST 36 - removed client supplied resource from html reply
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
    "<html>\n<head>\n"
    "<title>404 Not Found</title>\n"
    "</head>\n<body>\n"
    "<h1>Not Found</h1>\n"
    "<p>The requested resource was not found on this server. "
    "<hr>\n"
    "<address>NodeBrain Webster Server</address>\n"
    "</body></html>\n";
  const char *response=  // 2012-12-25 eat - AST 50 - removed Webster version
    "HTTP/1.1 404 Not Found\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster\r\n"
    "Location: https://%s/%s\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: text/html; charset=%s\r\n\r\n"
    "%s";
  time_t currentTime;
  char ctimeCurrent[32];
  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  page=nbProxyPageOpen(context,&data,&size);
  len=snprintf((char *)data,size,response,ctimeCurrent,session->reqhost,session->resource,strlen(html),nb_charset,html); // 2012-12-27 eat CID 761999
  if(len>=size) sprintf((char *)data,response,ctimeCurrent,"","",strlen(html),nb_charset,html);  // 2012-12-25 eat - AST 33  // 2012-12-27 eat CID 762000
  nbLogMsg(context,0,'T',"Reply:\n%s\n",(char *)data);
  nbProxyPageProduced(context,page,strlen((char *)data));
  nbProxyPutPage(context,session->client,page);
  }

/*
* Little Web Server
*
* Here we handle requests as if Webster were a normal web server.  We
* don't pretend that it is a fully compliant web server, but provides enough
* basic functionality to support web content and cgi scripts included
* in NodeBrain kits.  The goal of Webster is to provide a web interface
* to a NodeBrain agent.  Web server functionality is included only for
* convenient integration with NodeBrain applications when the use of
* an industrial web server is not convenient.
*
* Note: This function can be turned into a default handler by making
* it conform to handler parameters and return value.  This would enable
* an application to replace the default handler, perhaps with one that
* applies rules and then forwards allowed requests to this default handler.
*/
static void webRequirePassword(nbCELL context,nbWebSession *session);
static void nbWebsterServe(nbCELL context,nbWebServer *webster,nbWebSession *session){
  struct stat filestat;      // file statistics
  int fildes;
  char filename[1024],content[NB_BUFSIZE],*cursor,*delim;
  nbWebResource *resource;
  int contentLength;
  nbProxyPage *page;
  void *data;
  size_t size;
  int rc;
  int len;

  nbLogMsg(context,0,'T',"webServer: called - resource='%s' queryString=%p",session->resource,session->queryString);
  nbProxyProducer(context,session->client,session,NULL);
  len=strlen(session->resource);
  if(len>=sizeof(filename)-10) len=sizeof(filename)-11; // leave room for "index.html"
  strncpy(filename,session->resource,len);
  *(filename+len)=0;
  if(*(filename+strlen(filename)-1)=='/') strcat(filename,"index.html");
  if(!session->queryString && strlen(session->resource)>4 && strncmp(session->resource+strlen(session->resource)-4,".cgi",4)==0) session->queryString="";
  nbLogMsg(context,0,'T',"webServer: filename=%s",filename);
  resource=nbWebsterFindResource(context,webster,filename);
  if(resource){
/* 2011-10-12 eat - this test needs to move to the request handlers
    if(session->role!=NB_WEBSTER_ROLE_ADMIN){
      webRequirePassword(context,session);
      return;
      }
*/
    if(session->queryString){
      //nbLogMsg(context,0,'T',"webServer: A queryString=%s",session->queryString);
      nbWebsterParamDecode(context,session,session->queryString,session->parameters,sizeof(session->parameters));
      //nbLogMsg(context,0,'T',"webServer: B queryString=%s",session->queryString);
      }
    else if(session->content) nbWebsterParamDecode(context,session,session->content,session->parameters,sizeof(session->parameters));
    else *session->parameters=0;
    nbProxyProducer(context,session->client,session,NULL); // remove producer
    nbProxyBookClose(context,&session->book);  // make sure we have a closed book
    if(!session->queryString) session->queryString="";
    if((rc=chdir(webster->dir))==0){ // let handler make own decision about directory
      rc=(*resource->handler)(context,session,resource->handle);
      if(chdir(webster->rootdir)<0) nbAbort("Webster unable to chdir back to content directory");
      }
    nbWebsterReply(context,session);
    // 2012-05-11 eat - experiment with close option - note need to be able to close session any point we finish a reply
    if(session->close) nbProxyProducer(context,session->client,session,nbWebsterShutdownProducer);
    return;
    }
  if(session->queryString){
    //nbLogMsg(context,0,'T',"webServer: queryString=%s",session->queryString);
    if(0>nbWebsterCgi(context,session,filename,session->queryString)){
      const char *html=
        "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
        "<html>\n<head>\n"
        "<title>500 Internal Server Error</title>\n"
        "</head>\n<body>\n"
        "<b><big>500 Internal Server Error</big></b>\n"
        "<p>The server encountered an internal error and was unable to complete your request.</p>\n"
        "<p>If you think the resource is valid, please contact the webmaster\n"
        "<hr>\n"
        "<i>NodeBrain Webster Server</i>\n"
        "</body>\n</html>\n";
      const char *response=
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Date: %s\r\n"
        "Server: NodeBrain Webster\r\n"
        "Location: https://%s/%s\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: text/html; charset=%s\r\n\r\n"
        "%s";
      time_t currentTime;
      char ctimeCurrent[32];
      time(&currentTime);
      strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
      *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
      nbLogMsg(context,0,'T',"Error returned by webCgi");
      page=nbProxyPageOpen(context,&data,&size);
      len=snprintf((char *)data,size,response,ctimeCurrent,session->reqhost,filename,strlen(html),nb_charset,html); // 2012-12-27 eat - CID 762002
      if(len>=size) sprintf((char *)data,response,ctimeCurrent,"","",strlen(html),nb_charset,html);
      nbLogMsg(context,0,'T',"Reply:\n%s\n",(char *)data);
      nbProxyPageProduced(context,page,strlen((char *)data));
      nbProxyPutPage(context,session->client,page);
      }
    return;
    }
#if defined(WIN32)
  else if(stat(filename,&filestat)==0 && (filestat.st_mode&_S_IFDIR)){
#else
  else if(stat(filename,&filestat)==0 && S_ISDIR(filestat.st_mode)){
#endif
    const char *html=
      "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
      "<html><head>\n"
      "<title>301 Moved Permanently</title>\n"
      "</head><body>\n"
      "<h1>Moved Permanently</h1>\n"
      "<p>The document has moved <a href='https://%s/%s/index.html'>here</a>.</p>\n"
      "<hr/>\n"
      "<address>NodeBrain Webster Server</address>\n"
      "</body>\n</html>\n";
    const char *response=
      "HTTP/1.1 301 Moved Permanently\r\n"
      "Date: %s\r\n"
      "Server: NodeBrain Webster\r\n"
      "Location: https://%s/%s/index.html\r\n"
      "Connection: close\r\n"
      "Content-Length: %d\r\n"
      "Content-Type: text/html; charset=%s\r\n\r\n"
      "%s";
    time_t currentTime;
    char ctimeCurrent[32];
    time(&currentTime);
    strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
    *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
    page=nbProxyPageOpen(context,&data,&size);
    len=snprintf(content,sizeof(content),html,session->reqhost,filename);
    if(len>=sizeof(content)) sprintf(content,html,"","");
    contentLength=strlen(content);
    len=snprintf((char *)data,size,response,ctimeCurrent,session->reqhost,filename,contentLength,nb_charset,content);
    if(len>=size) sprintf((char *)data,response,"","",contentLength,content);
    nbLogMsg(context,0,'T',"Reply:\n%s\n",(char *)data);
    nbProxyPageProduced(context,page,strlen((char *)data));
    nbProxyPutPage(context,session->client,page);
    }
#if defined(WIN32)
  else if((fildes=open(filename,O_RDONLY|O_BINARY))<0){
#else
  else if((fildes=open(filename,O_RDONLY))<0){
#endif
    const char *html=
      "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
      "<html>\n<head>\n"
      "<title>404 Not Found</title>\n"
      "</head>\n<body>\n"
      "<h1>Not Found</h1>\n"
      "<p>The requested resource was not found on this server.</p>\n"
      "<hr>\n"
      "<address>NodeBrain Webster</address>\n"
      "</body></html>\n";
    const char *response=
      "HTTP/1.1 404 Not Found\r\n"
      "Date: %s\r\n"
      "Server: NodeBrain Webster\r\n"
      "Location: https://%s/%s/index.html\r\n"
      "Connection: close\r\n"
      "Content-Length: %d\r\n"
      "Content-Type: text/html; charset=%s\r\n\r\n"
      "%s";
    time_t currentTime;
    char ctimeCurrent[32];
    time(&currentTime);
    strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
    *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
    page=nbProxyPageOpen(context,&data,&size);
    len=snprintf((char *)data,size,response,ctimeCurrent,session->reqhost,filename,strlen(html),nb_charset,html); // 2012-12-27 eat - CID 762001
    if(len>=size) sprintf((char *)data,response,ctimeCurrent,"","",strlen(html),nb_charset,html); // 2012-12-27 eat - fixed 
    nbLogMsg(context,0,'T',"Reply:\n%s\n",(char *)data);
    nbProxyPageProduced(context,page,strlen((char *)data));
    nbProxyPutPage(context,session->client,page);
    }
  else{
    cursor=filename;                  // look for file ext
    delim=strchr(cursor,'.');
    while(delim!=NULL){
      cursor=delim+1;
      delim=strchr(cursor,'.');
      }
    // This needs to be cleaned up using a lookup tree
    nbLogMsg(context,0,'T',"File name: %s",filename);
    nbLogMsg(context,0,'T',"File extension: %s",cursor);
    session->fd=fildes;
    nbLogMsg(context,0,'T',"Session fd=%d",session->fd);
    if(strcmp(cursor,"html")==0 || strcmp(cursor,"htm")==0)
      webContentHeading(context,session,"200 OK","text","html",filestat.st_size);
    else if(strcmp(cursor,"pdf")==0) webContentHeading(context,session,"200 OK","application","pdf",filestat.st_size);
    else if(strcmp(cursor,"jar")==0) webContentHeading(context,session,"200 OK","application","java-archive",filestat.st_size);
    else if(strcmp(cursor,"class")==0) webContentHeading(context,session,"200 OK","application","java-byte-code",filestat.st_size);
    else if(strcmp(cursor,"js")==0) webContentHeading(context,session,"200 OK","application","x-javascript",filestat.st_size);
    else if(strcmp(cursor,"text")==0 || strcmp(cursor,"txt")==0)
        webContentHeading(context,session,"200 OK","text","plain",filestat.st_size);
    else if(strcmp(cursor,"ico")==0) webContentHeading(context,session,"200 OK","image","vnd.microsoft.icon",filestat.st_size);
    else if(strcmp(cursor,"css")==0) webContentHeading(context,session,"200 OK","text","css",filestat.st_size);
    // gif, png, jpg - all others are images
    else webContentHeading(context,session,"200 OK","image",cursor,filestat.st_size);
    nbLogMsg(context,0,'T',"webServer: context=%p session=%p",context,session);
    nbProxyProducer(context,session->client,session,nbWebsterFileProducer);
    }
  }

// Provide a response telling the client we need a user and password

static void webRequirePassword(nbCELL context,nbWebSession *session){
  nbProxyPage *page;
  void *data;
  size_t size;
  int   len;
  const char *html=
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
    "<ADDRESS>NodeBrain Webster Server</ADDRESS>\n"
    "</BODY></HTML>\n\n";
  const char *response=
    "HTTP/1.1 401 Authorization Required\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster\r\n"
    "WWW-Authenticate: Basic realm=\"Webster\"\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: text/html; charset=%s\r\n\r\n"
    "%s";
  time_t currentTime;
  char ctimeCurrent[32];
  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  page=nbProxyPageOpen(context,&data,&size);
  len=snprintf((char *)data,size,response,ctimeCurrent,strlen(html),nb_charset,html);
  if(len>=size) nbExit("Logic error in webRequirePassword - static content exceeds page size");
  nbLogMsg(context,0,'T',"nbWebRequirePassword: sending response");
  nbLogPut(context,"%s",(char *)data);
  nbProxyPageProduced(context,page,strlen((char *)data));
  nbProxyPutPage(context,session->client,page);
  }

static int getRoleByPassword(nbCELL context,nbWebSession *session){
  nbWebUser *userNode;
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

static int nbWebGetRoleByCertificate(nbCELL context,nbWebSession *session){
  X509 *clientCertificate;
  char *x509str;
  char buffer[2048];
  char *cursor,*delim;
  int  len;
  nbWebUser *userNode;
  nbCELL key;

  *session->userid=0;
  clientCertificate=SSL_get_peer_certificate(session->client->tls->ssl);
  if(clientCertificate==NULL) nbLogMsg(context,0,'T',"Client certificate not found");
  else{
    nbLogMsg(context,0,'T',"Client certificate found");
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
    cursor=strstr(buffer,"/emailAddress=");
    if(cursor){
      cursor+=14;
      delim=strchr(cursor,'/');
      if(!delim) len=strlen(cursor);
      else len=delim-cursor;
      if(len>=sizeof(session->email)) len=sizeof(session->email)-1;
      strncpy(session->email,cursor,len);
      *(session->email+len)=0;
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
  nbLogMsg(context,0,'I',"userid '%s' not found",session->userid);
  return(getRoleByPassword(context,session));   
  //return(NB_WEBSTER_ROLE_REJECT);
  }

// Check to see if X509 certificate authentication was successful
//   We always make it successful by returning 1, but will request a password via HTTP if necessary

/*   // 2012-12-25 eat - AST 52
static int websterVerify(int preverify_ok,X509_STORE_CTX *ctx){
  SSL *ssl;
  //X509 *clientCertificate;
  nbWebServer *webster;
  nbWebSession *session;
  //char *x509str;
  nbCELL context;
  //char buffer[2048];

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
*/

/*
*  Get option specified as string term in context
*
*  name:  Option name
*  value: Default value returned if term not found
*/
/*    // 2012-12-25 eat - AST 53
static char *getOption(nbCELL context,char *name,char *defaultValue){
  char *value=defaultValue;
  nbCELL cell;
  if((cell=nbTermLocate(context,name))!=NULL
    && (cell=nbTermGetDefinition(context,cell))!=NULL
    && nbCellGetType(context,cell)==NB_TYPE_STRING) value=nbCellGetString(context,cell);
  nbLogMsg(context,0,'T',"%s=%s",name,value);
  return(value);
  }
*/

/*
*  Load access list
*/
static int websterLoadAccessList(nbCELL context,nbWebServer *webster,char *filename){
  FILE *accessFile;
  char buffer[1024],*cursor,*delim;
  char role;
  nbWebUser *userNode;
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
        // 2012-10-13 eat - replaced malloc
        userNode=nbAlloc(sizeof(nbWebUser));
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

//============================================================================================
// nbproxy API exit routines
//============================================================================================
 
static void nbWebsterShutdownAccept(nbCELL context,nbProxy *proxy,void *handle,int code){
  nbWebServer *webster=(nbWebServer *)handle;
  nbLogMsg(context,0,'T',"nbWebsterShutdownAccept: Connection %s is shutting down",nbTlsGetUri(proxy->tls));
  nbLogMsg(context,0,'T',"nbWebsterShutdownAccept: webster=%p",webster);
  }

static void nbWebsterShutdown(nbCELL context,nbProxy *proxy,void *handle,int code){
  nbWebSession *session=(nbWebSession *)handle;
  nbWebServer *webster=session->webster;
  nbLogMsg(context,0,'T',"nbWebsterShutdown: Connection %s is shutting down",nbTlsGetUri(proxy->tls));
  nbLogMsg(context,0,'T',"nbWebsterShutdown: handle/session=%p webster=%p",session,webster);
  nbLogMsg(context,0,'T',"nbWebsterShutdown: webster->handler=%p webster->handle=%p",webster->handler,webster->handle);
  // 2012-01-03 eat - see if we can shutdown the other connection
  if(proxy->other){  // we are called for the client and other should be the server
    if(proxy->other->other) proxy->other->other=NULL;
    nbProxyShutdown(context,proxy->other,0);
    }
  if(webster->handler && webster->handle) (*webster->handler)(context,session->handle,1);
  // 2012-05-12 eat - included code to free up more things associated with the session
  if(session->fd) close(session->fd);
  if(session->cookiesIn) free(session->cookiesIn);
  if(session->cookiesOut) free(session->cookiesOut);
  nbProxyBookClose(context,&session->book);  // make sure we have a closed book
  if(session->process){
    nbLogMsg(context,0,'W',"Shutting down Webster session while process is still running\n");
    session->process->closer=NULL; // cancel nbWebsterCgiCloser
    }
  // 2012-10-13 eat - replaced free
  nbFree(session,sizeof(nbWebSession));
  nbLogMsg(context,0,'T',"nbWebsterShutdown: Connection is shut down");
  }

// 2012-05-11 eat - included to enable shutdown
static int nbWebsterShutdownProducer(nbCELL context,nbProxy *proxy,void *handle){
  return(2); // tell the proxy to shutdown after the buffer is consumed
  }

static int nbWebsterRequest(nbCELL context,nbProxy *proxy,void *handle){
  nbWebSession *session=(nbWebSession *)handle;
  nbWebServer *webster=session->webster;
  void *data;
  int   len;
  int   rc;
  nbProxyPage *page;
  nbCELL filterClass=NULL;

  session->type="text";     // initial type and subtype
  session->subtype="html";  // handlers may override with call to nbWebsterSetType
  len=nbProxyBookReadWhere(context,&session->client->ibook,&data);
  nbLogMsg(context,0,'T',"nbWebsterRequest: len=%d Connection %s ",len,nbTlsGetUri(proxy->tls));
  *(((char *)data)+len)=0;  // null terminate the request
  if(nb_websterTrace) nbLogPut(context,"]%s\n",(char *)data);
  nbLogMsg(context,0,'T',"nbWebsterRequest: calling nbWebsterDecodeRequest");
  if((rc=nbWebsterDecodeRequest(context,session,(char *)data,len))!=0){
    if(rc<0){
      nbWebsterBadRequest(context,session,"Sorry, no hints.");
      nbLogMsg(context,0,'T',"nbWebsterRequest: nbWebsterDecodeRequest returned non-zero");
      //return(-1); // force end of session
      return(0); // let's not force end of session
      }
    nbLogMsg(context,0,'T',"nbWebsterRequest: did not get the full request - waiting for more input");
    return(0); // need content wait for call again
    }
  nbLogMsg(context,0,'T',"nbWebsterRequest: nbWebsterDecodeRequest returned zero");
  //if(!session->server) nbProxyConsumed(context,session->client,len);
  if(!webster->forwardUri) nbProxyConsumed(context,session->client,len);
  // experimenting with filter
  if(webster->filter){
    nbLogMsg(context,0,'T',"nbWebsterRequest: calling filter\n%s",data);
    filterClass=nbTranslatorExecute(context,webster->filter,data);
    if(!filterClass || filterClass==NB_CELL_UNKNOWN){
      nbLogMsg(context,0,'T',"nbWebsterRequest: filter denied request");
      nbWebsterResourceNotFound(context,webster,session);
      return(0); // force end of session
      } 
    nbLogMsg(context,0,'T',"nbWebsterRequest: filter accepted request");
    }
  // now let's figure what to do
  //nbProxySend(context,session->proxy,session->request);     // use to inspect the client request
  session->role=NB_WEBSTER_ROLE_GUEST;
  if(strcmp(webster->authenticate,"no")!=0){
    if(strcmp(webster->authenticate,"password")==0) session->role=getRoleByPassword(context,session);
    else session->role=nbWebGetRoleByCertificate(context,session);
    if(session->role==NB_WEBSTER_ROLE_REJECT){
      nbLogMsg(context,0,'T',"webServer: requesting password");
      webRequirePassword(context,session);  // tell client we need a password
      // 2010-10-06 eat - experiment
      //if(!session->server) nbProxyConsumed(context,session->client,len);
      return(0);
      }
    }
  // call validation function
  nbLogMsg(context,0,'T',"Request: %s",session->request);
  // if we are a proxy service, forward to backend server
  //if(session->server){
  if(webster->forwardUri){
    if(!session->server){
      // For now create a connection each time.  In the future, maintain a pool of connections
      session->server=nbProxyConnect(context,webster->forwardTlsx,webster->forwardUri,session,NULL,NULL,NULL);
      if(!session->server){
        nbLogMsg(context,0,'T',"Unable to establish connection to server for %s",nbTlsGetUri(proxy->tls));
        return(-1);
        }
      nbProxyForward(context,session->client,session->server,0x27);
      }
    else{
      nbLogMsg(context,0,'T',"nbWebsterRequest: calling nbProxyGetPage");
      page=nbProxyGetPage(context,session->client);
      nbLogMsg(context,0,'T',"nbWebsterRequest: nbProxyGetPage returned page=%p",page);
      while(page){
        nbLogMsg(context,0,'T',"nbWebsterRequest: calling nbProxyPutPage");
        rc=nbProxyPutPage(context,session->server,page);
        nbLogMsg(context,0,'T',"nbWebsterRequest: nbProxyPutPage returned code=%d",rc);
        page=nbProxyGetPage(context,session->client);
        nbLogMsg(context,0,'T',"nbWebsterRequest: nbProxyGetPage returned page=%p",page);
        }
      }
    nbLogMsg(context,0,'T',"nbWebsterRequest: returning");
    return(0);
    }
  // ok, just act like a little web server
  if(chdir(webster->rootdir)==0){
    nbWebsterServe(context,webster,session);
    if(chdir(webster->dir)<0) nbAbort("Webster unable to chdir back to document directory");
    }
  return(0);
  }

static int nbWebsterAccept(nbCELL context,nbProxy *proxy,void *handle){
  nbWebServer *webster=(nbWebServer *)handle;
  nbWebSession *session;

  nbLogMsg(context,0,'T',"nbWebsterAccept: Connection %s",nbTlsGetUri(proxy->tls));
  nbLogMsg(context,0,'T',"nbWebsterAccept: webster=%p",webster);
  // 2012-10-13 eat - replaced malloc
  session=(nbWebSession *)nbAlloc(sizeof(nbWebSession));
  memset(session,0,sizeof(nbWebSession));
  session->webster=webster;
  session->role=NB_WEBSTER_ROLE_REJECT;
  *session->userid=0;
  session->contentLength=0;
  session->content=NULL;
  session->client=proxy;
  nbLogMsg(context,0,'T',"nbWebsterAccept: webster->handler=%p webstger->handle=%p",webster->handler,webster->handle);
  if(webster->handler) session->handle=(*webster->handler)(context,webster->handle,0);
  //session=(nbWebSession *)nbAlloc(sizeof(nbWebSession));
  else session->handle=handle;
  nbLogMsg(context,0,'T',"nbWebsterAccept: session=%p session->handle=%p",session,session->handle);
/*
  if(webster->forwardContext){
    session->server=nbProxyConnect(context,webster->forwardTlsx,webster->forwardUri,session,NULL,NULL,NULL);
    if(!session->server){
      nbLogMsg(context,0,'T',"Unable to establish connection to server for %s",nbTlsGetUri(proxy->tls));
      return(-1);
      } 
    nbProxyForward(context,session->client,session->server,0x35);
    }
*/
  nbProxyModify(context,proxy,session,NULL,nbWebsterRequest,nbWebsterShutdown);
  nbLogMsg(context,0,'T',"nbWebsterAccept: calling nbWebsterRequest");
  return(nbWebsterRequest(context,proxy,session));
  }

//============================================================================================
//  API
//============================================================================================

/*
*  Open a web server
*
*/
//nbWebServer *nbWebsterOpen(nbCELL context,void *handle,char *interface,short int port){
nbWebServer *nbWebsterOpen(nbCELL context,nbCELL siteContext,void *handle,
  void *(*handler)(nbCELL context,void *handle,int operation)){
  nbWebServer *webster;
  char *delim;

  // 2012-10-13 eat - replaced malloc
  webster=nbAlloc(sizeof(nbWebServer));
  memset(webster,0,sizeof(nbWebServer));
  webster->context=context;
  webster->siteContext=siteContext;
  webster->handle=handle;
  webster->handler=handler;
  if(getcwd(webster->dir,sizeof(webster->dir))==NULL) *webster->dir=0;
  delim=webster->dir;
  while((delim=strchr(delim,'\\'))!=NULL) *delim='/',delim++; // Unix the path
  webster->rootdir=strdup(webster->dir);
  if(!webster->rootdir) nbExit("nbWebsterOpen: out of memory"); // 2013-01-01 eat - VID 5327-0.8.13-1
  webster->userTree=NULL;
  webster->resource=(nbWebResource *)nbAlloc(sizeof(nbWebResource));
  memset(webster->resource,0,sizeof(nbWebResource));
  return(webster);
  }

char *nbWebsterGetConfig(nbCELL context,nbWebServer *webster){
  return(webster->config);
  }

char *nbWebsterGetRootDir(nbCELL context,nbWebServer *webster){
  return(webster->rootdir);
  }

void *nbWebsterGetHandle(nbCELL context,nbWebSession *session){
  nbLogMsg(context,0,'T',"nbWebsterGetHandle: called");
  return(session->webster->handle);
  }

void *nbWebsterGetSessionHandle(nbCELL context,nbWebSession *session){
  nbLogMsg(context,0,'T',"nbWebsterGetSessionHandle: called");
  return(session->handle);
  }

char *nbWebsterGetHost(nbCELL context,nbWebSession *session){
  return(session->reqhost);
  }

char *nbWebsterGetDir(nbCELL context,nbWebSession *session){
  return(session->webster->dir);
  }

char *nbWebsterGetCookies(nbCELL context,nbWebSession *session){
  return(session->cookiesIn);
  }

void nbWebsterSetCookies(nbCELL context,nbWebSession *session,char *cookies){
  if(session->cookiesOut) free(session->cookiesOut);
  session->cookiesOut=strdup(cookies);
  if(!session->cookiesOut) nbExit("out of memory"); // 2013-01-01 eat - VID 5453-0.8.13-1
  }

void nbWebsterSetType(nbCELL context,nbWebSession *session,char *type,char *subtype){
  session->type=type;       
  session->subtype=subtype;
  }

void nbWebsterSetExpires(nbCELL context,nbWebSession *session,int seconds){
  session->expires=seconds;
  }

/*
*  Enable Web Server
*/
int nbWebsterEnable(nbCELL context,nbWebServer *webster){
  //char *transport;
  //char *protocol="HTTPS";
  char rootdir[2048];
  char *accessListFile;
  char *filterFilename;
  int len;

  // get options
  webster->rootdir=strdup(nbTermOptionString(context,"DocumentRoot","web"));
  if(*webster->rootdir!='/'){
    len=snprintf(rootdir,sizeof(rootdir),"%s/%s",webster->dir,webster->rootdir);
    if(len>=sizeof(rootdir)){
      nbLogMsg(context,0,'E',"nbWebsterEnable: DocumentRoot path is too long for buffer - max=%d",sizeof(rootdir)-1);
      return(1);
      }
    free(webster->rootdir);
    webster->rootdir=strdup(rootdir);
    }
  //nbLogMsg(context,0,'T',"DocumentRoot=%s",webster->rootdir);

  webster->config=strdup(nbTermOptionString(context,"Config",""));
  webster->indexPage=nbTermOptionString(context,"IndexPage","index.html");
  webster->indexQuery=nbTermOptionString(context,"IndexQuery",NULL);
  //transport=strdup(nbTermOptionString(context,"Protocol","HTTPS"));
  //if(strcmp(transport,"HTTP")==0){
  //  protocol="HTTP";
  //  webster->authenticate=strdup(nbTermOptionString(context,"Authenticate","no"));
  //  }
  //else{  // change to function call ctx=getSSLContext(context);

  webster->authenticate=strdup(nbTermOptionString(context,"Authenticate","yes"));

//  webster->ctx=ctx;

  // Load authorized user tree
  accessListFile=nbTermOptionString(context,"AccessList","security/AccessList.conf");
  websterLoadAccessList(context,webster,accessListFile);

  // Set up forwarding if specified
  webster->forwardContext=nbTermLocateHere(webster->siteContext,"forward"); // see if we need to forward
  if(webster->forwardContext){
    webster->forwardUri=nbTermOptionString(webster->forwardContext,"uri","https://0.0.0.0:49443");
    if(!*webster->forwardUri){
      nbLogMsg(context,0,'E',"nbWebsterEnable: uri not defined in forward context");
      return(1);
      }
    webster->forwardTlsx=nbTlsLoadContext(context,webster->forwardContext,NULL,1);
    if(!webster->forwardTlsx){
      nbLogMsg(context,0,'E',"nbWebsterEnable: unable to create forward TLS context");
      return(1);
      }
    }
  filterFilename=nbTermOptionString(webster->siteContext,"Filter","");
  if(*filterFilename){
    webster->filter=nbTranslatorCompile(webster->siteContext,PCRE_MULTILINE,filterFilename);
    if(!webster->filter){
      nbLogMsg(context,0,'E',"nbWebsterEnable: Unable to load filter translator");
      return(1);
      }
    }
  // Set up listener
  webster->server=nbProxyConstruct(context,0,webster->siteContext,webster,NULL,nbWebsterAccept,nbWebsterShutdownAccept);
  if(!webster->server){
    nbLogMsg(context,0,'E',"nbWebsterEnable: Unable to create server");
    return(1);
    }
  if(nbProxyListen(context,webster->server)){  // listening if returns zero
    nbLogMsg(context,0,'E',"nbWebsterEnable: Server unable to listen");
    return(1);
    }
  nbLogMsg(context,0,'I',"Listening for Webster connections as %s",nbTlsGetUri(webster->server->tls));
  return(0);
  }

int nbWebsterRegisterResource(nbCELL context,nbWebServer *webster,char *name,void *handle,
  int (*handler)(nbCELL context,struct NB_WEB_SESSION *session,void *handle)){
  nbWebResource *resource=webster->resource,*res;
  char *cursor,*delim;
  char qualifier[512];
  int  len;
  NB_TreePath path;
  nbCELL key;
   
  nbLogMsg(context,0,'T',"nbWebsterRegisterResource: called with name='%s'",name);
  cursor=name;
  while(*cursor!=0){
    delim=strchr(cursor,'/');  
    if(!delim) delim=strchr(cursor,0);
    len=delim-cursor;
    if(len>=sizeof(qualifier)){
      nbLogMsg(context,0,'E',"Web resource path contains qualifier larger than buffer size at-->%s",cursor);
      return(-1);
      }
    strncpy(qualifier,cursor,len);
    *(qualifier+len)=0;
    key=nbCellCreateString(context,qualifier);
    if((res=nbTreeLocate(&path,key,(NB_TreeNode **)&resource->child))==NULL){
      // 2012-10-13 eat - replaced malloc
      resource=nbAlloc(sizeof(nbWebResource));
      resource->node.left=NULL;
      resource->node.right=NULL;
      resource->child=NULL;
      resource->handle=NULL;  
      resource->handler=NULL;
      nbTreeInsert(&path,(NB_TreeNode *)resource);
      }
    else resource=res;
    // find node
    cursor=delim;
    if(*cursor=='/') cursor++;
    }   
  resource->handle=handle;
  resource->handler=handler; 
  //nbLogMsg(context,0,'T',"nbWebsterResourceRegister: returning resource=%p resource->handle=%p",resource,resource->handle);
  return(0);
  }

nbWebResource *nbWebsterFindResource(nbCELL context,nbWebServer *webster,char *name){
  nbWebResource *resource=webster->resource;
  char *cursor,*delim;
  char qualifier[512];
  int  len;
  nbCELL key;

  cursor=name;
  while(*cursor!=0){
    delim=strchr(cursor,'/'); 
    if(!delim) delim=cursor+strlen(cursor); // 2013-01-01 eat - VID 5451-0.8.13-1 FP but changed from delim=strchr(cursor,'/'); to help checker
    len=delim-cursor;
    if(len>=sizeof(qualifier)){
      nbLogMsg(context,0,'E',"Web resource path contains qualifier larger than buffer size at-->%s",cursor);
      return(NULL);
      }
    strncpy(qualifier,cursor,len);
    *(qualifier+len)=0;
    key=nbCellCreateString(context,qualifier);
    resource=nbTreeFind(key,(NB_TreeNode *)resource->child);
    if(!resource) return(NULL);
    cursor=delim;
    if(*cursor=='/') cursor++;
    }
  return(resource);
  }

char *nbWebsterGetResource(nbCELL context,nbWebSession *session){
  nbLogMsg(context,0,'T',"nbWebsterGetResource: session=%p",session);
  nbLogMsg(context,0,'T',"nbWebsterGetResource: session->resource=%p",session->resource);
  return(session->resource);
  }

char *nbWebsterGetQuery(nbCELL context,nbWebSession *session){
  //if(session->queryString) nbLogMsg(context,0,'T',"nbWebsterGetQuery: session=%p &queryString=%p queryString=%s",session,session->queryString,session->queryString);
  return(session->queryString);
  }

char *nbWebsterGetParameters(nbCELL context,nbWebSession *session){
  return(session->parameters);
  }
/*
*  Get a parameter value
*/
char *nbWebsterGetParam(nbCELL context,nbWebSession *session,char *param){
  char *cursor;
  int step;
  char *pBuf=session->parameters;

  //nbLogMsg(context,0,'T',"nbWebsterParamGet: param=%s",param);
  cursor=pBuf;
  while(*cursor){
    //nbLogMsg(context,0,'T',"nbWebsterParamGet: param=%s",cursor+1);
    if(strcasecmp(cursor+1,param)==0){
      cursor+=(unsigned char)*cursor;
      //nbLogMsg(context,0,'T',"nbWebsterParamGet: value=%s\n",cursor+2);
      return(cursor+2);
      }
    cursor+=(unsigned char)*cursor;
    step=(unsigned char)*cursor;
    step=step<<8;
    cursor++;
    step+=(unsigned char)*cursor;
    cursor+=step;
    }
  return(NULL);
  }

/*
*  Get next instance of a parameter value
*    set char *pCursor=NULL and pass &pCursor to get first instance
*/
char *nbWebsterGetParamNext(nbCELL context,nbWebSession *session,char *param,char **pCursor){
  char *cursor;
  int step;

  cursor=*pCursor;
  if(!cursor) cursor=session->parameters;
  while(*cursor){
    //fprintf(stderr,"nbWebsterParamGetNext: param=%s\n",cursor+1);
    if(strcmp(cursor+1,param)==0){
      cursor+=(unsigned char)*cursor;
      //fprintf(stderr,"nbWebsterParamGetNext: value=%s\n",cursor+2);
      step=(unsigned char)*cursor;
      step=step<<8;
      cursor++;
      step+=(unsigned char)*cursor;
      *pCursor=cursor+step;
      return(cursor+1);
      }
    cursor+=(unsigned char)*cursor;
    step=(unsigned char)*cursor;
    step=step<<8;
    cursor++;
    step+=(unsigned char)*cursor;
    cursor+=step;
    }
  return(NULL);
  }

int *nbWebsterPut(nbCELL context,nbWebSession *session,void *buffer,int len){
  void *data;
  int size;

  size=nbProxyBookWriteWhere(context,&session->book,&data);
  while(len>size){
    memcpy(data,buffer,size);
    buffer+=size;
    len-=size;
    nbProxyBookProduced(context,&session->book,size);
    size=nbProxyBookWriteWhere(context,&session->book,&data);
    }
  memcpy(data,buffer,len);
  nbProxyBookProduced(context,&session->book,len);
  return(0);
  }

int *nbWebsterPutText(nbCELL context,nbWebSession *session,char *text){
  void *data;
  size_t size;
  size_t len=strlen(text);

  size=nbProxyBookWriteWhere(context,&session->book,&data);
  while(len>size){
    memcpy(data,text,size);
    text+=size;
    len-=size;
    nbProxyBookProduced(context,&session->book,size);
    size=nbProxyBookWriteWhere(context,&session->book,&data);
    }
  memcpy(data,text,len);
  nbProxyBookProduced(context,&session->book,len);
  return(0);
  }

/*
* Generate header and send reply
*
*   This will evolve.  Keeping it simple for now.
*/
int *nbWebsterReply(nbCELL context,nbWebSession *session){
  char ctimeCurrent[32];
  char ctimeExpires[32];
  char *ctimeModified=ctimeCurrent;
  char *expires=ctimeCurrent;
  time_t currentTime,expiresTime;
  nbProxyPage *page;
  void *data;
  size_t size;
  int length=0;
  char *code="200 OK";
  //char *type="text"; // need to get his from the cgi returned header
  //char *subtype="html"; // need to get this from the cgi returned header
  char *cookies="";
  char *connection="keep-alive";
  char *charsetlabel="; charset=";
  char *charset=nb_charset;

  nbLogMsg(context,0,'T',"nbWebsterReply: called");
  if(strcmp(session->type,"text")) charsetlabel="",charset="";
  if(session->close) connection="close";
  time(&currentTime);
  strncpy(ctimeCurrent,asctime(gmtime(&currentTime)),sizeof(ctimeCurrent));
  *(ctimeCurrent+strlen(ctimeCurrent)-1)=0;
  if(session->expires){
    expiresTime=currentTime+session->expires;
    strncpy(ctimeExpires,asctime(gmtime(&expiresTime)),sizeof(ctimeExpires));
    expires=ctimeExpires;
    }
  // get content length
  for(page=session->book.readPage;page;page=page->next){
    length+=page->dataLen;
    }
  nbLogMsg(context,0,'T',"nbWebsterReply: content length=%d",length);
  page=nbProxyPageOpen(context,&data,&size);
  if(session->cookiesOut) cookies=session->cookiesOut;
  snprintf(data,size,
    "HTTP/1.1 %s\r\n"
    "Date: %s\r\n"
    "Server: NodeBrain Webster\r\n"
    "Last-Modified: %s\r\n"
    "Expires: %s\r\n" 
    "Connection: %s\r\n"
    "Accept-Ranges: none\r\n"
    "%s"  // cookies
    "Content-Length: %d\r\n"
    "Content-Type: %s/%s%s%s\r\n\r\n",
    code,ctimeCurrent,ctimeModified,expires,connection,cookies,length,session->type,session->subtype,charsetlabel,charset);
  nbLogMsg(context,0,'T',"webContentHeading:");
  nbLogPut(context,data);
  nbProxyPageProduced(context,page,strlen((char *)data));
  // slip this page ahead - should be nbProxyBookPreface function
  page->next=session->book.readPage;
  session->book.readPage=page;
  // move this book to the proxy and let it go
  memcpy(&session->client->obook,&session->book,sizeof(nbProxyBook));
  memset(&session->book,0,sizeof(nbProxyBook));
  nbProxyProduced(context,session->client,0); // make it schedule
  if(session->cookiesOut){
    free(session->cookiesOut);
    session->cookiesOut=NULL;
    }
  session->expires=0;
  return(0);
  }

/*
*  Disable Web Server
*/
int nbWebsterDisable(nbCELL context,nbWebServer *webster){
  // include code to shutdown webster->server
  free(webster->rootdir);
  free(webster->authenticate);
  return(0);
  }

/*
*  Close Web Server
*/
nbWebServer *nbWebsterClose(nbCELL context,nbWebServer *webster){
  nbLogMsg(context,0,'T',"nbWebsterClose called");
  if(!webster) return(NULL);
  nbWebsterDisable(context,webster);
  // include code to clean up better
  // 2012-10-13 eat - replaced free()
  nbFree(webster,sizeof(nbWebServer));
  return(NULL);
  }
