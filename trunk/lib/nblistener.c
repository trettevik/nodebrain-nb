/*
* Copyright (C) 1998-2010 The Boeing Company
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
* File:     nblistener.c 
*
* Title:    Listener API
*
* Function:
*
*   This file provides an API for managing file listeners.  This is intended
*   for use by node modules.  This API is just a layer on top of the medulla.
*
*   NOTE: This should probably move to the medulla now that we have cleaned
*   up the old listener code.
*
* Synopsis:
*
*   #include "nblistener.h"
*
*   void nbListenerInit();
*   int  nbListenerEnableOnDaemon(nbCELL context);
*   int  nbListenerAdd(NB_Cell *context,int fildes,void *session,void (*handler)(struct NB_CELL *,int fildes,void *session));
*   int  nbListenerReplace(NB_Cell *context,int fildes,void *session,void (*handler)(struct NB_CELL *,int fildes,void *session));
*   int  nbListenerRemove(NB_Cell *context,int fildes);
*   void nbListenerCloseAll();
*   int  nbListenerStart(nbCELL context);
*   int  nbListenerStop(nbCELL context);
* 
* Description
*
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-09-30 Ed Trettevik (original prototype version introduced in 0.4.1)
*
* 2003-03-08 eat 0.5.1  Recognize "type" as replacement for "protocol".
*            Still recognize "protocol" for old code.
*
* 2003-03-08 eat 0.5.1  Recognize "interface" and "command" as "address".
*            Different parameter names go with different listener types.
*
* 2003-03-08 eat 0.5.1  Implemented LOG listener to conform to documentation.
*
* 2003-03-08 eat 0.5.1  Included a symbolic context for a listener.
*            For now this is only used with translators.  It enables a
*            listener to maintain independent state information.
*
* 2003-05-22 eat 0.5.3  Included closeListeners() function for spawned processes
* 2003-10-13 eat 0.5.5  Set clientIdentity before alerting listener
* 2004-04-13 eat 0.6.0  nbListenerAdd/Remove/Start/Stop() added
* 2005-04-25 eat 0.6.2  nbListenerEnableOnDaemon() added
* 2005-05-27 eat 0.6.3  changed port assignment to (unsigned short) for cygwin
* 2005-12-18 eat 0.6.4  Started converting to "Medulla" interface for managing I/O
*            The strategy is to start using the medulla interface as a layer
*            below the "listener" interface.  This enables skill modules to use
*            the medulla interface directly without using the listener interface.
*            Then we will start converting various listeners to skill modules
*            and use the medulla interface directly.  This should enable the
*            eventual elimination of the listener interface.
*
*            As a first step, we will modify listener functions to call
*            medulla functions.
* 2008/03-09 eat 0.7.0  Removed old listener stuff
* 2008-11-11 eat 0.7.3  Changed failure exit code to NB_EXITCODE_FAIL
* 2010-01-02 eat 0.7.7  Included type to enable separate read and write listeners
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#include "nbi.h"
  
int nb_listener_serving=0;  // set when serving

//void nbListenerInit(NB_Stem *stem){
//#if defined(WIN32)
//  static struct WSAData myWSAData;
//
//  if(WSAStartup(MAKEWORD(1,1),&myWSAData)!=0){
//    printf("NB000E Unable to startup winsock.\n");
//    exit(NB_EXITCODE_FAIL);
//    }
//#endif
//  }

//***************************************************************************
// Medulla reader for listeners

int nbListenerReader(void *session){
  NB_Listener *sel=(NB_Listener *)session;

#if defined(WIN32)
  int mode=0;
  WSAResetEvent(sel->hEvent);
  WSAEventSelect((SOCKET)sel->fildes,sel->hEvent,0);
  ioctlsocket((SOCKET)sel->fildes,FIONBIO,&mode); // Make the socket blocking
#endif 
  (sel->handler)(sel->context,sel->fildes,sel->session);
#if defined(WIN32)
  WSAEventSelect((SOCKET)sel->fildes,sel->hEvent,FD_ACCEPT|FD_READ);
#endif
  return(0);
  }

/**************************************************************************** 
* Listener API - Introduced in 0.6.0
****************************************************************************/
NB_Listener *selectFree=NULL;
NB_Listener *selectUsed=NULL;
NB_Listener *selectPending=NULL;
   
int nbListenerEnableOnDaemon(nbCELL context){
  NB_Listener *sel;
  // DEFECT: When we are already serving (selectListener() has been called),
  //         a term will not be enabled automatically.  We can't just enable
  //         it here because this routine is normally called by a skill modules
  //         construct method before the term points to the node.  We need
  //         to make a change so nodes are automatically enabled if defined
  //         when we are already serving.  We should fix this when replacing
  //         this code with a similar feature in the Medulla.
  if(nb_listener_serving){
     outMsg(0,'W',"Node must be enabled via enable command");
     return(0);
     }
  if(NULL==(sel=selectFree)) sel=malloc(sizeof(NB_Listener));
  else selectFree=sel->next;
  memset(sel,0,sizeof(NB_Listener)); // clear unneeded fields
  sel->context=context;
  sel->next=selectPending;
  selectPending=sel;
  return(0);
  }

int nbListenerAdd(NB_Cell *context,int fildes,void *session,void (*handler)(struct NB_CELL *,int fildes,void *session)){
  NB_Listener *sel;
  
  if(NULL==(sel=selectFree)) sel=malloc(sizeof(NB_Listener));
  else selectFree=sel->next;
  sel->context=context;
  sel->type=0;
  sel->fildes=fildes;
  sel->session=session;
  sel->handler=handler;
  sel->next=selectUsed;
  selectUsed=sel;
#if defined(WIN32)
  // insert call to nbMedullaWaitEnable() here
  outMsg(0,'T',"calling nbMedullaWaitEnable in nbListenerAdd");
  sel->hEvent=WSACreateEvent();
  WSAEventSelect((SOCKET)fildes,sel->hEvent,FD_ACCEPT|FD_READ);
  nbMedullaWaitEnable(sel->hEvent,sel,nbListenerReader);
#else
  nbMedullaWaitEnable(0,fildes,sel,nbListenerReader); // 0.6.4
#endif
  return(0);
  }

int nbListenerAddWrite(NB_Cell *context,int fildes,void *session,void (*handler)(struct NB_CELL *context,int fildes,void *session)){
  NB_Listener *sel;

  if(NULL==(sel=selectFree)) sel=malloc(sizeof(NB_Listener));
  else selectFree=sel->next;
  sel->context=context;
  sel->type=1;
  sel->fildes=fildes;
  sel->session=session;
  sel->handler=handler;
  sel->next=selectUsed;
  selectUsed=sel;
#if defined(WIN32)
  // insert call to nbMedullaWaitEnable() here
  outMsg(0,'T',"calling nbMedullaWaitEnable in nbListenerAdd");
  sel->hEvent=WSACreateEvent();
  WSAEventSelect((SOCKET)fildes,sel->hEvent,FD_WRITE);
  nbMedullaWaitEnable(sel->hEvent,sel,nbListenerReader);
#else
  nbMedullaWaitEnable(1,fildes,sel,nbListenerReader); // 0.6.4
#endif
  return(0);
  }

int nbListenerReplace(NB_Cell *context,int fildes,void *session,void (*handler)(struct NB_CELL *context,int fildes,void *session)){
  NB_Listener *sel,**selP;

  for(selP=&selectUsed;*selP!=NULL && ((*selP)->type!=0 || (*selP)->fildes!=fildes);selP=&(*selP)->next);
  if(*selP==NULL || (*selP)->fildes!=fildes) return(1); 
  sel=*selP;
  sel->session=session;
  sel->handler=handler;
  return(0);
  }

int nbListenerReplaceWrite(NB_Cell *context,int fildes,void *session,void (*handler)(struct NB_CELL *context,int fildes,void *session)){
  NB_Listener *sel,**selP;

  for(selP=&selectUsed;*selP!=NULL && ((*selP)->type!=1 || (*selP)->fildes!=fildes);selP=&(*selP)->next);
  if(*selP==NULL || (*selP)->fildes!=fildes) return(1);
  sel=*selP;
  sel->session=session;
  sel->handler=handler;
  return(0);
  }

int nbListenerRemove(NB_Cell *context,int fildes){
  NB_Listener *sel,**selP;

  for(selP=&selectUsed;*selP!=NULL && ((*selP)->type!=0 || (*selP)->fildes!=fildes);selP=&(*selP)->next);
  if(*selP==NULL || (*selP)->fildes!=fildes) return(1); 
  sel=*selP;
  *selP=sel->next;
  sel->next=selectFree;
  selectFree=sel;
#if defined(WIN32)
  nbMedullaWaitDisable(sel->hEvent);
  WSACloseEvent(sel->hEvent);
#else
  nbMedullaWaitDisable(0,fildes);  // 0.6.4
#endif
  return(0);
  }

int nbListenerRemoveWrite(NB_Cell *context,int fildes){
  NB_Listener *sel,**selP;

  for(selP=&selectUsed;*selP!=NULL && ((*selP)->type!=1 || (*selP)->fildes!=fildes);selP=&(*selP)->next);
  if(*selP==NULL || (*selP)->fildes!=fildes) return(1);
  sel=*selP;
  *selP=sel->next;
  sel->next=selectFree;
  selectFree=sel;
#if defined(WIN32)
  nbMedullaWaitDisable(sel->hEvent);
  WSACloseEvent(sel->hEvent);
#else
  nbMedullaWaitDisable(1,fildes);  // 0.6.4
#endif
  return(0);
  }


void nbListenerCloseAll(){
  NB_Listener *sel;
  for(sel=selectUsed;sel!=NULL;sel=sel->next){
    if(sel->fildes)
#if defined(WIN32)
      closesocket(sel->fildes);
#else
      close(sel->fildes);
#endif
    }
  }


/*
*  This is the main loop when NodeBrain is operating in server mode.
*/
#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbListenerStart(nbCELL context){
  NB_Listener *sel,*selnext=NULL;
#if !defined(WIN32)
  struct passwd *pwd=NULL;
  struct group  *grp=NULL;
#endif
 
  if(trace) outMsg(0,'T',"selectListener() called");
  // enable listeners in nb_Disabled state
  for(sel=selectPending;sel!=NULL;sel=selnext){
    selnext=sel->next;      // Get pointer to next pending entry
    sel->next=selectFree;   // move this entry to the free list
    selectFree=sel;         // so it can be reused by the enable method
    context=sel->context;
    if(context->object.value==nb_Disabled){
      context->object.type->enable(context);
      }
    }
  nb_listener_serving=1;  // flag serving mode

#if(!defined(WIN32))
  // change working directory if requested
  if(*servedir!=0){
    if(chdir(servedir)<0){
      outMsg(0,'E',"Unable to change working directory to %s - errno=%d",servedir,errno);
      exit(NB_EXITCODE_FAIL);
      }
    outMsg(0,'I',"Working directory changed to %s",servedir);
    }
  // If running as root, check for and process special settings
  if(getuid()==0){
    // get user id if user parameter specified
    if(*serveuser!=0){
      if((pwd=getpwnam(serveuser))==NULL){
        outMsg(0,'E',"User %s not defined",serveuser);
        exit(NB_EXITCODE_FAIL);
        }
      if((grp=getgrgid(pwd->pw_gid))==NULL){
        outMsg(0,'E',"User %s has undefined group id %d",serveuser,pwd->pw_gid);
        exit(NB_EXITCODE_FAIL);
        }
      }
    // get group id if group parameter specified
    if(*servegroup && (grp=getgrnam(servegroup))==NULL){
      outMsg(0,'E',"User %s has undefined group id %d",serveuser,pwd->pw_gid);
      exit(NB_EXITCODE_FAIL);
      }
    // change root directory (jail) if requested
    if(*servejail!=0){
      if(chroot(servejail)<0){
        outMsg(0,'E',"Unable to change root directory to %s - errno=%d",servejail,errno);
        exit(NB_EXITCODE_FAIL);
        }
      outMsg(0,'I',"Root directory changed to %s",servejail);
      }
    // switch group if requested
    if(grp){
      if(setgid(grp->gr_gid)<0){
        outMsg(0,'E',"Unable to set group to %s - %s",grp->gr_name,strerror(errno));
        exit(NB_EXITCODE_FAIL);
        }
      outMsg(0,'I',"Set group to %s",grp->gr_name);
      }
    // switch user if requested
    if(pwd){ 
      if(setuid(pwd->pw_uid)<0){
        outMsg(0,'E',"Unable to set user to %s - %s",serveuser,strerror(errno));
        exit(NB_EXITCODE_FAIL);
        }
      outMsg(0,'I',"Set user to %s",serveuser);
      }
    }
#endif
  outFlush();

  nbMedullaPulse(1);      // start server
  return(0);
  }

#if defined(WIN32)
_declspec (dllexport)
#endif
int nbListenerStop(nbCELL context){
  nbMedullaStop();
  return(0);
  }
