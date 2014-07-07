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
* File:     nbsource.h
*
* Title:    File Source Routine Header 
*
* Function:
*
*   This header defines routines for sourcing NodeBrain rule files.
*
* See nbsource.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2010-11-07 Ed Trettevik - (Version 0.8.5 - split out from nbcmd.c)
* 2013-04-27 eat 0.8.15 Included option parameter in nbSource header
*=============================================================================
*/
#ifndef _NB_SOURCE_H_
#define _NB_SOURCE_H_

#if defined(NB_INTERNAL) 

int nbSourceTil(struct NB_CELL *context,FILE *file);
int nbSourceIgnoreTil(struct NB_CELL *context,FILE *file,char *buf,int til);
 
#endif // !NB_INTERNAL (external API)

void nbSource(nbCELL context,int option,char *cursor);

#endif 
