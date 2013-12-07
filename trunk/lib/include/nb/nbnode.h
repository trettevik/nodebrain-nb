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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbnode.h 
*
* Title:    Header for Node Cell Management Routines
*
* See nbnode.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/08/25 Ed Trettevik (original split from nbcache.c)
* 2005/04/08 eat 0.6.2  API function definitions moved to nbapi.h
* 2005/05/14 eat 0.6.3  Added module handle in NB_SKILL_BIND
* 2007/07/16 eat 0.6.8  change name from "expert" to "node"
* 2010/02/28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-10-17 eat 0.8.12 Added size parameter to nbNodeGetNameFull
* 2013-01-11 eat 0.8.13 Checker updates
*=============================================================================
*/
#ifndef _NB_NODE_H_
#define _NB_NODE_H_

#if defined(NB_INTERNAL)

#include <nb/nbstem.h>
#include <nb/nblist.h>

/*
*  A node cell represents the combination of skill and knowledge.  Basic
*  skills and knowledge representation are supported by NodeBrain.  Extensions
*  are supported by skill modules.
*/
struct NB_NODE{
  struct NB_CELL   cell;        /* cell object header */
  struct NB_TERM   *context;    /* term serving as context handle */
  struct NB_TERM   *reference;  /* reference term */ 
  struct IDENTITY  *owner;      /* identity that owns the context */
  struct STRING    *source;     /* command to request unknown values */
  struct ACTION    *ifrule;     /* if rules - check on alert */
  unsigned char    cmdopt;      /* command option - see NB_CMDOPT_* in nbcmd.h */ // 2013-01-11 eat - VID 6552
  char             reserved;    /* reserved */
  unsigned short   alertCount;  /* wrap-around counter for alerts */
  struct NB_SKILL  *skill;      /* skill managing this node */
  struct NB_FACET  *facet;      // facet list - first is prmary facet
  /* struct NB_LIST   *args; */      /* arguments at definition time */
  /* struct STRING    *text; */      /* text at definition time */
  void             *knowledge;  /* knowledge handle - e.g. CACHE */
  };

typedef struct NB_NODE NB_Node;


/*******************************************************************************
*  Node/Context Methods
*******************************************************************************/
NB_Node *nbNodeNew(void);
void contextAlert(NB_Term *term);

/*******************************************************************************
* Skill structures and functions 
*******************************************************************************/
typedef struct NB_SKILL{
  struct NB_OBJECT  object;
  struct NB_TERM   *term;
  int               status; // 0 - need binding, 1 - binding complete
  struct STRING    *ident;  // identifier [<module>.]symbol
  struct NB_LIST   *args;
  struct STRING    *text;
  struct NB_FACET  *facet;  // facet list
  void             *handle;
  } NB_Skill;

typedef void *(*NB_SKILL_BIND)(struct NB_TERM *context,void *moduleHandle,NB_Skill *skill,NB_List *args,char *source);

typedef struct NB_FACET{
  struct NB_OBJECT  object;
  struct NB_SKILL   *skill;
  struct STRING     *ident;
  void             *(*construct)(struct NB_TERM *context,void *skillHandle,NB_Cell *args,char *text);
  void             *(*destroy)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  void              (*show)(struct NB_TERM *context,void *skillHandle,void *objectHandle,int option);
  int               (*enable)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  int               (*disable)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  void              (*alarm)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  int               (*assert)(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value);
  struct NB_OBJECT *(*eval)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args);
  struct NB_OBJECT *(*compute)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args);
  void              (*solve)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args);
  int               (*command)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args,char *text);
  int               (*alert)(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value);
  struct NB_FACET_SHIM *shim;  // shim for facet methods
  } NB_Facet;

typedef struct NB_FACET_SHIM{
  void             *(*construct)(struct NB_TERM *context,void *skillHandle,NB_Cell *args,char *text);
  void             *(*destroy)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  void              (*show)(struct NB_TERM *context,void *skillHandle,void *objectHandle,int option);
  int               (*enable)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  int               (*disable)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  void              (*alarm)(struct NB_TERM *context,void *skillHandle,void *objectHandle);
  int               (*assert)(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value);
  int     assertFlags;
  clock_t assertTicks;
  struct NB_OBJECT *(*eval)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args);
  struct NB_OBJECT *(*compute)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args);
  void              (*solve)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args);
  int               (*command)(struct NB_TERM *context,void *skillHandle,void *objectHandle,struct NB_LIST *args,char *text);
  int               (*alert)(struct NB_TERM *context,void *skillHandle,void *objectHandle,NB_Cell *arglist,NB_Cell *value);
  int     alertFlags;
  clock_t alertTicks;
  } NB_FacetShim;


/* 
* NOTE: 
*
*   Effectively, we are extending COND with SKILLCOND by adding another type
*   and associated evaluation methods.  This structure must have the
*   same basic structure as COND (cell and two pointers).
*/
struct NB_CALL{             /* Skill cell object */
  struct NB_CELL   cell;    /* cell header */
  struct NB_TERM  *term;    /* term pointing to node object */
  // when we start using the facet here, we need to stop managing it as a cond
  //struct NB_FACET *facet;   /* facet within the node's associated skill */
  struct NB_LIST  *args;    /* argument list */
  };

extern struct TYPE *skillType;
extern struct TYPE *condTypeNode; 
extern struct TYPE *nb_NodeType;
extern struct NB_TERM *nb_SkillGloss;

/* Functions */

void nbNodeInit(NB_Stem *stem);
struct NB_SKILL     *nbSkillParse(NB_Term *context,char *cursor);
struct NB_LIST      *nbSkillArgs(NB_Term *context,char **curptr);
struct NB_SKILLCOND *nbSkillCondUse(NB_Term *context,char **cursor,char *msg);
struct NB_SKILLCOND *nbSkillAssertionUse(NB_Term *context,char **cursor,char *msg);
int nbSkillCmd(NB_Term *context,char **cursor,char *msg);

NB_Term *nbNodeParse(NB_Term *context,char *ident,char *cursor);

struct NB_SKILL *nbSkillNew(char *ident,NB_List *arglist,char *text);
struct NB_FACET *nbFacetNew(NB_Skill *skill,const char *ident);

#endif // NB_INTERNAL

// External API

#define NB_NODE_CONSTRUCT 1  /* Construct a node */
#define NB_NODE_ASSERT    2  /* Assert something to a node */
#define NB_NODE_EVALUATE  3  /* Evaluate a node condition */
#define NB_NODE_COMPUTE   4  /* Compute a node condition */
#define NB_NODE_SOLVE     5  /* Solve a node condition */
#define NB_NODE_SHOW      6  /* Show a node */
#define NB_NODE_ENABLE    7  /* Eabled a node */
#define NB_NODE_DISABLE   8  /* Disable a node */
#define NB_NODE_DESTROY   9  /* Destroy a node */
#define NB_NODE_COMMAND  10  /* Interpret a node command */
#define NB_NODE_ALARM    11  /* Alarm a node */
#define NB_NODE_ALERT    12  /* Alert a node */


#define NB_SHOW_ITEM   0 /* show node as single line item */
#define NB_SHOW_REPORT 1 /* show node as multi-line report */


#if defined(WIN32)
__declspec(dllexport)
#endif
extern char *nbNodeGetName(nbCELL context);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern char *nbNodeGetNameFull(nbCELL context,char *name,size_t size);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void *nbNodeGetKnowledge(nbCELL context,nbCELL cell);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern int nbNodeSetLevel(nbCELL context,nbCELL cell);

#if defined(WIN32)
__declspec(dllexport)
#endif
extern void nbNodeSetValue(nbCELL context,nbCELL cell);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbNodeCmd(nbCELL context,char *name,char *cursor);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern int nbNodeCmdIn(nbCELL context,nbCELL args,char *text);

#if defined(WIN32)
_declspec (dllexport)
#endif
extern void nbNodeAlert(nbCELL context,nbCELL node);


#endif
