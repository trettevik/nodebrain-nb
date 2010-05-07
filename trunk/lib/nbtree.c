/*
* Copyright (C) 2006-2010 The Boeing Company
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
* File:     nbtree.c
*
* Title:    Binary Tree Routines
*
* Purpose:
*
*   This file contains functions used to manage binary trees keeping them
*   reasonably well balanced.
*
*
* Synopsis:
*
*   void *nbTreeFind(void *key,NB_TreeNode *root);
*
*   void *nbTreeFindValue(
*     void *key,
*     NB_TreeNode *root,
*     int (*compare)(void *handle,void *key1,void *key2),
*     void *handle);
*
*   void *nbTreeFindFloor(
*     void *key,
*     NB_TreeNode *root,
*     int (*compare)(void *handle,void *key1,void *key2),
*     void *handle);
*
*   void *nbTreeLocate(NB_TreePath *path,void *key,NB_TreeNode **rootP);
*
*   void *nbTreeLocateValue(NB_TreePath *path,void *key,NB_TreeNode **rootP,
*    int (*compare)(void *handle,void *key1,void *key2),
*    void *handle);
*
*   void nbTreeInsert(NB_TreePath *path,NB_TreeNode *newNode);
*
*   void *nbTreeRemove(NB_TreePath *path);
*
*
* Reference:
*
*   Search for "AVL tree" on the web for a general explanation of the algorithm.
*
* Description:
*   
*   This code is an adaptation of a small subset of Ben Pfaff's GNU libavl 2.0.2.  
*
*    1) The search steps of the insert and remove operations have been implemented as a locate
*       function to be called prior to calling insert or remove.  The locate function creates a
*       path structure to be used in a subsequent call to insert or remove.  This enables the
*       calling program to avoid having to perform two searches in situations where access to
*       an entry is required before deciding if a delete operation is desired.  This is especially
*       useful for recursive algorithms that operate on a complex collection of trees.  The path
*       structure can be a local variable in recursive or looping algorithms, enabling calls to 
*       locate for multiple trees before deciding which remove operations to perform.  The caller
*       must take care to avoid using an "obselete" path structure for insert or remove operations.
*       A path structure for a given tree is made obselete by a call to insert or remove on the
*       the same tree.
*
*    2) Two search order options are supported: a) "key address order" and b) "key value order".  Key
*       address order enables more efficient searches because key pointers can be compared without
*       use of a callback comparison function.  The nbTreeFind() and nbTreeLocate() functions use
*       key address order searches.  Since NodeBrain only stores one copy of any given cell value,
*       two cells are equal when their cell pointers are equal. When cell pointers are used as keys
*       to nbTreeFind() and nbTreeLocate() we maintain a balanced but unordered tree.  This works fine
*       as long as we don't need an ordered tree.  The nbTreeFindValue(), nbTreeFindFloor() and
*       nbTreeLocateValue() functions are used for key value ordered trees.  For a given tree, one
*       must be consistent in calling the right functions for the ording option selected.  Mixing
*       functions will cause unpredictable results.  The nbTreeInsert() and nbTreeRemove() functions
*       are not impacted by the ordering option selected---another benefit of item (1) above.
*
*    3) You might wonder why the ordering option is not defined in the tree header structure so
*       inconsistent calls can be avoided.  This is because the tree header has been eliminated from
*       these routines to enable single pointer headers in complex tree structures.  So the calling
*       program is required to keep track of the ordering option.       
*       
*    4) The node structure has been modified by replacing the item pointer with a key pointer.  For
*       NodeBrain we expect the node structure to be the first element in an "item" structure, so a
*       node pointer is a pointer to an "item."  However, programs calling these routines may elect
*       to use the key pointer as an item pointer when using key value ordered trees because then
*       it is up to the callback comparison function how the key value is used.  In that case, the
*       node structure may be used as an independent structure instead of as an item header.
* 
*    5) Memory management is not a concern of the nbTreeInsert() and nbTreeRemove() functions so
*       allocate and free callback functions are not required.  This results from using node pointers
*       as arguments and return values instead of separate "item" pointers.  This is also made
*       convenient by the removal of the search step from the insert function.
*
*    6) As a personal preference, labels are used for left and right pointers instead of using a
*       (perhaps more elegant) 2-pointer array.  The impact on performance has not been studied.
*
*    5) Variable names were changed from letters to words where it seems more clear to the author.
*       The letter notation has been retained in nbTreeInsert() and nbTreeRemove() to aid in 
*       comparision to the referenced algorithm during debugging.
*
*   Only the Search, Insert, and Remove operations are implemented here, and they are reorganized as
*   follows. 
*
*     nbTreeFind()         - Find a node using "key address ordering"
*     nbTreeFindValue()    - Find a node using "key value ordering"
*     nbTreeFindFloor()    - Find the max node <= key using "key value ordering"
*     nbTreeLocate()       - Locate a node, constructing a path structure for use by insert or remove
*     nbTreeLocateValue()  - Like nbTreeLocate(), but used for "key value ordered" trees
*     nbTreeLocateString() - Like nbTreeValue(), but has built-in string comparison
*     nbTreeInsert()       - Insert a node using a path structure returned by a locate function
*     nbTreeRemove()       - Remove a node using a path structure returned by a locate function
*
*   Since the nbTreeFind() function is so simple, there may be cases where it is more efficient
*   for a calling program to use a search loop.  Traversal is equally simple using recursive functions
*   so no traversal function has been provided.
*
*     void MyDoSomethingTraversal(NB_TreeNode *node){
*       if(node->left!=NULL) MyDoSomethingTraversal(node->left);
*       ... do something with node ...
*       if(node->right!=NULL) MyDoSomethingTraversal(node->right);
*       }
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2006/09/02 Ed Trettevik - first prototype
* 2007/05/22 eat 0.6.8 - removed from nb_mod_tree.c and made part of the API
*=============================================================================
*/
#include "nbi.h"

// nbTreeFind()    - Binary tree search function
//
//   key       - pointer to key used for comparison
//   root      - pointer to root node
//
// Returns node ptr when found, otherwise NULL.
//
// This function is called version of the code generated by the NB_TREE_FIND
// macro.  Calling code can use the macro directly to generate inline code
// and avoid a call.
// 
void *nbTreeFind(void *key,NB_TreeNode *node){
  NB_TREE_FIND(key,node)
/*
  while(node!=NULL){ 
    if(key<node->key) node=node->left;
    else if(key>node->key) node=node->right; // need to use assembler to avoid double comparison
    else break;
    }
*/
  return(node);
  }

// nbTreeFindValue()    - Binary tree search function
//
//   key       - pointer to key used for comparison
//   root      - pointer to root node
//   compare   - function to compare the key to a node's key returning (-1,0,+1) like strcmp
//   handle    - used by caller to pass information to the compare function
//
// Returns node ptr when found, otherwise NULL.
//
void *nbTreeFindValue(
  void *key,
  NB_TreeNode *root,
  int (*compare)(void *handle,void *key1,void *key2),
  void *handle){

  int cmp;
  NB_TreeNode *node=root;

  if(compare==NULL) return(nbTreeFind(key,node));
  while(node!=NULL){ 
    if((cmp=compare(handle,key,node->key))==0) return(node);
    if(cmp>0) node=node->right;
    else node=node->left;
    }
  return(NULL);
  }

// Find String - Case Sensitive
//
//   key       - pointer to key used for comparison
//   root      - pointer to root node
//
// Returns node ptr when found, otherwise NULL.
//
void *nbTreeFindString(char *key,NB_TreeNode *root){
  int cmp;
  NB_TreeNode *node=root;

  while(node!=NULL){
    //fprintf(stderr,"nbTreeFindString key=%s node->key=%s\n",key,(char *)node->key);
    if((cmp=strcmp(key,node->key))==0) return(node);
    if(cmp>0) node=node->right;
    else node=node->left;
    }
  return(NULL);
  }

// Find String Case Insensitive
//
//   key       - pointer to key used for comparison
//   root      - pointer to root node
//
// Returns node ptr when found, otherwise NULL.
//
void *nbTreeFindStringCase(char *key,NB_TreeNode *root){
  int cmp;
  NB_TreeNode *node=root;

  while(node!=NULL){
    //fprintf(stderr,"nbTreeFindString key=%s node->key=%s\n",key,(char *)node->key);
    if((cmp=strcasecmp(key,node->key))==0) return(node);
    if(cmp>0) node=node->right;
    else node=node->left;
    }
  return(NULL);
  }

// nbTreeFindFloor()   - Binary tree search for max node less than or equal to key
//
// Same arguments and return value as nbTreeFind

void *nbTreeFindFloor(
  void *key,
  NB_TreeNode *root,
  int (*compare)(void *handle,void *key1,void *key2),
  void *handle){

  int cmp;
  NB_TreeNode *node=root,*floor=NULL;

  while(node!=NULL){ 
    if((cmp=compare(handle,key,node->key))==0) return(node);
    if(cmp>0) floor=node, node=node->right;
    else node=node->left;
    }
  return(floor);
  }

// nbTreeLocate()  - AVL tree node locate function
//
void *nbTreeLocate(NB_TreePath *path,void *key,NB_TreeNode **rootP){
  NB_TreeNode *node;     // Node pointer 
  NB_TreeNode **nodeP;   // Address of node pointer 
  int depth=0;     // index into path     

  path->key=key;
  path->rootP=nodeP=path->balanceP=rootP; 
  path->balanceDepth=1;
  path->node[depth]=(NB_TreeNode *)rootP; // this is a trick that depend on the left pointer
  path->step[depth++]=0;                  // being the first element of the node structure
  for(node=*rootP;node!=NULL;node=*nodeP){
    if(key==node->key) break;
    if(node->balance!=0) path->balanceP=nodeP, path->balanceDepth=depth;
    path->node[depth]=node;
    if((path->step[depth++]=(key>node->key))) nodeP=&node->right;
    else nodeP=&node->left;
    }
  path->nodeP=nodeP;
  path->depth=depth;
  return(node);
  }

// nbTreeLocateValue()  - AVL tree node location function
//
void *nbTreeLocateValue(NB_TreePath *path,void *key,NB_TreeNode **rootP,
  int (*compare)(void *handle,void *key1,void *key2),
  void *handle){

  NB_TreeNode *node;     // Node poiter 
  NB_TreeNode **nodeP;   // Address of node pointer 
  int depth=0;     // index into path
  int cmp;         // comparison result

  if(compare==NULL) return(nbTreeLocate(path,key,rootP));
  path->key=key;   // save key for insertions
  path->rootP=nodeP=path->balanceP=rootP; 
  path->balanceDepth=1;
  path->node[depth]=(NB_TreeNode *)rootP; // this is a trick that depend on the left pointer
  path->step[depth++]=0;            // being the first element of the node structure
  for(node=*rootP;node!=NULL;node=*nodeP){
    if((cmp=compare(handle,key,node->key))==0) break;
    cmp=cmp>0;          // 0 left, 1 right
    if(node->balance!=0) path->balanceP=nodeP, path->balanceDepth=depth;
    path->node[depth]=node;
    if((path->step[depth++]=cmp)) nodeP=&node->right;
    else nodeP=&node->left;
    }
  path->nodeP=nodeP;
  path->depth=depth;
  return(node);
  }

// Locate String - case sensitive
//
void *nbTreeLocateString(NB_TreePath *path,char *key,NB_TreeNode **rootP){
  NB_TreeNode *node;     // Node poiter
  NB_TreeNode **nodeP;   // Address of node pointer
  int depth=0;     // index into path
  int cmp;         // comparison result

  //fprintf(stderr,"nbTreeLocateString: called keyat=%p key=%s rootP=%x *rootP=%x\n",key,key,rootP,*rootP);
  //if(*rootP!=NULL) fprintf(stderr,"nbTreeLocateString: root keyat=%x key=%s\n",&(*rootP)->key,(*rootP)->key);
  path->key=key;   // save key for insertions
  path->rootP=nodeP=path->balanceP=rootP;
  path->balanceDepth=1;
  path->node[depth]=(NB_TreeNode *)rootP; // this is a trick that depend on the left pointer
  path->step[depth++]=0;            // being the first element of the node structure
  for(node=*rootP;node!=NULL;node=*nodeP){
    //fprintf(stderr,"nbTreeLocateString: key=%s node->keyat=%x node->key=%s\n",key,&node->key,(char *)node->key);
    if((cmp=strcmp(key,(char *)node->key))==0) break;
    cmp=cmp>0;          // 0 left, 1 right
    if(node->balance!=0) path->balanceP=nodeP, path->balanceDepth=depth;
    path->node[depth]=node;
    if((path->step[depth++]=cmp)) nodeP=&node->right;
    else nodeP=&node->left;
    }
  path->nodeP=nodeP;
  path->depth=depth;
  //if(node==NULL) fprintf(stderr,"nbTreeLocateString: node=%x\n",node);
  //else fprintf(stderr,"nbTreeLocateString: node=%x key=%s\n",node,node->key);
  return(node);
  }

// Locate String Case Insensitive 
//
void *nbTreeLocateStringCase(NB_TreePath *path,char *key,NB_TreeNode **rootP){
  NB_TreeNode *node;     // Node poiter
  NB_TreeNode **nodeP;   // Address of node pointer
  int depth=0;     // index into path
  int cmp;         // comparison result

  //fprintf(stderr,"nbTreeLocateString: called keyat=%p key=%s rootP=%x *rootP=%x\n",key,key,rootP,*rootP);
  //if(*rootP!=NULL) fprintf(stderr,"nbTreeLocateString: root keyat=%x key=%s\n",&(*rootP)->key,(*rootP)->key);
  path->key=key;   // save key for insertions
  path->rootP=nodeP=path->balanceP=rootP;
  path->balanceDepth=1;
  path->node[depth]=(NB_TreeNode *)rootP; // this is a trick that depend on the left pointer
  path->step[depth++]=0;            // being the first element of the node structure
  for(node=*rootP;node!=NULL;node=*nodeP){
    //fprintf(stderr,"nbTreeLocateString: key=%s node->keyat=%x node->key=%s\n",key,&node->key,(char *)node->key);
    if((cmp=strcasecmp(key,(char *)node->key))==0) break;
    cmp=cmp>0;          // 0 left, 1 right
    if(node->balance!=0) path->balanceP=nodeP, path->balanceDepth=depth;
    path->node[depth]=node;
    if((path->step[depth++]=cmp)) nodeP=&node->right;
    else nodeP=&node->left;
    }
  path->nodeP=nodeP;
  path->depth=depth;
  //if(node==NULL) fprintf(stderr,"nbTreeLocateString: node=%x\n",node);
  //else fprintf(stderr,"nbTreeLocateString: node=%x key=%s\n",node,node->key);
  return(node);
  }



// nbTreeInsert()  - AVL tree node insertion function
//
//   path      - pointer to path structure returned by nbTreeLocate() or nbTreeLocateValue()    
//   newNode   - pointer to node structure (header to callers node structure)
// 
void nbTreeInsert(NB_TreePath *path,NB_TreeNode *newNode){
  NB_TreeNode *node;   // node iterator
  NB_TreeNode *y;      // node to balance
  NB_TreeNode *w;      // new root of rebalanced subtree
  NB_TreeNode *x;
  int depth;     // path depth

  // Insert new node

  newNode->left=newNode->right=NULL;
  newNode->balance=0;
  newNode->key=path->key;
  *path->nodeP=newNode;

  // Update balance factors

  if((y=*path->balanceP)==NULL) return;  // still balanced

  for(node=y,depth=path->balanceDepth;node!=newNode;depth++){
    if(path->step[depth]==0){
      node->balance--;
      node=node->left;
      }
    else{
      node->balance++;
      node=node->right;
      }
    // consider using path->node instead of left and right
    }

  // Rebalance

  if(y->balance==-2){        // rebalance after left insertion 
    x=y->left;
    if(x->balance==-1){      // Rotate right at y
      w=x;
      y->left=x->right;
      x->right=y;
      x->balance= y->balance=0; 
      }
    else{                    // Rotate left at x and then right at y 
      w=x->right;
      x->right=w->left;
      w->left=x;
      y->left=w->right;
      w->right=y;
      if(w->balance==-1) x->balance=0, y->balance=+1;
      else if(w->balance==0) x->balance= y->balance=0;
      else /* w->balance==+1 */ x->balance=-1, y->balance=0;
      w->balance=0; 
      }
    }
  else if(y->balance==+2){   // rebalance after right insertion 
    x=y->right;
    if(x->balance==+1){      // Rotate left at y 
      w=x;
      y->right=x->left;
      x->left=y;
      x->balance=y->balance=0;
      }
    else{                    // Rotate right at x then left at y 
      w=x->left;
      x->left=w->right;
      w->right=x;
      y->right=w->left;
      w->left=y;
      if(w->balance==+1) x->balance=0, y->balance=-1;
      else if(w->balance==0) x->balance= y->balance=0;
      else /* w->balance == -1 */ x->balance=+1, y->balance=0;
      w->balance=0; 
      } 
    }
  else return;
  *path->balanceP=w;            // replace root node
  }


// nbTreeRemove()  - AVL tree node removal routine
//
//   path      - pointer to path structure returned by nbTreeLocate() or nbTreeLocateValue()    
//   newNode   - pointer to node structure (header to callers node structure)
// 
// Returns address of deleted (removed) node or NULL if not found
//           

void *nbTreeRemove(NB_TreePath *path){ 
  NB_TreeNode *node;   // node to be removed
  NB_TreeNode **nodeP; // pointer to node pointer - used for steping left and right
  NB_TreeNode *r,*s,*w;// temp node pointers
  NB_TreeNode *y;
  NB_TreeNode *x;        
  int   depth;   // path depth
  int   j;       // temp depth index 

  depth=path->depth;
  node=*path->nodeP;

  // Remove node

  if(path->step[depth-1]) nodeP=&(path->node[depth-1])->right;
  else nodeP=&(path->node[depth-1])->left;
  if((r=node->right)==NULL) *nodeP=node->left;  // Case 1: Right link is null - replace with left
  else if(r->left==NULL){      // Case 2: Right->left is null - replace with right
    r->left=node->left;
    r->balance=node->balance;
    *nodeP=r;
    path->step[depth]=1;
    path->node[depth++]=r;
    }
  else{                        // Case 3: Replace with leftmost or right 
    j=depth++;
    for(;;){
      path->step[depth]=0;    // moving left
      path->node[depth++]=r;  
      s=r->left;    
      if(s->left==NULL) break;
      r=s;
      }
    s->left=node->left;
    r->left=s->right;
    s->right=node->right;
    s->balance=node->balance;
    if(path->step[j-1]) path->node[j-1]->right=s;
    else path->node[j-1]->left=s;
    path->step[j]=1;
    path->node[j]=s;
    }
   
// Update balance factors and rebalance

  while(--depth>0){
    y=path->node[depth];
    if(path->step[depth-1]) nodeP=&path->node[depth-1]->right;
    else nodeP=&path->node[depth-1]->left;
    if(path->step[depth]==0){     // Update y's balance factor after left-side deletion 
      y->balance++;
      if(y->balance==+1) break;
      else if(y->balance==+2){  // rebalance    
        x=y->right;
        if(x->balance==-1){  // left side deletion rebalance case 1
          w=x->left;
          x->left=w->right;
          w->right=x;
          y->right=w->left;
          w->left=y;
          if(w->balance==+1) x->balance=0,y->balance=-1;
          else if(w->balance==0) x->balance=y->balance=0;
          else /* w->balance == -1 */ x->balance=+1,y->balance=0;
          w->balance = 0;
          *nodeP=w; 
          }
        else{                   // left side deletion rebalance case 2
          y->right=x->left;
          x->left=y;
          *nodeP=x;
          if(x->balance==0){
            x->balance=-1;
            y->balance=+1;
            break;
            }
          else x->balance=y->balance=0; 
          } 
        } 
      }
    else{             // Update y's balance factor after right-side deletion
      y->balance--;
      if(y->balance==-1) break;
      else if(y->balance==-2){
        x=y->left;
        if(x->balance==+1){  // Rotate left at x then right at y
          w=x->right;
          x->right=w->left;
          w->left=x;
          y->left=w->right;
          w->right=y;
          if(w->balance==-1) x->balance=0,y->balance=+1;
          else if(w->balance==0) x->balance=y->balance=0;
          else /* w->balance == +1 */ x->balance=-1,y->balance=0;
          w->balance=0;
          *nodeP=w;
          } 
        else{
          y->left=x->right;
          x->right=y;
          *nodeP=x;
          if(x->balance==0){
            x->balance=+1;
            y->balance=-1;
            break;
            }
          else x->balance=y->balance=0;
          }
        } 
      }
    }
  return((void *)node);
  }

/*
*  Recursive routine to flatten a subtree
*    Updates a pointer to the flattened substree (nodeP)
*    Returns the last node in the flattened subtree
*/
NB_TreeNode *nbTreeFlatten(NB_TreeNode **nodeP,NB_TreeNode *node){
  NB_TreeNode *lastNode;
  if(node->left!=NULL){
    lastNode=nbTreeFlatten(nodeP,node->left);  // flatten left side
    node->left=NULL;
    nodeP=&lastNode->right;
    }
  *nodeP=node;
  if(node->right!=NULL) return(nbTreeFlatten(&node->right,node->right));
  return(node);
  }

/*
*  Recursive routine to balance a subtree
*    Returns root node and updates next pointer
*    The next pointer identifies the second half of the list
*    which is null when called for the right half.
*/
NB_TreeNode *nbTreeBalance(NB_TreeNode *node,int n,NB_TreeNode **nextP){
  NB_TreeNode *leftroot;

  switch(n){
    case 0:
      node=NULL;
      break;
    case 1:
      *nextP=node->right;
      node->right=NULL;
      break;
    case 2:
      *nextP=node->right->right;
      node->right->right=NULL;
      break;
    case 3:
      node->right->left=node;
      node=node->right;
      node->left->right=NULL;
      *nextP=node->right->right;
      node->right->right=NULL;
      break;
    default:
      leftroot=nbTreeBalance(node,n/2,&node);
      node->left=leftroot;
      node->right=nbTreeBalance(node->right,n-n/2-1,nextP);
    }
  return(node);
  }

