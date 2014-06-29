/*
* Copyright (C) 1998-2014 Ed Trettevik <eat@nodebrain.org>
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
* File:     nbiconv.h
*
* Title:    ASCII/EBCDIC Conversion
*
* Function:
*
*   This header provides an ASCII to EBCDIC and EBCDIC to
*   ASCII conversion function.  
*
* See nbiconv.c for more information.
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2003/03/15 eat 0.5.1  New header after renamed old one to nbiconv.c
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
extern unsigned char NB_A2E[];
extern unsigned char NB_E2A[];

void nbiconv(unsigned char buf1[],unsigned char buf2[],unsigned char table[],int len);
