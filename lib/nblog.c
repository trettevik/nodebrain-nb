/*
* Copyright (C) 1998-2013 The Boeing Company
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
* Program:  NodeBrain
*
* File:     nblog.c
*
* Title:    Log Stream Routines
*
* Function:
*
*   This header provides routines for managing and printing to multiple
*   log streams like stdout, log file, trace file, or a channel.  These
*   routines are required by most nb*.h headers.  The goal is to isolate
*   other code from the complexity of multiple simultaneous log streams.
*
* Synopsis:
*
*   #include "nblog.h"
*
*   void outInit();
*   void outStream(int stream,void (*handler)());
*   void outMsg(int id,char class,char *format,arg1,arg2,...);

*   void outHex(int length,char *buffer);
*   void outStamp();
*   void outFlush();
*
*   (API)
*
*   void nbLogBar(nbCELL context);
*   void nbLogDump(nbCell context,void *data,int len);
*   void nbLogFlush(nbCELL context);
*   void nbLogHandlerAdd(nbCELL context,...);
*   void nbLogHandlerRemove(nbCELL context,...);
*   void nbLogMsg(nbCELL context,int id,char class,char *format,arg1,arg2,...);
*   void nbLogPut(nbCELL context,char *format,arg1,arg2,...);
*   voit nbLogStr(nbCELL context,char *buffer);
*
* Description
*
*   The outInit function must be called before calling any of the
*   other functions in this header.
*
*   The outStream routine is used to register 1 to 3 parallel output
*   streams.  If you simply want all output to go to stdout, you don't
*   need to call this routine because stream 0 defaults to stdout.
*
*   If you want to cancel output to stdout, make the call below.
*
*     outStream(0,NULL);
*
*   If you want stream 0 to go to your own handler, you code a handler
*   and register for stream 0.
*
*     void myPrint(buffer) char * buffer; {
*       ... do what you want here ...
*       }
*
*     outStream(0,&myPrint);
*
*   If you want to add your handler as a parallel stream without disturbing
*   the current 0 stream, use a different stream number.
*
*     outStream(1,&myPrint);
*
*   To restore stdout to stream 0, re-register outStd.
*
*     outStream(0,&outStd);
*
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/06/28 Ed Trettevik (original prototype version)
*             1) This code has been moved from nodebrain.c and revised.  The
*             printReg feature should make it easier to test various components
*             of nodebrain independently.  Test drivers just need to include
*             this header to get standard output.
* 2002/02/02 eat - version 0.2.9g
*             1) implemented outPut to replace print0-4
*             2) renamed from nbio.h to nbout.h and renamed all functions
*                from print* to out*.
*             3) Included outMsg routine to eliminated time stamping in  
*                outPut based on recognition of message identifier.
* 2002/03/06 eat - version 0.3.0 A9
*             1) Included outStr() function for calls without a format string.
* 2004/10/10 eat 0.6.1  Added support for check scripts
*
* 2004/11/20 eat 0.6.2  Added nbLogHandlerAdd() and nbLogHandlerRemove()
*            These functions are included in the API to support modules that 
*            need to capture NodeBrain output.  The NodeBrain Console (Java) 
*            support module nb_mod_console uses this to redirect output back
*            to a the console.  It is possible to have multiple consoles
*            attached at the same time.  
*
* 2005/03/26 eat 0.6.2  Included nbLogDump()
* 2008/11/11 eat 0.7.3  Changed "bail" exit code to 255.
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-01-26 dtl 0.8.6  Checker updates
* 2012-05-12 eat 0.8.9  Increased width of dump lines from 16 to 32
* 2012-09-16 eat 0.8.11 Replaced vsprintf with vsnprintf to avoid buffer overflows
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2012-10-17 eat 0.8.12 Replaced termGetName with nbTermName
* 2012-12-25 eat 0.8.13 AST 3
* 2013-01-11 eat 0.8.13 Checker updates
*=============================================================================
*/
#include <nb/nbi.h>

#include <stdarg.h>

int  trace=0;               /* debugging trace flag */

char *nb_OutLine;           /* temporary line buffer */
char *nb_OutBuffer;         /* output buffer */
char *nb_OutCursor;         /* cursor for nb_OutBuffer */

char *nb_OutDirName;        /* output directory name - for jobs */
char *nb_OutLogName;        /* log file name - for daemons */
char *nb_OutUserDir;        /* user directory - for private.nb etc. */

int   nb_OutCheckTime=0;    /* check line counter */
char *nb_OutCheckBuf=NULL;  /* check buffer */
char *nb_OutCheckCur;       /* check buffer cursor */

void (*OUT_stream0)()=NULL; /* Registered output stream handlers */
void (*OUT_stream1)()=NULL;   
void (*OUT_stream2)()=NULL;

struct NB_OUTPUT_HANDLER *nb_OutputHandler=NULL;
struct NB_OUTPUT_HANDLER *nb_OutputHandlerFree=NULL;

/**********************************************************************
*  Routines
**********************************************************************/
/*
*  Default stream handler - prints to STDERR (was STDOUT)
*/
void outStd(char *buffer){
  fprintf(stderr,"%s",buffer);
  }

/*
*  Inititalize buffers and pointers for output streams
*    0 - Called previously
*    1 - Successful
*/
int outInit(void){
  if(nb_OutBuffer!=NULL) return(0);
  nb_OutLine=(char *)nbAlloc(NB_BUFSIZE);
  nb_OutBuffer=(char *)nbAlloc(NB_BUFSIZE);
  nb_OutCursor=nb_OutBuffer;
  OUT_stream0=&outStd;
  nb_OutDirName=(char *)nbAlloc(512);
  *nb_OutDirName=0;
  nb_OutLogName=(char *)nbAlloc(512);
  *nb_OutLogName=0;
  nb_OutUserDir=(char *)nbAlloc(512);
  *nb_OutUserDir=0;
  return(1);
  }

/*
*  Flush output buffer to output handlers
*
*  2004/11/20 - Included output handler list.  We should now replace OUT_stream2
*               (used by NBP) with an output handler on the list.  We can also
*               replace OUT_stream1 (used for tee log).  It is probably a good 
*               idea to keep the primary handler first, although it may be best
*               to build it into this routine.
*               
*/
void outFlush(void){
  struct NB_OUTPUT_HANDLER *outputHandler;
  if(nb_OutCursor==nb_OutBuffer) return;
  *nb_OutCursor=0;
  if(OUT_stream0!=NULL) (OUT_stream0)(nb_OutBuffer);
  if(OUT_stream1!=NULL) (OUT_stream1)(nb_OutBuffer);
  if(OUT_stream2!=NULL) (OUT_stream2)(nb_OutBuffer);
  fflush(NULL);  /* 2002/02/19 */
  for(outputHandler=nb_OutputHandler;outputHandler!=NULL;outputHandler=outputHandler->next){
    (*outputHandler->handler)(outputHandler->context,outputHandler->session,nb_OutBuffer);
    }
  if(nb_mode_check&1){  /* collect in odd modes */
    if(strlen(nb_OutBuffer)>(size_t)NB_BUFSIZE-(nb_OutCheckCur-nb_OutCheckBuf)){
      outMsg(0,'L',"Check error - output exceeds buffer size");
      exit(NB_EXITCODE_FAIL);
      }
    strcat(nb_OutCheckCur,nb_OutBuffer);
    nb_OutCheckCur=strchr(nb_OutCheckCur,0);
    }
  nb_OutCursor=nb_OutBuffer;
  *nb_OutCursor=0;
  }

/* 
*  Reset the check buffer 
*
*  opt:  0 - reset check buffer
*        1 - verify, write and reset check buffer
*
*  file: new check script file
*/
void outCheckReset(int opt,FILE *file){
  char *cursor;

  outFlush(); /* make sure we have all the output in check buffer */
  if(opt){
    if(nb_mode_check&1 && nb_OutCheckCur!=nb_OutCheckBuf){   /* never checked? */ 
      if(nb_mode_check==7) outMsg(0,'C',"Check error - output not checked");
      nb_OutCheckCur=nb_OutCheckBuf;
      }
    else if(nb_mode_check==6 && *nb_OutCheckCur!=0){           /* still checking? */
      outMsg(0,'C',"Check error - unchecked output remaining");
      }
    /* write the check buffer to new script */
    while(*nb_OutCheckCur!=0){
      if(file!=NULL) fprintf(file,"~ ");
      cursor=strchr(nb_OutCheckCur,'\n');
      if(cursor!=NULL){
        *cursor=0;
        if(file!=NULL) fprintf(file,"%s\n",nb_OutCheckCur);
        *cursor='\n';
        cursor++;
        nb_OutCheckCur=cursor;
        }
      else{
        if(file!=NULL) fprintf(file,"%s\n",nb_OutCheckCur);
        nb_OutCheckCur=strchr(nb_OutCheckCur,0);
        }
      }
    }
  /* reset */
  *nb_OutCheckBuf=0;
  nb_OutCheckCur=nb_OutCheckBuf;
  nb_OutCheckTime=0;
  nb_mode_check|=1;     /* start collecting again */
  nb_mode_check&=7;     /* turn off errors */
  }
 
void outBail(){
  outStamp();
  outPut("NB000I Bail option forced termination - exec code=%d\n",NB_EXITCODE_BAIL);
  outFlush();
  exit(NB_EXITCODE_BAIL);
  }
/*
*  Check script command handler
*
*  Note: We will have to handle nested check scripts.  For now
*  we are not  When we do, it will just be a matter of pushing the
*  control variable, including the file pointer, to a stack. Checking
*  on one level will be suspended while we check at the lower level.
*  There is no need to attempt checking at multiple levels concurrently.
*/
int outCheck(int option,char *cursor){
  static FILE *file;
  char filename[256];
  char *newline,*newlineIn;
  int n;

  switch(option){
    case NB_CHECK_START: /* Allocate a buffer and open an output file */
//    strcpy(filename,cursor);
//    strcat(filename,"~");
//2012-01-26 dtl: validate input string before use
      if(((n=strlen(cursor))+1)<sizeof(filename)) {
        strncpy(filename,cursor,n+1); //dtl: cp include 0 byte
        strcat(filename,"~");         //dtl: checked len
        }
      else {outMsg(0,'E',"Input too big, length= %d",n);return(1);} //dtl: return error
      if(nb_OutCheckBuf==NULL) nb_OutCheckBuf=nbAlloc(NB_BUFSIZE);
      *nb_OutCheckBuf=0;
      nb_OutCheckCur=nb_OutCheckBuf;
      if((file=fopen(filename,"w"))==NULL){
        outMsg(0,'E',"Unable to open new check script %s for output",filename);
        }
      outFlush();      /* clear the output buffer before we start collecting for check */
      nb_mode_check|=1;  /* start collecting */
      /* turn the enforce bit on if we are running a check script */
      if(*(cursor+strlen(cursor)-1)=='~') nb_mode_check|=2;
      break;
    case NB_CHECK_LINE:  /* parse a source file line */
      outFlush();   /* make sure we have all the output in the check buffer */
      if(*cursor=='~'){  /* check line */
        if(nb_mode_check>7) return(1); /* if we already have error let nbCmd() echo it */
        if(nb_mode_check&1) nb_OutCheckCur=nb_OutCheckBuf;
        nb_mode_check&=14;   /* stop collecting */
        outPut("%s",cursor);  /* we'll echo it differently */
        switch(*(cursor+1)){
          case '#': /* check buffer comment */
            if(file!=NULL) fprintf(file,"%s",cursor);
            break;
          case '^': /* reset check buffer */
            outCheckReset(0,file);
            if(file!=NULL) fprintf(file,"%s\n",cursor); /* write check command to new script */
            break;
          case ' ': /* match line */
            if(nb_mode_check&2){
              if(*nb_OutCheckCur==0){
                outMsg(0,'C',"Check error - check buffer is empty");
                }
              else{ /* compare line */
                newline=strchr(nb_OutCheckCur,'\n');
                if(newline!=NULL) *newline=0;
                if(file!=NULL) fprintf(file,"~ %s\n",nb_OutCheckCur);
                newlineIn=strchr(cursor,'\n');
                if(newlineIn!=NULL) *newlineIn=0;
                if(strcmp(nb_OutCheckCur,cursor+2)!=0){
                  outMsg(0,'C',"Check error - string does not match");
                  }
                if(newlineIn!=NULL) *newlineIn='\n';
                if(newline!=NULL) nb_OutCheckCur=newline+1;
                else nb_OutCheckCur=strchr(nb_OutCheckCur,0);
                }
              }
            break;
          default:
            outMsg(0,'C',"Check error - expecting '#', '^' or ' ' in column 2");
          }
        return(0);
        }
      else{         /* not a check line */
        outCheckReset(1,file); /* verify check buffer has been fully checked */
        fprintf(file,"%s",cursor);
        return(1);  /* nbCmd() should process this */
        }
      break;
    case NB_CHECK_STOP: /* handle end of file on check script */
      outCheckReset(1,file);  /* verify check buffer has been fully checked */
      fclose(file);
      nb_mode_check=0;
      break;
    default:
      outMsg(0,'L',"Unrecognized check operation");
      exit(NB_EXITCODE_FAIL);   
    }
  return(0);
  } 

/*
*  Register output stream handler
*    Use NULL handler to cancel
*/
void outStream(int stream,void (*handler)(char *buffer)){
  outFlush();
  switch(stream){
    case 0: OUT_stream0=handler; break;
    case 1: OUT_stream1=handler; break;
    case 2: OUT_stream2=handler; break;
    default:
      printf("NB000L outStream received invalid stream number - %d\n",stream);
    }
  }
  
/*
*  Print date and time stamp to output streams
*/
void outStamp(void){
  time_t systemTime;
  struct tm  *localTime;

  if(nb_mode_check>0 && nb_mode_check<8){
    systemTime=++nb_OutCheckTime;
    localTime=gmtime(&systemTime);
    }
  else{
    time(&systemTime);
    localTime=localtime(&systemTime);
    }
  sprintf(nb_OutCursor,"%.4d-%.2d-%.2d %.2d:%.2d:%.2d ",
    localTime->tm_year+1900,localTime->tm_mon+1,localTime->tm_mday,
    localTime->tm_hour,localTime->tm_min,localTime->tm_sec);
  nb_OutCursor+=20;
  }  

void nbLogStr(nbCELL context,char *string){
  int len;

  len=strlen(string);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE)) outFlush();
  while(len>=NB_BUFSIZE){
    memcpy(nb_OutCursor,string,NB_BUFSIZE);
    nb_OutCursor+=NB_BUFSIZE;
    outFlush();
    string+=NB_BUFSIZE;
    len-=NB_BUFSIZE;
    }
  if(len>0) strcpy(nb_OutCursor,string),nb_OutCursor+=len;
  }

void outData(char *data,size_t len){   // 2013-01-11 eat VID 6288,6696-0.8.13-2  - int to size_t
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE)) outFlush();
  while(len>=NB_BUFSIZE){
    memcpy(nb_OutCursor,data,NB_BUFSIZE);
    nb_OutCursor+=NB_BUFSIZE;
    outFlush();
    data+=NB_BUFSIZE;
    len-=NB_BUFSIZE;
    }
  if(len>0) memcpy(nb_OutCursor,data,len),nb_OutCursor+=len;
  }

/*
*  Print to output streams - like printf()
*
*    outPut(format,arg1,arg2,...)
*/
void outPut(const char * format,...){
  va_list args;
  int len;
  if(nb_opt_hush) return;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE)) outFlush();
  strcpy(nb_OutCursor,nb_OutLine);
  nb_OutCursor+=len;
  }

/*
* Temporary copy of outPut() for code we need to migrate from outPut() to nbLogPut()
* but which doesn't have a context to pass to nbLogPut() (e.g. nbprotocol.c used by
* the peer module).  This name is used to make them easy to find and to avoid
* exporting the internal function outPut().
*/
void nbLogPutI(char * format,...){
  va_list args;
  int len;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE)) outFlush();
  strcpy(nb_OutCursor,nb_OutLine);
  nb_OutCursor+=len;
  }

/*
*  Print to output streams in hex format for debugging
*/
void outHex(unsigned int l,void *buf){
  unsigned char *s=buf;
  unsigned char *cx,*ex;
  outPut("x[%u]'",l);
  ex=s+l;
  for(cx=s;cx<ex;cx++){
    outPut("%2.2x",*cx);
    }
  outPut("'");
  }

void outMsgHdr(int msgid,char msgclass,char *format,...){
  va_list args;
  int len;
  if(nb_opt_hush) return;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-20)) outFlush();
  outStamp();
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-256)) outFlush();
  if(msgid<0) sprintf(nb_OutCursor,"NX%3.3d%c %s",-msgid,msgclass,nb_OutLine);
  else sprintf(nb_OutCursor,"NB%3.3d%c %s",msgid,msgclass,nb_OutLine); 
  len=strlen(nb_OutCursor);
  nb_OutCursor+=len;
  }

/*
*  Print message to output streams and exit
*
*    nbExit(exitcode,msgclass,format,arg1,arg2,...);
*
*/
void nbExit(char *format,...){
  va_list args;
  int len;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-20)) outFlush();
  outStamp();
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-256)) outFlush();
  sprintf(nb_OutCursor,"NB998X %s",nb_OutLine);
  len=strlen(nb_OutCursor);
  nb_OutCursor+=len;
  *nb_OutCursor='\n';
  nb_OutCursor++;
  outFlush();
  exit(NB_EXITCODE_FAIL);
  }

/*
*  Print message to output streams and abort
*
*    nbAbort(msgid,msgclass,format,arg1,arg2,...);
*
*/
void nbAbort(const char *format,...){
  va_list args;
  int len;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-20)) outFlush();
  outStamp();
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-256)) outFlush();
  sprintf(nb_OutCursor,"NB999X %s",nb_OutLine);
  len=strlen(nb_OutCursor);
  nb_OutCursor+=len;
  *nb_OutCursor='\n';
  nb_OutCursor++;
  outFlush();
  abort();
  }


/*
*  Print message to output streams
*
*    outMsg(msgid,msgclass,format,arg1,arg2,...);
*
*/
void outMsg(int msgid,char msgclass,char *format,...){
  va_list args;
  int len;
  if(nb_opt_hush) return;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-20)) outFlush();
  if(msgclass=='C'){
    outFlush();
    nb_mode_check|=8;
    }
  outStamp();
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-256)) outFlush();
  if(msgid<0) sprintf(nb_OutCursor,"NX%3.3d%c %s",-msgid,msgclass,nb_OutLine);
  else sprintf(nb_OutCursor,"NB%3.3d%c %s",msgid,msgclass,nb_OutLine);
  len=strlen(nb_OutCursor);
  nb_OutCursor+=len;
  *nb_OutCursor='\n';
  nb_OutCursor++;
  if(msgclass=='T') outFlush();
  else if(msgclass=='E' || msgclass=='L' || msgclass=='C'){
    outFlush();
    if(nb_opt_bail==1 && (!nb_mode_check || msgclass=='C')) outBail();
    }
  }
// duplicate using exported name - callers should convert to nbLogMsg()
// this is not part of the API
void nbLogMsgI(int msgid,char msgclass,char *format,...){
  va_list args;
  int len;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-20)) outFlush();
  if(msgclass=='C'){
    outFlush();
    nb_mode_check|=8;
    }
  outStamp();
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-256)) outFlush();
  if(msgid<0) sprintf(nb_OutCursor,"NX%3.3d%c %s",-msgid,msgclass,nb_OutLine);
  else sprintf(nb_OutCursor,"NX%3.3d%c %s",msgid,msgclass,nb_OutLine); 
  len=strlen(nb_OutCursor);
  nb_OutCursor+=len;
  *nb_OutCursor='\n';
  nb_OutCursor++;
  if(msgclass=='T') outFlush();
  else if(msgclass=='E' || msgclass=='L' || msgclass=='C'){
    outFlush();
    if(nb_opt_bail==1 && (!nb_mode_check || msgclass=='C')) outBail();
    }
  }

/*
*
*/
void outBar(void){
  outPut("---------- --------\n");
  }

/* */

char *outDirName(char *name){
  if(name!=NULL) strcpy(nb_OutDirName,name);
  return(nb_OutDirName);
  }

char *outLogName(char *name){
  if(name!=NULL) strcpy(nb_OutLogName,name);
  return(nb_OutLogName);
  }

char *outUserDir(char *name){
  if(name!=NULL) strcpy(nb_OutUserDir,name);
  return(nb_OutUserDir);
  }

/* API Functions */

/*
* Quick Message API 
*   This is starting as a copy of outMsg().  We may have to
*   modify this interface based on user requests, and I want
*   outMsg() to remain stable. Message identifiers start with
*   NX here instead of NB, so we can tell where a message
*   came from.
*
*     nbLogMsg(context,msgid,msgclass,format,arg1,arg2,...);
*/
int nbLogMsg(nbCELL context,int msgNumber,char msgType,char *format,...){
  va_list args;
  int len;
  char termName[1024];
  //char *termName;
  char *skillName;
  if(context->object.type!=termType){
    outMsg(0,'L',"Skill module called nbLogMsg() with invalid context - ignoring call");
    return(-1);
    }
  nbTermName(termName,sizeof(termName),(NB_Term *)context,rootGloss);
  //termName=((NB_Term *)context)->word->value;
  if(((NB_Node *)(((NB_Term *)context)->def))->cell.object.type!=nb_NodeType){
    outMsg(0,'L',"Skill module called nbLogMsg() with term %s which is not a node - ignoring call",termName);
    return(-1);
    }
  if(((NB_Node *)((NB_Term*)context)->def)->skill!=NULL) skillName=((NB_Node *)((NB_Term*)context)->def)->skill->ident->value;
  else skillName="skull";
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-20)) outFlush();
  outStamp();
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE-256)) outFlush();
  sprintf(nb_OutCursor,"NM%3.3d%c %s %s: %s",msgNumber,msgType,skillName,termName,nb_OutLine);
  len=strlen(nb_OutCursor);
  nb_OutCursor+=len;
  *nb_OutCursor='\n';
  nb_OutCursor++;
  /* note: msgclass='C' is reserved for outMsg above */
  if(msgType=='T') outFlush();
  else if(msgType=='E' || msgType=='L'){
    outFlush();
    if(nb_opt_bail==1) outBail();
    }
  return(0);
  }

/*
*  Print to output streams - like printf()
*
*    nbLogPut(format,arg1,arg2,...)
*/
int nbLogPut(nbCELL context,char *format,...){
  va_list args;
  int len;
  va_start(args,format);
  len=vsnprintf(nb_OutLine,NB_BUFSIZE,format,args);
  if(len>=NB_BUFSIZE) strcpy(nb_OutLine+NB_BUFSIZE-4,"...");
  va_end(args);
  len=strlen(nb_OutLine);
  if((nb_OutCursor+len)>=(nb_OutBuffer+NB_BUFSIZE)) outFlush();
  strcpy(nb_OutCursor,nb_OutLine);
  nb_OutCursor+=len;
  return(0);
  }

/*
*  Dump a buffer
*/
int nbLogDump(nbCELL context,void *data,int len){
  unsigned char *cursor=(unsigned char *)data,*dataend=(unsigned char *)data+len,*wordend;
  unsigned char str[33],*strcur;
  int word;

  while(cursor<dataend){
    strcpy((char *)str,"................................");
    strcur=str;
    //nbLogPut(context,"%8.8x %4.4x ",cursor,cursor-(unsigned char *)data);
    nbLogPut(context,"%16.16x %4.4x ",cursor,cursor-(unsigned char *)data);
    for(word=0;word<8 && cursor<dataend;word++){
      wordend=cursor+4;
      while(cursor<wordend && cursor<dataend){
        nbLogPut(context,"%2.2x",*cursor);
        if((*cursor>='a' && *cursor<='z') ||
           (*cursor>='A' && *cursor<='Z') ||
           (*cursor>='0' && *cursor<='9') ||
           (*cursor!=0 && strchr("\"~`!@#$%^&*()_-+=[]\\{}|;':,./<>?",*cursor)!=NULL))
          *strcur=*cursor;
        cursor++;
        strcur++;
        }
      while(cursor<wordend){
        nbLogPut(context,"..");
        cursor++;
        }
      nbLogPut(context," ");
      }
    for(;word<8;word++) nbLogPut(context,"........ ");
    *strcur=0;
    nbLogPut(context,"%s\n",str);
    }
  nbLogPut(context,"\n");
  return(0);
  }


/*
*  Add an output handler
*/
int nbLogHandlerAdd(NB_Cell *context,void *session,void (*handler)(NB_Cell *context,void *session,char *buffer)){
  struct NB_OUTPUT_HANDLER *outputHandler;

  outFlush(); /* make sure it doesn't get buffered output */
  if((outputHandler=nb_OutputHandlerFree)==NULL) outputHandler=nbAlloc(sizeof(struct NB_OUTPUT_HANDLER));
  outputHandler->next=nb_OutputHandler;
  outputHandler->context=context;
  outputHandler->session=session;
  outputHandler->handler=handler;
  nb_OutputHandler=outputHandler;
  return(0);
  }

/*
*  Remove an output handler
*/
int nbLogHandlerRemove(NB_Cell *context,void *session, void (*handler)(NB_Cell *context,void *session,char *buffer)){
  struct NB_OUTPUT_HANDLER **outputHandlerP=&nb_OutputHandler,*outputHandler;

  outFlush(); /* make sure it has all output before removing */
  for(outputHandler=*outputHandlerP;
      outputHandler!=NULL && outputHandler->session!=session && outputHandler->handler!=handler;
      outputHandler=*outputHandlerP)
    outputHandlerP=&(outputHandler->next);
  if(outputHandler==NULL) return(-1);
  *outputHandlerP=outputHandler->next;
  outputHandler->next=nb_OutputHandlerFree;
  nb_OutputHandlerFree=outputHandler;
  return(0);
  }

/*
*  Flush output
*/
void nbLogFlush(NB_Cell *context){
  outFlush();
  }

//
// Display bar in log file
//
void nbLogBar(NB_Cell *context){
  outBar();
  }
