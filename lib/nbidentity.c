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
* File:     nbidentity.c 
*
* Title:    Identity Object Management Routines (prototype)
*
* Function:
*
*   This file provides routines that manage nodebrain identity objects.
*  
* Synopsis:
*
*   #include "nbidentity.h"
*
*   void initIdentity();
*
*   struct IDENTITY *nbIdentityNew(char *key);
*   struct IDENTITY *getIdentity(char *ident);
*
* Description
*
*   You can construct an identity using the nbIdentityNew() method.
*
*     struct IDENTITY *myid=nbIdentityNew(key);
*
*   The caller is responsible for incrementing the use count of an identity
*   when references are assigned.  A call to grabObject() for the identity
*   might look like this.
*
*     mypointer=grabObject(myid);
*
*   A call to dropObject is required when the pointer is changed.
*
*     dropObject(struct IDENTITY *myid);
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
* 2001-07-21 Ed Trettevik (original prototype version 0.2.8)
*             1) This code has been pulled from nodebrain.c.
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

NB_Term   *identityC;      /* public identity context */
//struct HASH   *identityH;      /* identity hash */
struct TYPE   *identityType;   /* identity object type */

struct IDENTITY *nb_IdentityFree;

struct IDENTITY *defaultIdentity;   /* default identity */

//#if defined(WIN32)
//__declspec(dllexport)
//#endif
struct IDENTITY *clientIdentity;    /* client identity */

/*******************************************************************************
*  Identity Methods
*******************************************************************************/
struct IDENTITY * nbIdentityNew(char *name,unsigned char authority){
  struct IDENTITY *identity;
  identity=(struct IDENTITY *)newObject(identityType,(void **)&nb_IdentityFree,sizeof(struct IDENTITY));
  identity->name=grabObject(useString(name));
  identity->authority=authority;  /* set authority */
  return(identity);
  } 
   
struct IDENTITY *getIdentity(char *ident){
  NB_Term *term;
  if((term=nbTermFind(identityC,ident))==NULL) return(NULL);
  return((struct IDENTITY *)(term->def));
  }

/*
*  Print an identity
*/
void printIdentity(identity) struct IDENTITY *identity;{
  outPut(" %s ",identity->object.type->name);
  if(identity->authority==AUTH_OWNER) outPut(" owner");
  else if(identity->authority==AUTH_USER) outPut(" user");
  else if(identity->authority==AUTH_PEER) outPut(" peer");
  else if(identity->authority==AUTH_GUEST) outPut(" guest");
  else {
    if(identity->authority&(AUTH_CONNECT)) outPut(" connect");
    if(identity->authority&AUTH_ASSERT)  outPut(" assert");
    if(identity->authority&AUTH_DEFINE)  outPut(" define");
    if(identity->authority&AUTH_SYSTEM)  outPut(" system");
    }
  }

/*
*  Destroy an identity
*/
void destroyIdentity(identity) struct IDENTITY *identity;{
  identity->object.next=(NB_Object *)nb_IdentityFree;
  nb_IdentityFree=identity;
  }

/*
*  Context object type initialization
*/
void initIdentity(NB_Stem *stem){
  nb_IdentityFree=NULL;
  identityType=nbObjectType(stem,"identity",0,0,printIdentity,destroyIdentity);
  identityC=nbTermNew(NULL,"identity",nbNodeNew());
  }

//******************
// External API

struct IDENTITY *nbIdentityGet(NB_Cell *context,char *ident){
  return(getIdentity(ident));
  }

char *nbIdentityGetName(NB_Cell *context,nbIDENTITY identity){
  return(identity->name->value);
  }

nbIDENTITY nbIdentityGetActive(nbCELL context){
  return(clientIdentity);
  }

nbIDENTITY nbIdentitySetActive(nbCELL context,nbIDENTITY identity){
  nbIDENTITY currentIdentity=clientIdentity;
  clientIdentity=identity;
  return(currentIdentity);
  }

