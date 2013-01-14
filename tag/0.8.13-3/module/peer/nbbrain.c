/*
* Copyright (C) 1998-2012 The Boeing Company
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
* File:     nbbrain.c 
*
* Title:    Brain Methods
*
* Function:
*
*   This file provides methods for NodeBrain brain objects.  This is not a
*   true NodeBrain object yet.
*
* Synopsis:
*
*   #include "nb.h"
*
* 
* Description
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-10-06 Ed Trettevik (split out from nb.c in 0.4.1)
* 2002-10-21 eat - version 0.4.1 A8
*            1) Included skullTarget brain name.  This is passed to a peer's
*               skull when we are copying files to a queue.  The target is
*               optional in the syntax below.  For compatibility with existing
*               nb files, [] is still supported as an alternative to ().
*
*               declare <term> brain id@host:port(queue,target);
*
* 2002-12-09 eat - version 0.4.3 B3
*            1) Added queue management parameters (qsec,qfsize,qsize)
*
*               declare <term> brain <id>@<host>:<port><queue_definition>
*
*               <queue_definition> :=  (queue(qsec,qfsize,qsize),target)
*
* 2003-10-06 eat 0.5.5  Included support for holding open sessions
*            declare <term> brain <id>@<host>:<port>{holdtime}<queue_def>;
*            This is experimental and will be finished later.
*
* 2006-05-25 eat 0.6.6  Included support for local domain socket paths
*            declare <term> brain <id>@"<socket_path>"{holdtime}<queue_def>;
*            This is experimental.
*
* 2007-07-01 eat 0.6.8  Modified to support local domain socket without quotes
*            declare <term> brain <id>@socket_path{holdtime}<queue_def>;
*            This support brain specifications as parameters to NBP nodes
*            which are replacing brain declarations.
*
* 2007-03-07 eat 0.6.9  Included support for portrayed identity override
*            declare <term> brain [<myid>~]<id>@...
*            When myid is specified, it overrides the identity set with PORTRAY command
* 2009-07-01 eat 0.7.6  Using ip address as hostname when gethostbyaddr() doesn't resolve it
*            This fixes a defect where hostname and ip address were defaulting
*            to localhost and 127.0.0.1 when hostname was not resolved.
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-10-17 eat 0.8.12 Checker updates
*=============================================================================
*/
#include <nb/nbi.h>
#include "nbvli.h"
#include "nbprotokey.h"
#include "nbbrain.h"
#include "nbchannel.h"

//char *chgetaddr();     /* in nbchannel.c */
//char *chgetname();     /* in nbchannel.c */

NB_Term   *brainC;         /* brain context */
//struct HASH   *brainH;         /* brain hash */

struct TYPE *nb_BrainType; /* object type pointer */

struct BRAIN *nb_BrainFree;

/*
*  Brain object routines 
*/

void printBrain(struct BRAIN *brain){
  if(brain->myId!=NULL && brain->myId!=brain->id) nbLogPutI("%s~",brain->myId);
  if(brain->id!=NULL) nbLogPutI("%s@",brain->id);
  if(brain->ipaddr!=NULL){
    if(brain->hostname!=NULL)
      nbLogPutI("%s:%s:%d{%d}",brain->hostname,brain->ipaddr,brain->port,brain->dsec); 
    else nbLogPutI("%s:%d{%d}",brain->ipaddr,brain->port,brain->dsec);
    }
  if(brain->dir!=NULL){
    nbLogPutI("(%s",brain->dir->value);
    if(brain->qsec!=0 || brain->qfsize!=0 || brain->qsize!=0){
      char qparms[64];
      snprintf(qparms,sizeof(qparms),"(%d,%d,%d)",brain->qsec,(int)brain->qfsize,(int)brain->qsize); // 2013-01-13 eat - VID 5867
      nbLogPutI(qparms);
      }
    if(brain->skullTarget!=NULL) nbLogPutI(",%s",brain->skullTarget->value);
    nbLogPutI(")");
    } 
  }

void destroyBrain(struct BRAIN *brain){
  // convert id,hostname, and ipaddr to STRING objects 
  // and insert dropObject() calls here 
  //dropObject(brain->dir);
  //brain->object.next=(NB_Object *)nb_BrainFree;
  //nb_BrainFree=brain;
  nbFree(brain,sizeof(struct BRAIN));
  }
    
// Create new brain object
//
// 2007/07/01 eat - modified to support unix sockets without quotes
//            This is helpful so we can temporarily call this function
//            from the NBP skill module.  Since peer specification are
//            within quotes, we had to remove the quirement for quotes.
//            Quotes are still supported for backward compatibility until
//            we drop support for DECLARE BRAIN completely.
//
//   <spec>   ::= <identity>@[<socket>[{pTime}]][(qDir[(qTime)])]
//   <socket> ::= <host>:<port> | <socketFilename>
//   <host>   ::= <hostname> | <hostIpAddress>
//   <pTime>  ::= persistence time in seconds (holds connections)
//   <qDir>   ::= queue directory - may be enclosed in [] for backward compatibility
//   <qTime>  ::= queue time interval in seconds to write to same file

struct BRAIN * nbBrainNew(int version,char *string){
  size_t len;
  char *cursor=string,*cursave=string,*delim;
  char symid,ident[256],*ipaddr,*hostname; 
  struct BRAIN *brain;
  int number;

  brain=(struct BRAIN *)nbAlloc(sizeof(struct BRAIN));
  brain->version=version;
  brain->context=NULL;
  brain->myId=NULL;
  brain->myIdentity=NULL;
  brain->id=NULL;
  brain->identity=NULL;
  brain->hostname=NULL;
  brain->ipaddr=NULL;
  brain->port=0;
  brain->spec=0;
  brain->dir=NULL;
  brain->qsec=86400;  /* 1 day */
  brain->qfsize=0;
  brain->qsize=0;
  brain->skullTarget=NULL;
  brain->session=NULL;
  brain->dsec=0;
  brain->rsec=0;

  symid=nbParseSymbol(ident,&cursor);
  if(symid==';') return(brain);
  if(symid=='t'){
    if(*cursor=='~'){
      brain->myId=((struct STRING *)nbCellCreateString(NULL,ident))->value;
      cursor++;
      symid=nbParseSymbol(ident,&cursor);
      }
    if(*cursor!='@'){
      nbLogMsgI(0,'E',"Expecting '@' symbol at: %s",cursor);
      destroyBrain(brain);
      return(NULL);
      }
    brain->id=((struct STRING *)nbCellCreateString(NULL,ident))->value;
    cursor++;
    cursave=cursor;
    if(*cursor=='"'){ //local (unix) domain socket path
      cursor++;
      delim=strchr(cursor,'"');
      if(delim==NULL){
        nbLogMsgI(0,'E',"Unbalanced quotes in local domain socket path");
        destroyBrain(brain);
        return(NULL);
        }
      *(delim)=0;
      brain->hostname=((struct STRING *)nbCellCreateString(NULL,cursor))->value;
      *(delim)='"';
      cursor=delim+1;
      // port=0 is our clue that it is a local domain socket path
      brain->ipaddr="";
      }
    else{
      if(*cursor!='.' && (len=strspn(cursor,"0123456789."))>0){
        cursor+=len;
        if(len>15){
          nbLogMsgI(0,'E',"Internet address \"%s\" may not exceed 15 characters.",cursave);
          destroyBrain(brain);
          return(NULL);
          }
        symid=*cursor;
        *cursor=0;
        brain->ipaddr=((struct STRING *)nbCellCreateString(NULL,cursave))->value;
        *cursor=symid;
        if((hostname=chgetname(brain->ipaddr))!=NULL)
          brain->hostname=((struct STRING *)nbCellCreateString(NULL,hostname))->value;
        else brain->hostname=grabObject(brain->ipaddr);
        if(*cursor!=':'){
          nbLogMsgI(0,'E',"Expecting ':' at: %s",cursor);
          destroyBrain(brain);
          return(NULL);
          }
        }  
      else{
        for(delim=cursor;*delim!=0 && *delim!=':' && *delim!='{' && *delim!='(' && *delim!=' ';delim++);
        len=delim-cursor;
        if(len>255){
          nbLogMsgI(0,'E',"Hostname longer than 255 limit before ':' encountered.");
          destroyBrain(brain);
          return(NULL);
          }
        strncpy(ident,cursor,len);
        ident[len]=0;
        cursor=delim;
        brain->hostname=((struct STRING *)nbCellCreateString(NULL,ident))->value;
        if(*cursor==':'){
          if((ipaddr=chgetaddr(ident))==NULL){
            nbLogMsgI(0,'E',"Unknown host name \"%s\".",ident);
            destroyBrain(brain);
            return(NULL);
            }
          brain->ipaddr=((struct STRING *)nbCellCreateString(NULL,ipaddr))->value;
          }
        else brain->ipaddr="";
        }
      if(*cursor==':'){
        if(brain->hostname==NULL){
          brain->hostname=((struct STRING *)nbCellCreateString(NULL,"localhost"))->value;
          brain->ipaddr=((struct STRING *)nbCellCreateString(NULL,"127.0.0.1"))->value;
          }
        cursor++;
        cursave=cursor;
        symid=nbParseSymbol(ident,&cursor);
        if(symid!='i'){
          nbLogMsgI(0,'E',"Expecting integer port number at: %s",cursave);
          destroyBrain(brain);
          return(NULL);
          }
        cursave=cursor;
        number=atoi(ident);
        if(number<0 || number>=65535){
          nbLogMsgI(0,'E',"Expecting integer port number from 0 to 65535  at: %s",cursave);
          destroyBrain(brain);
          return(NULL);
          }
        brain->port=number;
        }
      else brain->port=0;
      }
    symid=nbParseSymbol(ident,&cursor);
    if(symid=='{'){
      cursave=cursor;
      symid=nbParseSymbol(ident,&cursor);
      if(symid!='i'){
        nbLogMsgI(0,'E',"Expecting persistent connection options at \"%s\"",cursave);
        destroyBrain(brain);
        return(NULL);
        }
      cursave=cursor;
      brain->dsec=atoi(ident);
      if(brain->dsec<0){
        nbLogMsgI(0,'E',"Expecting non-negative persistent connection seconds at: %s",cursave);
        destroyBrain(brain);
        return(NULL);
        }
      cursave=cursor;
      symid=nbParseSymbol(ident,&cursor);
      if(symid==','){
        cursor++;
        cursave=cursor;
        symid=nbParseSymbol(ident,&cursor);
        if(symid!='i'){
          nbLogMsgI(0,'E',"Expecting reconnect delay seconds at \"%s\"",cursave);
          destroyBrain(brain);
          return(NULL);
          }
        cursave=cursor;
        brain->rsec=atoi(ident);
        if(brain->rsec<0){
          nbLogMsgI(0,'E',"Expecting non-negative reconnect delay seconds at: %s",cursave);
          destroyBrain(brain);
          return(NULL);
          }
        cursave=cursor;
        symid=nbParseSymbol(ident,&cursor);
        }
      if(symid!='}'){
        nbLogMsgI(0,'E',"Expecting \"}\" at \"%s\"",cursave);
        destroyBrain(brain);
        return(NULL);
        }
      cursor++;
      symid=nbParseSymbol(ident,&cursor);
      }
    }
  if(symid=='(' || symid=='['){
    char stop=')';
    if(symid=='[') stop=']';  /* deprecated syntax */
    for(delim=cursor;*delim!=stop && *delim!=',' && *delim!='(' && *delim!=0;delim++);
    if((len=delim-cursor)>255){
      nbLogMsgI(0,'E',"File name length restriction of 255 reached before delimiter");
      destroyBrain(brain);
      return(NULL);
      }
    strncpy(ident,cursor,len);
    ident[len]=0;
    brain->dir=(struct STRING *)nbCellCreateString(NULL,ident);
    if(*delim=='('){
      cursor=delim+1;
      cursave=cursor;
      symid=nbParseSymbol(ident,&cursor);
      if(symid!='i'){
        nbLogMsgI(0,'E',"Expecting queue seconds at \"%s\"",cursave);
        destroyBrain(brain);
        return(NULL);
        }
      cursave=cursor;
      brain->qsec=atoi(ident);
      if(brain->qsec<0){
        nbLogMsgI(0,'E',"Expecting non-negative interval at \"%s\"",cursave);
        destroyBrain(brain);
        return(NULL);
        }
      if(brain->qsec==0) brain->qsec=86400; /* default to 1 day */
      while(*cursor==' ') cursor++;
      if(*cursor==','){
        cursor++;
        cursave=cursor;
        symid=nbParseSymbol(ident,&cursor);
        if(symid!='i'){
          nbLogMsgI(0,'E',"Expecting queue file size limit at \"%s\"",cursave);
          destroyBrain(brain);
          return(NULL);
          }
        cursave=cursor;
        number=atoi(ident);
        if(number<0 || number>65535){
          nbLogMsgI(0,'E',"File size limit out of bounds 0-65535 at \"%s\"",cursave);
          destroyBrain(brain);
          return(NULL);
          }
        brain->qfsize=number;
        while(*cursor==' ') cursor++;
        if(*cursor==','){
          cursor++;
          cursave=cursor;
          symid=nbParseSymbol(ident,&cursor);
          if(symid!='i'){
            nbLogMsgI(0,'E',"Expecting queue size limit at \"%s\"",cursave);
            destroyBrain(brain);
            return(NULL);
            }
          cursave=cursor;
          number=atoi(ident);
          if(number<0 || number>65535){
            nbLogMsgI(0,'E',"Queue size limit out of bounds 0-65535 at \"%s\"",cursave);
            destroyBrain(brain);
            return(NULL);
            }
          brain->qsize=number;
          }
        }
      if(*cursor!=')'){
        nbLogMsgI(0,'E',"Expecting right parenthesis at \"%s\"",cursave);
        destroyBrain(brain);
        return(NULL);
        }
      delim=cursor+1;
      while(*delim==' ') delim++;
      }
    if(*delim==','){
      cursor=delim+1;
      for(delim=cursor;*delim!=stop && *delim!=0;delim++);
      if((len=delim-cursor)>64){
        nbLogMsgI(0,'E',"Skull target brain name length restriction of 64 reached before expected ']'");
        destroyBrain(brain);
        return(NULL);
	}
      strncpy(ident,cursor,len);
      ident[len]=0;
      brain->skullTarget=(struct STRING *)nbCellCreateString(NULL,ident);
      }
    cursor=delim+1;
    }
  while(*cursor==' ') cursor++;
  if(*cursor!=0 && *cursor!=';' && *cursor!='\n'){
    nbLogMsgI(0,'E',"Peer specification not recognized at: %s",cursor);
    destroyBrain(brain);
    return(NULL);
    }
  return(brain);  
  }
  
NB_Term *getBrainTerm(char *ident){
  return((NB_Term *)nbTermLocate((nbCELL)brainC,ident));
  }

// This is a transitional function to enable the nbp and queue modules to
// declare brains internally.

static int brainCount=0;
NB_Term *nbBrainMakeTerm(struct TERM *context,struct BRAIN *brain){
  char ident[20];
  sprintf(ident,"b%4.4d",brainCount);
  brainCount++;
  brain->context=context; // link brain to "alias" - peer node term
  return((NB_Term *)nbTermCreate((nbCELL)brainC,ident,(nbCELL)brain));
  }
