/*
* Copyright (C) 2006-2014 The Boeing Company
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbtree.h
*
* Title:    Binary Tree Routines
*
* Purpose:
*
*   This header defines structures and functions of nbtree.c.
*
*   See nbtree.c for more information.
*
*============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2006/09/02 Ed Trettevik - first prototype
* 2007/05/22 eat 0.6.8 - split out header and made part of API
* 2008/02/08 eat 0.6.9 - included iterator macros
* 2011-07-09 eat 0.8.6 - included NB_TREE_ITERATE_ORDER_STRING_CASE_AFTER macro
* 2013-12-20 eat 0.9.00 Changed balance from int to char
*=============================================================================
*/
#ifndef _NB_TREE_H_
#define _NB_TREE_H_

typedef struct NB_TREE_NODE{   // Balanced binary tree node 
  struct NB_TREE_NODE *left;   // left node  (lower keys for binary search tree)
  struct NB_TREE_NODE *right;  // right node (higher keys for binary search tree)
  signed char balance;         // AVL balance code (-1 left tall, 0 - balanced, +1 right tall)
  unsigned char reserved[7];  
  void   *key;                 // key pointer - or parent pointer if using object tree routines
  } NB_TreeNode;

typedef struct NB_TREE_PATH{
  void  *key;                  // Key pointer
  NB_TreeNode **rootP;         // Addrees of root node pointer
  NB_TreeNode **nodeP;         // Address of pointer to node found or insertion point
  NB_TreeNode **balanceP;      // Address of top node to update balance factor on insert
  NB_TreeNode *node[32];       // path node array
  int   step[32];              // path step array
  int   depth;                 // index into path
  int   balanceDepth;          // index of top node to update balance factors on insert   
  } NB_TreePath;

#define NB_TREE_FIND(KEY,NODE) \
  while(NODE!=NULL){ \
    if(KEY<NODE->key) NODE=NODE->left; \
    else if(KEY>NODE->key) NODE=NODE->right; \
    else break; \
    }

#define NB_TREE_FIND_COND_RIGHT(RIGHT,NODE) \
  while(NODE!=NULL){ \
    if(RIGHT<((NB_Cond *)NODE->key)->right) NODE=NODE->left; \
    else if(RIGHT>((NB_Cond *)NODE->key)->right) NODE=NODE->right; \
    else break; \
    }

typedef struct NB_TREE_ITERATOR{
  NB_TreeNode *right[32];
  NB_TreeNode **rightP;
  } NB_TreeIterator;

#define NB_TREE_ITERATE(ITERATOR,NODE,ROOT) \
  NODE=(NB_TreeNode *)ROOT; \
  ITERATOR.rightP=&ITERATOR.right[0]; \
  if(NODE!=NULL) while(1)

#define NB_TREE_ITERATE_NEXT(ITERATOR,NODE) \
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
#define NB_TREE_ITERATE2(ITERATOR,NODE,ROOT) \
  NODE=(NB_TreeNode *)ROOT; \
  ITERATOR.rightP=&ITERATOR.right[0]; \
  while(NODE!=NULL)

#define NB_TREE_ITERATE_NEXT2(ITERATOR,NODE) \
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

#define NB_TREE_ITERATE_ORDER(ITERATOR,NODE,ROOT) \
  NODE=(NB_TreeNode *)ROOT; \
  ITERATOR.rightP=&ITERATOR.right[0]; \
  if(NODE!=NULL) while(NODE->left!=NULL){ \
    *ITERATOR.rightP=NODE; \
    ITERATOR.rightP++; \
    NODE=NODE->left; \
    } \
  if(NODE!=NULL) while(1)

#define NB_TREE_ITERATE_ORDER_NEXT(ITERATOR,NODE) \
  if(NODE->right!=NULL){ \
    NODE=NODE->right; \
    while(NODE->left!=NULL){ \
      *ITERATOR.rightP=NODE; \
      ITERATOR.rightP++; \
      NODE=NODE->left; \
      } \
    } \
  else{ \
    ITERATOR.rightP--; \
    if(ITERATOR.rightP<&ITERATOR.right[0]) break; \
    NODE=*ITERATOR.rightP; \
    }

#define NB_TREE_ITERATE_ORDER_STRING_CASE_AFTER(ITERATOR,NODE,ROOT,AFTER) \
  NODE=(NB_TreeNode *)ROOT; \
  ITERATOR.rightP=&ITERATOR.right[0]; \
  if(NODE!=NULL) while(NODE->left!=NULL && strcasecmp(NODE->key,AFTER)>0){ \
    *ITERATOR.rightP=NODE; \
    ITERATOR.rightP++; \
    NODE=NODE->left; \
    } \
  if(NODE!=NULL) while(1)

#define NB_TREE_ITERATE_ORDER_STRING_CASE_AFTER_NEXT(ITERATOR,NODE,AFTER) \
  if(NODE->right!=NULL){ \
    NODE=NODE->right; \
    while(NODE->left!=NULL && strcasecmp(NODE->key,AFTER)>0){ \
      *ITERATOR.rightP=NODE; \
      ITERATOR.rightP++; \
      NODE=NODE->left; \
      } \
    } \
  else{ \
    ITERATOR.rightP--; \
    if(ITERATOR.rightP<&ITERATOR.right[0]) break; \
    NODE=*ITERATOR.rightP; \
    }

//=============================================================================

void *nbTreeLocateTerm(NB_TreePath *path,void *term,NB_TreeNode **rootP);
void *nbTreeFindCondRight(void *right,NB_TreeNode *node);
void *nbTreeLocateCondRight(NB_TreePath *path,void *right,NB_TreeNode **rootP);
void *nbTreeLocateCondRightString(NB_TreePath *path,char *right,NB_TreeNode **rootP);
void *nbTreeLocateCondRightReal(NB_TreePath *path,double right,NB_TreeNode **rootP);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeFind(void *key,NB_TreeNode *root);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeFindString(char *key,NB_TreeNode *root);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeFindStringCase(char *key,NB_TreeNode *root);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeFindValue(
     void *key,
     NB_TreeNode *root,
     int (*compare)(void *handle,void *key1,void *key2),
     void *handle);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeFindFloor(
     void *key,
     NB_TreeNode *root,
     int (*compare)(void *handle,void *key1,void *key2),
     void *handle);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeLocate(NB_TreePath *path,void *key,NB_TreeNode **rootP);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeLocateString(NB_TreePath *path,char *key,NB_TreeNode **rootP);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeLocateStringCase(NB_TreePath *path,char *key,NB_TreeNode **rootP);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeLocateBinary(NB_TreePath *path,void *key,size_t size,NB_TreeNode **rootP);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeLocateValue(NB_TreePath *path,void *key,NB_TreeNode **rootP,
    int (*compare)(void *handle,void *key1,void *key2),
    void *handle);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbTreeInsert(NB_TreePath *path,NB_TreeNode *newNode);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void *nbTreeRemove(NB_TreePath *path);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern NB_TreeNode *nbTreeFlatten(NB_TreeNode **nodeP,NB_TreeNode *node);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern NB_TreeNode *nbTreeBalance(NB_TreeNode *node,int n,NB_TreeNode **nextP);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbTreeInsertBalance(NB_TreeNode *root,NB_TreeNode *node);

#endif
