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
