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
* File:     lib/test/eSkillMethods.c 
*
* Title:    API Test - Register and exercise skill methods
*
* Category: Example - Short illustrations using sparse C code formatting.
*
* Function:
*
*   This file exercises NodeBrain API functions that register skill methods
*   for an application provided skill, creating a new type of node.
*   
* See NodeBrain Library manual for a description of each API function.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2014/11/16 Ed Trettevik - introduced in version 0.9.03
*=============================================================================
*/
#include <nb/nb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>

#define TEST(TITLE) nbLogPut( context, "\nTEST: line %5d - %s\n", __LINE__, TITLE );

void showValue( nbCELL context, char *identifier )
{
        nbCELL termCell;
        nbCELL valueCell;
        int cellType;

	nbLogFlush( context );		// flush log buffer out before writing to stderr
        nbLogPut( context, "showValue: %s.%s is ", nbNodeGetName( context ), identifier );
        termCell = nbTermLocate( context, identifier );
        if( termCell == NULL )
        {
                nbLogPut( context, "not defined\n" );
                return;
        }
        valueCell = nbCellGetValue( context, termCell );
        //nbCellDrop( context, termCell ); // must release cell obtained by nbTermLocate to avoid memory leak
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

static int fireAssert( nbCELL context, void *skillHandle, void *knowledgeHandle, nbCELL arglist, nbCELL value )
{
	nbSET argSet;
	nbCELL cell;
        int cellType;
        char *comma = " ";

	nbLogMsg( context, 0, 'T', "fireAssert handling assertion" );
	// MQTT call goes here.
	argSet = nbListOpen( context, arglist );
        nbLogPut( context, "%s(", nbNodeGetName( context ) );
	while ( ( cell = nbListGetCellValue( context, &argSet ) ) )			// if fire( 1, 2, 3 )=12, this get access to the 1,2,3 and 12 comes via value!
	{
		// If Fire() has no args this loop will never execute!
		nbLogPut( context, "%s", comma);
        	cellType=nbCellGetType( context, cell );
		if( cellType==NB_TYPE_REAL) 
        		nbLogPut( context, "%f ", nbCellGetReal( context, cell ) ); 
		else if( cellType==NB_TYPE_STRING ) 
        		nbLogPut( context, "\"%s\" ", nbCellGetString( context, cell ) );
		else nbLogPut( context, " ??? " );
		nbCellDrop( context, cell );  // must release cell when done with it to avoid memory leak
		// whatever you want to do
		comma = ", ";
	}
        nbLogPut( context, ") = ");
        cellType=nbCellGetType( context, value);
	if( cellType==NB_TYPE_REAL) 
        	nbLogPut( context, "%f\n", nbCellGetReal( context, value ) ); 
	else if( cellType==NB_TYPE_STRING ) 
        	nbLogPut( context, "\"%s\"\n", nbCellGetString( context, value ) );
	else nbLogPut( context, " ??? " );

	nbLogFlush( context );		// this does NOT force out the last :show
	return ( 0 ); // aways return zero
}

static int fireCommand( nbCELL context, void *skillHandle, void *knowledgeHandle, nbCELL arglist, char *text )
{
	nbSET argSet;
	nbCELL cell;
	nbCELL valueCell;
        int cellType;
        char *comma = " ";

	nbLogMsg( context, 0, 'T', "fireCommand handling command" );
	// MQTT call goes here.
	argSet = nbListOpen( context, arglist );
        nbLogPut( context, "%s(", nbNodeGetName( context ) );
	while ( ( cell = nbListGetCell( context, &argSet ) ) )			// if "fire( 1, 2, 3 ):xyz" command, this get access to the 1,2,3 and xyz is in text! 
	{
		// If Fire() has no args this loop will never execute!
		nbLogPut( context, "%s", comma );
        	cellType = nbCellGetType( context, cell );
		if( cellType == NB_TYPE_REAL ) 
        		nbLogPut( context, "%f ", nbCellGetReal( context, cell ) ); 
		else if( cellType == NB_TYPE_STRING ) 
			nbLogPut( context, "\"%s\" ", nbCellGetString( context, cell ) );
		else if( cellType == NB_TYPE_TERM ) 
			nbLogPut( context, "%s ", nbTermGetName( context, cell ) );
		else
		{
			valueCell = nbCellCompute( context, cell );
			cellType = nbCellGetType( context, valueCell );
			if( cellType == NB_TYPE_REAL )
				nbLogPut( context, "%f ", nbCellGetReal( context, valueCell ) );
			else if( cellType == NB_TYPE_STRING ) 
				nbLogPut( context, "\"%s\" ", nbCellGetString( context, valueCell ) );
			else nbLogPut( context, " ??? " );
 			nbCellDrop( context, valueCell );
		}
		nbCellDrop( context, cell );  // must release cell when done with it to avoid memory leak
		// whatever you want to do
		comma = ", ";
	}
        nbLogPut( context, ") : %s\n", text );
        showValue( context, "Foo.bar");  // display the value of an identifier
	return ( 0 );
}

static void *fireBind( nbCELL context, void *moduleHandle, nbCELL skill, nbCELL arglist, char *text )
{
	nbSkillSetMethod( context, skill, NB_NODE_ASSERT, fireAssert );		// connects fireAssert as what is executed when fire() is in the rule
	nbSkillSetMethod( context, skill, NB_NODE_COMMAND, fireCommand );		// connects fireAssert as what is executed when fire() is in the rule
	return ( NULL );
}

int main( int argc, char *argv[] )
{
	nbCELL context;

	context = nbStart( argc, argv );

	TEST("nbSkillDeclare - declare a new skill");
	nbSkillDeclare( context, fireBind, NULL, "", "fire", NULL, "" );	// "fire" is the skill name, which has a hashed association by fireBind 
	TEST("nbCmd - define a node that uses the skill");
	nbCmd( context, "define Fire node fire;", NB_CMDOPT_ECHO );		// creates a node named Fire with fire skill provided by nbSkillDeclare & fireBind

        // Illustrate how to get the value of a cell with known identifier
	nbCmd( context, "Fire. define Foo node;", NB_CMDOPT_ECHO );		// creates a node named Fire with fire skill provided by nbSkillDeclare & fireBind
	nbCmd( context, "Fire.Foo. define bar cell \"value of Fire.Foo.bar\";", NB_CMDOPT_ECHO );		// creates a node named Fire with fire skill provided by nbSkillDeclare & fireBind
        nbCmd( context, "Fire. define a cell 1;", NB_CMDOPT_ECHO );
        showValue( context, "Fire.Foo.bar");  // display the value of an identifier
        
	nbCmd( context, "define Ice node fire;", NB_CMDOPT_ECHO );		// creates a node named Ice with fire skill provided by nbSkillDeclare & fireBind

        // Illustrate how to get the value of a cell with known identifier
	nbCmd( context, "Ice. define Foo node;", NB_CMDOPT_ECHO );		// creates a node named Fire with fire skill provided by nbSkillDeclare & fireBind
	nbCmd( context, "Ice.Foo. define bar cell \"value of Ice.Foo.bar\";", NB_CMDOPT_ECHO );		// creates a node named Fire with fire skill provided by nbSkillDeclare & fireBind
        nbCmd( context, "Ice. define b cell 2;", NB_CMDOPT_ECHO );
        nbCmd( context, "Ice. define c cell 3;", NB_CMDOPT_ECHO );
        nbCmd( context, "Ice. define d_x cell 3;", NB_CMDOPT_ECHO );
        nbCmd( context, "Ice. define d_y cell 3;", NB_CMDOPT_ECHO );
        nbCmd( context, "Ice. define e cell 3;", NB_CMDOPT_ECHO );
        showValue( context, "Ice.Foo.bar");  // display the value of an identifier

	nbCmd( context, "define r1 on(y=4) Fire(22)=7,Ice(10)=\"abc\",Fire(\"abc\",\"def\")=1,x=1:show y, x, r1, Fire;", NB_CMDOPT_ECHO );
	nbCmd( context, "define r2 on(y=2) Ice(22,\"abc\")=99,x=2:show y, x, r2, Fire;", NB_CMDOPT_ECHO );
	nbCmd( context, "assert y=4;", NB_CMDOPT_ECHO );

	TEST("Expecting r2 to fire")
	nbCmd( context, "assert y=2;", NB_CMDOPT_ECHO );

	TEST("Expecting r1 to fire")
	nbCmd( context, "assert y=4;", NB_CMDOPT_ECHO );
	nbCmd( context, "Fire(x,y,\"abc\",x+y):this is command text to Fire node", NB_CMDOPT_ECHO );
	nbCmd( context, "Ice(y,x,3.9,4+7):this is command text to Ice node", NB_CMDOPT_ECHO );

	return ( nbStop( context ) );
}
