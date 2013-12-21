/*
* Copyright (C) 1998-2009 The Boeing Company
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbbind.c 
*
* Title:    Static Skill Bind Routine
*
* Function:
*
*   This file provides the nbBind() function that declares builtin extended
*   skills. 
*
* Description:
*
*   The nbBind() function is similar to the nbBind() function in dynamic skill
*   modules.  Both "register" extended skills, but instead of skills in a
*   dynamic skill module, nbBind() registers extended skills linked into the
*   static library. 
*
*   If you imbed NodeBrain in another program using the NodeBrain library,
*   you can use calls to nbSkillDeclare() in that program to register
*   statically linked extended skills supplied from outside the library.  Or
*   you can add new calls here and rebuild the NodeBrain library to include
*   your extended skills.  In that case, you will need to update Makefile.am
*   to include the additional source files for the library and run automake
*   to update the Makefile. 
*
*   It is best to use dynamic skill modules, or link extensions into another
*   program from outside the library, rather than updating the library.  But
*   this also gives us a clean way to extend the distributed library and nb
*   program.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2005/04/10 Ed Trettevik (separated from other source in 0.6.2)
* 2005/12/31 eat 0.6.4  Included servant module
* 2007/05/30 eat 0.6.8  Removed cache module
* 2008-10-17 eat 0.7.2  Removed servant module
*=============================================================================
*/
#include <nb/nb.h>

void nbBind(nbCELL context){
//  void *servantBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text);
//  nbSkillDeclare(context,servantBind,NULL,"","servant",NULL,"");
/*
*  Here's where additional skills can be declared.
*  Remember to add the source file nb_mod_<name>.c to
*  libnb_a_SOURCES in Makefile.am (assuming you have automake)
*/
  }
