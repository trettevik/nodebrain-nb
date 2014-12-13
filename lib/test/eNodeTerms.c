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
* File:     lib/test/eNodeTerms.c 
*
* Title:    API Test - Access to terms and their values within nodes.
*
* Category: Example - Short illustrations using sparse C code formatting.
*
* Function:
*
*   This file exercises NodeBrain API functions that access a node's terms.
*
* See NodeBrain Library manual for a description of each API function.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2014-11-16 Ed Trettevik - introduced in version 0.9.03
*=============================================================================
*/
#include <nb/nb.h>

#define TEST( TITLE ) nbLogPut( context, "\nTEST: line %5d - %s\n", __LINE__, TITLE )

#define MAX_TERMS 256  // Maximum number of terms we expect in a node

/*
*  This function shows a node's state in a format provided by the nbNodeGetTermValueString function
*/
static void showNodeTerms1( nbCELL context )
{
        char buffer[4096];
        char *cursor=buffer;
        int  size;

        nbLogPut( context, "%s terms:\n", nbNodeGetName( context ) );
        size = nbNodeGetTermNameString( context, &cursor, sizeof(buffer) );
        if( size > 0 ) nbLogPut( context, "%s\n", buffer );
        else nbLogPut( context, "*** State string buffer for %s is %zu bytes and %zu are required\n", nbNodeGetName(context), sizeof(buffer), sizeof(buffer)+1-size );

}

static void showNodeTerms2( nbCELL context )
{
	nbCELL term[MAX_TERMS];
        int terms;
	int i;
        char name[1024];
        char *cursor;
        int size;

	nbLogPut( context, "%s terms:\n", nbNodeGetName( context ) );
	terms=nbNodeGetTermCellArray( context, term, MAX_TERMS );
        if( terms > MAX_TERMS )
	{
		terms = MAX_TERMS;
	}
        for( i=0; i < terms; i++)
	{
		cursor = name;
                size = nbCellGetName( context, term[i], &cursor, sizeof(name) );
		if( size > 0 ) nbLogPut( context, "  %s\n", name );
                else nbLogPut( context, "showNodeTerms: name too large for buffer\n" );
	}
        nbLogPut( context, "%d - terms\n", terms );
	if( terms >= MAX_TERMS )
		nbLogPut( context, "Warning: array too small to display all terms\n" );
	
}

/*
*  This function shows a node's state in a format provided by the nbNodeGetTermValueString function
*/
static void showNodeValues1( nbCELL context )
{
        char buffer[4096];
        char *cursor=buffer;
        int  size;

        nbLogPut( context, "%s values:\n", nbNodeGetName( context ) );
        size = nbNodeGetTermValueString( context, &cursor, sizeof(buffer) );
        if( size > 0 ) nbLogPut( context, "%s\n", buffer );
        else nbLogPut( context, "*** State string buffer for %s is %zu bytes and %zu are required\n", nbNodeGetName(context), sizeof(buffer), sizeof(buffer)+1-size ); 
        
}

/*
*  This function shows a node's state in a format of our own choice
*/
static void showNodeValues2( nbCELL context )
{
        int size=4096;
        char buffer[size];
        char *cursor=buffer;
        nbCELL cell[MAX_TERMS];
        int n,i;

        nbLogPut( context, "%s values:\n", nbNodeGetName( context ) );
	n = nbNodeGetTermCellArray( context, cell, MAX_TERMS );
	for( i = 0; i < n; i++ )
	{
		if( size>1 && i>0 ) strcpy(cursor,";"), cursor++,
		size--;
		size = nbCellGetName( context, cell[i], &cursor, size );
 		if( size>1 ) strcpy( cursor, ":" ), cursor++;
		size--;
		size = nbCellGetValueName( context, cell[i], &cursor, size );
	}
        if( size > 0 ) nbLogPut( context, "{%s}\n", buffer );
        else nbLogPut( context, "*** State string buffer for %s is %zu bytes and %zu are required\n", nbNodeGetName(context), sizeof(buffer), sizeof(buffer)+1-size ); 
}

/*
*  This function shows a node's state in a format provided by the nbNodeGetTermValueString function
*/
static void showNodeFormulas1( nbCELL context )
{
        char buffer[4096];
        char *cursor=buffer;
        int  size;

        nbLogPut( context, "%s formulas:\n", nbNodeGetName( context ) );
        size = nbNodeGetTermFormulaString( context, &cursor, sizeof(buffer) );
        if( size > 0 ) nbLogPut( context, "%s\n", buffer );
        else nbLogPut( context, "*** State string buffer for %s is %zu bytes and %zu are required\n", nbNodeGetName(context), sizeof(buffer), sizeof(buffer)+1-size );

}

/*
*  This function shows a node's term names and formulas in a format of our own choice
*/
static void showNodeFormulas2( nbCELL context )
{
        int size=4096;
        char buffer[size];
        char *cursor=buffer;
        nbCELL cell[MAX_TERMS];
        int n,i;
        nbCELL defCell;

        nbLogPut( context, "%s formulas:\n", nbNodeGetName( context ) );
        n = nbNodeGetTermCellArray( context, cell, MAX_TERMS );
        for( i = 0; i < n; i++ )
        {
                if( size>1 && i>0 ) strcpy(cursor,";"), cursor++,
                size--;
                size = nbCellGetName( context, cell[i], &cursor, size );
                if( size>1 ) strcpy( cursor, ":" ), cursor++;
                size--;
                defCell = nbTermGetDefinition( context, cell[i] );
                size = nbCellGetName( context, defCell, &cursor, size );
        }
        if( size > 0 ) nbLogPut( context, "{%s}\n", buffer );
        else nbLogPut( context, "*** Definitions buffer for %s is %zu bytes and %zu are required\n", nbNodeGetName(context), sizeof(buffer), sizeof(buffer)+1-size );
}



static void showTermValue1( nbCELL context, char *identifier )
{
        nbCELL termCell;
        char name[1024];
        char *cursor=name;
        int size=sizeof(name);

        nbLogPut( context, "At %s %s is ", nbNodeGetName( context ), identifier );
        termCell = nbTermLocate( context, identifier );
        if( termCell == NULL )
        {
                nbLogPut( context, "not defined\n" );
                return;
        }
	size = nbCellGetValueName( context, termCell, &cursor, size );
        if( size > 0 ) nbLogPut( context, "%s\n", name );
        else nbLogPut( context, " too long to fit in value name buffer\n" );
}

static void showTermValue2( nbCELL context, char *identifier )
{
        nbCELL termCell;
        nbCELL valueCell;
        int cellType;

        nbLogPut( context, "At %s %s is ", nbNodeGetName( context ), identifier );
        termCell = nbTermLocate( context, identifier );
        if( termCell == NULL )
        {
                nbLogPut( context, "not defined\n" );
                return;
        }
        valueCell = nbCellGetValue( context, termCell );
        if( valueCell == NULL )
        {
                nbLogPut( context, "defined but value not returned\n" );
                return;
        }
        cellType=nbCellGetType( context, valueCell );
        if( cellType == NB_TYPE_REAL )
                nbLogPut( context, "number %f\n", nbCellGetReal( context, valueCell ) );
        else if( cellType == NB_TYPE_STRING )
                nbLogPut( context, "string \"%s\"\n", nbCellGetString( context, valueCell ) );
        else nbLogPut( context, " of an unrecognized type\n" );
}

int main( int argc, char *argv[] )
{
	nbCELL context,testContext;
        nbCELL defCell;
 	int cellType;

	context = nbStart( argc, argv );

	TEST( "Creating a term at the top level context" );
        nbCmd( context, "define f_a cell 20;", NB_CMDOPT_ECHO );

	TEST( "Creating Test node to test ability to access the subordinate terms" );
        nbCmd( context, "define Test node;",  NB_CMDOPT_ECHO );
        nbCmd( context, "Test. assert .a=1,.b=\"abc\",!.c,?.d,.e,.f_a=1,.f_b=\"abc\",f_c==a and b;", NB_CMDOPT_ECHO );
	nbCmd( context, "Test. define r1 on(a or b) x=2;", NB_CMDOPT_ECHO );
        nbCmd( context, "Test. define foobar node;",  NB_CMDOPT_ECHO );

	TEST( "Locating the Test node" );
        testContext = nbTermLocate( context, "Test" );
        if( !testContext )
        {
        	nbLogPut( context, "*** Identifier 'Test' not found\n" );
        	return(1);
        }

        defCell = nbTermGetDefinition( context, testContext );
        cellType = nbCellGetType( context, defCell );
	if( cellType != NB_TYPE_NODE )
	{
        	nbLogPut( context, "*** Identifier 'Test' not defined as node\n" );
        	return(1);
	}
	TEST( "Accessing terms from the top level context" );
        showTermValue1( context, "f_a" ); 
        showTermValue1( context, "Test.f_a" ); 
        showTermValue1( context, "Test.f_b" ); 

	TEST( "Accessing the same terms from the Test node context" );
        showTermValue1( testContext, "..f_a" ); 
        showTermValue1( testContext, "f_a" ); 
        showTermValue1( testContext, ".f_b" ); 

	TEST( "Accessing the same terms a harder way with access to cell type codes and C data types" );
        showTermValue2( testContext, "..f_a" ); 
        showTermValue2( testContext, "f_a" ); 
        showTermValue2( testContext, ".f_b" ); 

	TEST( "Displaying Test node term names the easy way as single string" );
        showNodeTerms1( testContext );

	TEST( "Displaying Test node terms the hard way using an array of cells" );
        showNodeTerms2( testContext );

	TEST( "Displaying Test node values the easy way" );
        showNodeValues1( testContext );

	TEST( "Displaying Test node values a harder way with more control over format" );
        showNodeValues2( testContext );

	TEST( "Displaying Test node term formulas the easy way" );
        showNodeFormulas1( testContext );

	TEST( "Displaying Test node term formulas a slightly harder way with more format control" );
        showNodeFormulas2( testContext );

	return ( nbStop( context ) );
}
