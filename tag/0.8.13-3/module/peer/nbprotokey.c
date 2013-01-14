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
* File:     nbprotokey.c 
*
* Title:    Peer Key Management Routines (prototype)
*
* Function:
*
*   This file provides routines that manage nodebrain peer key entries 
*  
* Synopsis:
*
*   #include "nbprotokey.h"
*
*   void nbpLoadKeys();
*
*   NB_PeerKey *nbpNewPeerKey(char *name,char *key);
*   NB_PeerKey *nbpGetPeerKey(char *ident);
*
* Description
*
*   You can construct an identity using the newIdentity() method.
*
*     NB_PeerKey *myid=nbpNewPeerKey(name,key);
*
*   The caller is responsible for incrementing the use count of an identity
*   when references are assigned.  A call to nbCellCreateString for the identity
*   might look like this.
*
*     mypointer=nbCellCreateString(NULL,myid);
*
*   A call to nbCellDrop is required when the pointer is changed.
*
*     nbCellDrop(NULL,NB_PeerKey *myid);
*
*    
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2001/07/21 Ed Trettevik (original prototype version 0.2.8)
*             1) This code has been pulled from nodebrain.c.
* 2008/10/31 eat 0.7.3  Renamed key file to nb_peer.key
* 2012-01-12 dtl        Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
*=============================================================================
*/
#include <nb/nbi.h>
#include "nbprotocol.h"

NB_PeerKey *nb_PeerKeyTree=NULL;  // defined keys
NB_PeerKey *nb_PeerKeyFree=NULL;  // free key structures 
NB_PeerKey *nb_DefaultPeerKey;    // default key

/*******************************************************************************
*  Identity Methods
*******************************************************************************/
int nbpSetIdentity(NB_PeerKey *peerKey,char *key){
  /* Update a peer key structure */
  /* e.n.d.o    e.g. 7.d3c8f3f8.f8867ca7.0  */
  /* e - exponent */
  /* n - modulus */
  /* d - private exponent */
  /* o - owner identifier */
  /* return code:  0 - success, 1 - failed */
  char keybuf[1024];
  int len,part=0;
  char *value,*cursor; 

  if(snprintf(keybuf,sizeof(keybuf),"%s",key)>=sizeof(keybuf)){ //2012-01-31 dtl: replaced strcpy
    // 2012-02-09 eat - included error response when key too large for buffer
    nbLogMsgI(0,'E',"Invalid peer key \"%s\". Length greater than max of %d",key,sizeof(keybuf)-1);
    return(1);
    }
  len=strspn(keybuf,"0123456789abcdef");
  value=keybuf;
  cursor=value+len;
  if(len<1 || *cursor!='.') part=1;
  else{
    *cursor=0;
    vligetx(peerKey->exponent,(unsigned char *)value);
    *cursor='.';
    value=cursor+1;
    len=strspn(value,"0123456789abcdef");
    cursor=value+len;
    if(len<1 || *cursor!='.') part=2;
    else{
      *cursor=0;
      vligetx(peerKey->modulus,(unsigned char *)value);
      *cursor='.';
      value=cursor+1;
      len=strspn(value,"0123456789abcdef");
      cursor=value+len;
      if(len<1 || *cursor!='.') part=3;  
      else{
        *cursor=0;
        vligetx(peerKey->private,(unsigned char *)value);
        *cursor='.';
        value=cursor+1;
        len=strspn(value,"0123456789");
        cursor=value+len;
        if(len<1 || (*cursor!=0 && *cursor!=';' && *cursor!=' ' && *cursor!='\n')) part=4;
        else{
          *cursor=0;
          peerKey->user=atol(value);
          }
        }
      }
    }
  if(part>0){
    nbLogMsgI(0,'E',"Invalid peer key \"%s\". Part %d not recognized.",key,part);
    return(1);
    }
  return(0);  
  }
  
NB_PeerKey *nbpNewPeerKey(char *name,char *key){
  NB_PeerKey *peerKey;
  NB_TreePath path; 
  nbCELL treeKey;

  treeKey=nbCellCreateString(NULL,name);
  if(nbTreeLocate(&path,treeKey,(NB_TreeNode **)&nb_PeerKeyTree)!=NULL) return(NULL);
  if((peerKey=nb_PeerKeyFree)==NULL) peerKey=nbAlloc(sizeof(NB_PeerKey));
  else nb_PeerKeyFree=(NB_PeerKey *)peerKey->node.left;
  peerKey->identity=nbIdentityGet(NULL,name);
  peerKey->authority=0;
  if(peerKey->identity) peerKey->authority=peerKey->identity->authority;  /* set authority */
  if(nbpSetIdentity(peerKey,key)){
    peerKey->node.left=(NB_TreeNode *)nb_PeerKeyFree;
    nb_PeerKeyFree=peerKey;
    return(NULL);   /* syntax error */
    }
  nbTreeInsert(&path,(NB_TreeNode *)peerKey);
  return(peerKey);
  } 
   
NB_PeerKey *nbpGetPeerKey(char *name){
  NB_PeerKey *peerKey;
//  NB_TreePath path;
  nbCELL treeKey;
  if(nb_PeerKeyTree==NULL) nbpLoadKeys();
  treeKey=nbCellCreateString(NULL,name);
  peerKey=nbTreeFind(treeKey,(NB_TreeNode *)nb_PeerKeyTree);
  nbCellDrop(NULL,treeKey);
  return(peerKey);
  }

/*
*  Print a peer key
*/
void nbpPrintPeerKey(NB_PeerKey *peerKey){
  nbLogPutI(" %s ",((struct STRING *)peerKey->node.key)->value);
  if(peerKey->authority==AUTH_OWNER) nbLogPutI(" owner");
  else if(peerKey->authority==AUTH_USER) nbLogPutI(" user");
  else if(peerKey->authority==AUTH_PEER) nbLogPutI(" peer");
  else if(peerKey->authority==AUTH_GUEST) nbLogPutI(" guest");
  else {
    if(peerKey->authority&(AUTH_CONNECT)) nbLogPutI(" connect");
    if(peerKey->authority&AUTH_ASSERT)  nbLogPutI(" assert");
    if(peerKey->authority&AUTH_DEFINE)  nbLogPutI(" define");
    if(peerKey->authority&AUTH_SYSTEM)  nbLogPutI(" system");
    }
  }

/*
*  Destroy a peer key
*/
void nbpDestroyPeerKey(NB_PeerKey *peerKey){
  peerKey->node.left=(NB_TreeNode *)nb_PeerKeyFree;
  peerKey->node.key=nbCellDrop(NULL,(nbCELL)peerKey->node.key);
  nb_PeerKeyFree=peerKey;
  }

/*
*  Context object type initialization
*/
void nbpLoadKeys(){
  char bufin[NB_BUFSIZE];
  FILE *file;
  char *name=bufin,*key;
  char filename[1024];
  char *userDir;
  char *basename="/nb_peer.keys";

  nb_DefaultPeerKey=nbpNewPeerKey("default","7.d3c8f3f8.f8867ca7.0");  /* default */
  userDir=nbGetUserDir();
  if(strlen(userDir)>=sizeof(filename)+sizeof(basename)){
    outMsg(0,'E',"User home directory path too long.");
    return;
    }
  sprintf(filename,"%s%s",userDir,basename);
  //strcpy(filename,nbGetUserDir());
  //strcat(filename,"/nb_peer.keys");
  if((file=fopen(filename,"r"))==NULL) return;
  while(fgets(bufin,NB_BUFSIZE,file)!=NULL){
    key=bufin;
    while(*key && *key!=' ') key++;
    *key=0;
    key++;
    while(*key && *key==' ') key++;
    nbpNewPeerKey(name,key);
    }
  fclose(file);
  nbLogMsgI(0,'I',"Peer keys loaded.");
  }
