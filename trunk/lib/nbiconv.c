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
* Foundation, Inc., 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbiconv.c
*
* Title:    ASCII/EBCDIC Conversion
*
* Function:
*
*   This file provides an ASCII to EBCDIC and EBCDIC to
*   ASCII conversion function.  
*
* Synopsis:
*
*   #include "nbiconv.h"
*
*   nbiconv(buf1,buf2,table,len);
*
*   nbiconv(buf1,buf2,NB_E2A,len);  EBCDIC-to-ASCII
*   nbiconv(buf1,buf2,NB_A2E,len);  ASCII-to-EBCDIC
*
* Future:
*
*   We should probably use the iconv() function instead.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2002/11/19 eat 0.4.1  Introduced when porting to IBM USS environment.
* 2003/03/15 eat 0.5.1  Renamed to nbiconv.c and created new nbiconv.h
*=============================================================================
*/
unsigned char NB_A2E[256]={
  0,1,2,3,55,45,46,47,22,5,37,11,12,13,14,15,
  16,17,18,19,60,61,50,38,24,25,63,39,28,29,30,31,
  64,90,127,123,91,108,80,125,77,93,92,78,107,96,75,97,
  240,241,242,243,244,245,246,247,248,249,122,94,76,126,110,111,
  124,193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,
  215,216,217,226,227,228,229,230,231,232,233,173,224,189,95,109,
  121,129,130,131,132,133,134,135,136,137,145,146,147,148,149,150,
  151,152,153,162,163,164,165,166,167,168,169,192,79,208,161,7,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64};

unsigned char NB_E2A[256]={
  0,1,2,3,64,9,64,127,64,64,64,11,12,13,14,15,
  16,17,18,19,64,64,8,64,24,25,64,64,28,29,30,31,
  64,64,64,64,64,10,23,27,64,64,64,64,64,5,6,7,
  64,64,22,64,64,64,64,4,64,64,64,64,20,21,64,26,
  32,64,64,64,64,64,64,64,64,64,64,46,60,40,43,124,
  38,64,64,64,64,64,64,64,64,64,33,36,42,41,59,94,
  45,47,64,64,64,64,64,64,64,64,64,44,37,95,62,63,
  64,64,64,64,64,64,64,64,64,96,58,35,64,39,61,34,
  64,97,98,99,100,101,102,103,104,105,64,64,64,64,64,64,
  64,106,107,108,109,110,111,112,113,114,64,64,64,64,64,64,
  64,126,115,116,117,118,119,120,121,122,64,64,64,91,64,64,
  64,64,64,64,64,64,64,64,64,64,64,64,64,93,64,64,
  123,65,66,67,68,69,70,71,72,73,64,64,64,64,64,64,
  125,74,75,76,77,78,79,80,81,82,64,64,64,64,64,64,
  92,64,83,84,85,86,87,88,89,90,64,64,64,64,64,64,
  48,49,50,51,52,53,54,55,56,57,64,64,64,64,64,64};

/* Convert to/from ASCII/EBCDIC
*    This should be replaced with a function that
*    does a machine level translate instruction
*/
void nbiconv(buf1,buf2,table,len)
  unsigned char buf1[],buf2[],table[];
  int len;{

  while(len>0){
    buf2[len]=table[buf1[len]];
    len--;
    }
  }

