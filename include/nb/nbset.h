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
* File:     nbset.h
*
* Title:    Set Binary Tree Routines
*
* Purpose:
*
*   This header defines structures and functions of nbset.c.
*
*   See nbset.c for more information.
*
*============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2013-12-30 Ed Trettevik - Introduced in 0.9.00
*=============================================================================
*/
#ifndef _NB_SET_H_
#define _NB_SET_H_

typedef struct NB_SET_NODE{    // Balanced binary tree node - object header 
  struct NB_SET_NODE *left;    // left node  (lower members for binary search tree)
  struct NB_SET_NODE *right;   // right node (higher members for binary search tree)
  signed char balance;         // AVL balance code (-1 left tall, 0 - balanced, +1 right tall)
  unsigned char reserved[7];  
  struct NB_SET_NODE *parent;  // parent node
  } NB_SetNode;

typedef struct{                // structure used to point to set elements
  NB_SetNode setnode;
  void       *member;
  } NB_SetMember;

#define NB_SET_FIND_MEMBER(MEMBER,NODE) \
  while(NODE!=NULL){ \
    if(MEMBER<NODE->member) NODE=(NB_SetMember *)NODE->setnode.left; \
    else if(MEMBER>NODE->member) NODE=(NB_SetMember *)NODE->setnode.right; \
    else break; \
    }

#define NB_SET_LOCATE_MEMBER(MEMBER,NODE,PARENT,NODEP) \
  PARENT=(NB_SetMember *)NODEP; \
  for(NODE=*NODEP;NODE!=NULL;NODE=*NODEP){ \
    PARENT=NODE; \
    if(MEMBER<NODE->member) NODEP=(NB_SetMember **)&NODE->setnode.left; \
    else if(MEMBER>NODE->member) NODEP=(NB_SetMember **)&NODE->setnode.right; \
    else break; \
    }


typedef struct NB_SET_ITERATOR{
  NB_SetNode *right[32];
  NB_SetNode **rightP;
  } NB_SetIterator;

#define NB_SET_ITERATE(ITERATOR,NODE,ROOT) \
  NODE=(NB_SetNode *)ROOT; \
  ITERATOR.rightP=&ITERATOR.right[0]; \
  if(NODE!=NULL) while(1)

#define NB_SET_ITERATE_NEXT(ITERATOR,NODE) \
  if(NODE->left!=NULL){ \
    if(NODE->right!=NULL){ \
      *ITERATOR.rightP=NODE->right; \
      ITERATOR.rightP++; \
      } \
    NODE=NODE->left; \
    } \
  else{ \
    NODE=NODE->right; \
    if(NODE==NULL){ \
      ITERATOR.rightP--; \
      if(ITERATOR.rightP<&ITERATOR.right[0]) break; \
      NODE=*ITERATOR.rightP; \
      } \
    }

// Tree iteration macros where NEXT is not last thing in loop
#define NB_SET_ITERATE2(ITERATOR,NODE,ROOT) \
  NODE=(NB_SetNode *)ROOT; \
  ITERATOR.rightP=&ITERATOR.right[0]; \
  while(NODE!=NULL)

#define NB_SET_ITERATE_NEXT2(ITERATOR,NODE) \
  if(NODE->left!=NULL){ \
    if(NODE->right!=NULL){ \
      *ITERATOR.rightP=NODE->right; \
      ITERATOR.rightP++; \
      } \
    NODE=NODE->left; \
    } \
  else{ \
    NODE=NODE->right; \
    if(NODE==NULL){ \
      ITERATOR.rightP--; \
      if(ITERATOR.rightP>=&ITERATOR.right[0]) NODE=*ITERATOR.rightP; \
      } \
    }

//=============================================================================

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbSetFind(void *member,NB_SetMember *root);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern NB_SetNode *nbSetFlatten(NB_SetNode **nodeP,NB_SetNode *node);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern NB_SetNode *nbSetBalance(NB_SetNode *node,int n,NB_SetNode **nextP);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbSetInsert(NB_SetNode *root,NB_SetNode *parent,NB_SetNode **nodeP,NB_SetNode *node);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbSetRemove(NB_SetNode *root,NB_SetNode *node);

#endif
