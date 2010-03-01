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
* Program:  nodebrain
*
* File:     nbsynapse.c 
* 
* Title:    Synapse Management Routines
*
* Function:
*
*   This header provides routines that manage NodeBrain synapses (NB_Synapse), an
*   extension of NB_Object.  These routines provide a mechanism on top of the
*   cell subscription and publication scheme that enables specific actions in
*   response to changes in specific cells.
*
*   
* Synopsis:
*
*   (Internal):
*
*   #include "nb.h"
*
*   void nbSynapseInit();
*
*   (API):
*
*   #include "nbapi.h"
*
*   void nbSynapseInit(NB_Stem *stem);
*   nbCELL nbSynapseOpen(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell,
*          void (*handler)(nbCELL context,void *skillHandle,void *nodeHandle,nbCELL cell)); 
*   void nbSynapseSetTimer(nbCELL synapse,int seconds);
*   void *nbSynapseClose(nbCELL synapse);
*
*
* Description
*
*   nbSynapseInit()    - Initialize synapse object environment
*   nbSynapseOpen()    - Construct a synapse object and subscribe to specific cell
*   nbSynapseClose()   - unsubscribe to specific cell and destroy synapse 
*
*   A synapse is a simple object that subscribes to a single cell and calls a
*   handler function whenever the cell changes.  The call to the handler is similar
*   to that of a skill module method.
*
*   We do not provide much visibility for synapses.  For now we are willing to
*   allow the handle and handler to be invisible; that is, we do not provide
*   for the display of handles and handlers.  To support the "show impact"
*   option, we provide a minimal print method for a synapse.  
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2004-12-02 Ed Trettevik (original prototype version introduced in 0.6.2)
* 2007-07-21 eat 0.6.8  Modified to simplify use within skill modules.
* 2008-05-24 eat 0.7.0  Modified to include nbSynapseSetTimer function
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#include "nbi.h"

struct NB_SYNAPSE *nb_SynapsePool;
struct TYPE *nb_SynapseType;

void nbSynapsePrint(struct NB_SYNAPSE *synapse){
  outPut("(synapse)");
  }

void nbSynapseAlert(struct NB_SYNAPSE *synapse){
  (*synapse->handler)(synapse->context,synapse->skillHandle,synapse->nodeHandle,synapse->cell);
  }

/*
*  Initialization Cells 
*/
void nbSynapseInit(NB_Stem *stem){
  nb_SynapseType=newType(stem,"synapse",NULL,0,nbSynapsePrint,NULL);
  nb_SynapseType->alert=nbSynapseAlert;
  nb_SynapseType->alarm=nbSynapseAlert;
  }

/*
*  Open a synapse
*/
NB_Cell *nbSynapseOpen(NB_Cell *context,void *skillHandle,void *nodeHandle,NB_Cell *cell,
  void (*handler)(NB_Cell *context,void *skillHandle,void *nodeHandle,NB_Cell *cell)){

  NB_Synapse *synapse;
  synapse=newObject(nb_SynapseType,(void **)&nb_SynapsePool,sizeof(struct NB_SYNAPSE));
  synapse->context=context;
  synapse->skillHandle=skillHandle;
  synapse->nodeHandle=nodeHandle;
  synapse->cell=cell;
  synapse->handler=handler;
  if(trace) nbLogMsg(context,0,'T',"nbSynapseOpen: calling nbCellEnable");
  if(cell) nbCellEnable(cell,(NB_Cell *)synapse); /* subscribe to cell */
  return((NB_Cell *)synapse);
  }

/*
* Schedule a synapse to fire after a specified number of seconds
*
*   You can schedule a synapse to fire after zero seconds to cause it to fire 
*   immediately after giving other events an opportunity to fire.
*
*/
void nbSynapseSetTimer(nbCELL context,nbCELL synapse,int seconds){
  time_t at;
  time(&at);
  at+=seconds;
  nbClockSetTimer(at,synapse);
  }

/*
*  Close a synapse and obtain the handle
*    
*    The caller is responsible for disposing of the handle if necessary
*
*  Returns: handle pointer  
*/
void *nbSynapseClose(NB_Cell *context,NB_Cell *synapse){
  if(((NB_Synapse *)synapse)->cell) nbCellDisable(((NB_Synapse *)synapse)->cell,(NB_Cell *)synapse);
  synapse->object.next=(NB_Object *)nb_SynapsePool;
  nb_SynapsePool=(NB_Synapse *)synapse;
  return(NULL);
  }
