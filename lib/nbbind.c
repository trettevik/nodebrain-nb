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
*   to include the additional source files for the library.
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
