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
* Program:  NodeBrain API Test Suite
*
* File:     lib/test/eCellFunctions.c 
*
* Title:    API Test - Access to terms and their values within nodes.
*
* Category: Example - Short illustrations using sparse C code formatting.
*
* Function:
*
*   This file exercises NodeBrain API functions that define and use cell
*   functions.
*
* See NodeBrain Library manual for a description of each API function.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2014-11-21 Ed Trettevik - introduced in version 0.9.03
*=============================================================================
*/
#include <nb/nb.h>

#define TEST( TITLE ) nbLogPut( context, "\nTEST: line %5d - %s\n", __LINE__, TITLE )


static double xyminus2( double x, double y )
{
	return( x * y - 2 );
}


static double xtwice( double x )
{
	return( x*2 );
}


/*
*  This is a function to return a truncation of the input string.
*  The buffer size is adjust to
*/
char *buffer = NULL;
int   buflen = 0;

static char *strTruncate(char *str,int len)
{
	if( strlen( str ) < len ) len = strlen( str ); // will copy the lesser of the two

        if( buffer && len >= buflen )
	{
		free( buffer );
		buflen = 0;
	}
	if( !buffer )
	{
		buffer = malloc( len + 1 );
		buflen = len + 1;	
	}
        strncpy( buffer, str, len );
	buffer[ len ] = 0;
        return( buffer );  // The binding will dup from our buffer
}

int main( int argc, char *argv[] )
{
	nbCELL context;

	context = nbStart( argc, argv );

	TEST( "Testing a couple existing cell functions" );
        nbCmd( context, "define x cell `math.mod(a,7) + `math.sqrt(b);", NB_CMDOPT_ECHO );
        nbCmd( context, "define r1 on(x>=16);", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,b,x;", NB_CMDOPT_ECHO );
        nbCmd( context, "assert a=13,b=100;", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,b,x;", NB_CMDOPT_ECHO );
        nbCmd( context, "assert a=14;", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,b,x;", NB_CMDOPT_ECHO );

	TEST( "Testing a C library function that I register" );
        nbBindCellFunction( context, "my.mod", fmod, "nb.d(d,d)" );
	nbCmd( context, "assert y=`my.mod(b,7);", NB_CMDOPT_ECHO );
        nbCmd( context, "show b,y;", NB_CMDOPT_ECHO );

	TEST( "Testing a silly function of my creation: xyminus2(a,b) returns  (a * b - 2)" );
        nbBindCellFunction( context, "my.xyminus2", xyminus2, "nb.d(d,d)" );
	nbCmd( context, "assert y=`my.xyminus2(a,b);", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,b,y;", NB_CMDOPT_ECHO );
         
	TEST( "Testing a silly single operand function: xtwice(a) returns (a * 2)" );
        nbBindCellFunction( context, "my.xtwice", xtwice, "nb.d(d)" );
	nbCmd( context, "assert y=`my.xtwice(a);", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,y;", NB_CMDOPT_ECHO );
         
	TEST( "Testing a string truncation function: str.trunc(s,n) returns up to n characters of s" );
        nbBindCellFunction( context, "str.trunc", strTruncate, "nb.s(s,i)" );
	nbCmd( context, "assert a=\"01234567890123456789\";", NB_CMDOPT_ECHO );
	nbCmd( context, "assert y=`str.trunc(a,10);", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,y;", NB_CMDOPT_ECHO );
         
	return ( nbStop( context ) );
}
