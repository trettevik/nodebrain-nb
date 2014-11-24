## Process this file with automake to produce Makefile.in
## =============================================================================
##
## Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
##
## NodeBrain is free software; you can modify and/or redistribute it under the
## terms of either the MIT License (Expat) or the following NodeBrain License.
##
## Permission to use and redistribute with or without fee, in source and binary
## forms, with or without modification, is granted free of charge to any person
## obtaining a copy of this software and included documentation, provided that
## the above copyright notice, this permission notice, and the following
## disclaimer are retained with source files and reproduced in documention
## included with source and binary distributions. 
##
## Unless required by applicable law or agreed to in writing, this software is
## distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
## KIND, either express or implied.
##
##=============================================================================
## Program:  NodeBrain (nb)
##
## File:     Makefile.am
##
## Title:    NodeBrain API Test Suite automake input file
##
## Function:
##
##   This file is processed by automake to create the Makefile.in file when
##   a NodeBrain developer is creating a NodeBrain Library distribution.
##
## Description:
##   
##   You will only need this file if you are developing NodeBrain enhancements
##   or improving the install for you platform.
##
##   See http://www.gnu.org/software/automake for more information.
##
##=============================================================================
## Change History:
##
## Date       Name/Change
## ---------- -----------------------------------------------------------------
## 2014-11-15 Ed Trettevik - Introduced in version 0.9.03
##=============================================================================
     
noinst_PROGRAMS = eCellFunctions eNodeTerms eSkillMethods

EXTRA_DIST = \
  eCellFunctions.got \
  eNodeTerms.got \
  eSkillMethods.got \
  nbtest 

AM_CFLAGS = -Wall -I../../include
AM_LDFLAGS = -L../.libs -lnb

eCellFunctions_SOURCES = eCellFunctions.c
eNodeTerms_SOURCES = eNodeTerms.c
eSkillMethods_SOURCES = eSkillMethods.c

## Run a set of tests to check out a build

.PHONY: check
check: all
	./nbtest
