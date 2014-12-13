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
* File:     lib/test/eSynapse.c 
*
* Title:    API Test - Use of synapse to respond to cell changes or timer.
*
* Category: Example - Short illustrations using sparse C code formatting.
*
* Function:
*
*   This file exercises NodeBrain API synapse functions.
*
* See NodeBrain Library manual for a description of each API function.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2014-12-12 Ed Trettevik - introduced in version 0.9.03
*=============================================================================
*/
#include <nb/nb.h>

#define TEST( TITLE ) nbLogPut( context, "\nTEST: line %5d - %s\n", __LINE__, TITLE )

// Show the value of a term

static void showTermValue( nbCELL context, char *identifier )
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

// Respond to a change in a subordinate cell

static void myAlert( nbCELL context, void *skillHandle, void *nodeHandle, nbCELL cell )
{
        char name[1024];
        char *cursor=name;
        int size;

        nbLogMsg( context, 0, 'T', "myAlert was called");
        size = nbCellGetName( context, cell, &cursor, sizeof(name) );
        if( size > 0 ) nbLogPut( context, " Cell: %s\n ", name );
        else nbLogMsg( context, 0, 'E', "myAlert: name too large for buffer" );
	cursor=name;
        size = nbCellGetValueName( context, cell, &cursor, size );
        if( size > 0 ) nbLogPut( context, "Value: %s\n", name );
        else nbLogMsg( context, 0, 'E', "myAlert: value too large for buffer" );
}

// Respond to a timer 

static void myAlarm( nbCELL context, void *skillHandle, void *nodeHandle, nbCELL cell )
{
	nbLogMsg( context, 0, 'T', "myAlarm was called");
}

// Main routine

int main( int argc, char *argv[] )
{
	nbCELL context;
        nbCELL xCell;
	nbCELL synapseAlertCell;
	nbCELL synapseAlarmCell;
	int i;

	context = nbStart( argc, argv );

	TEST( "Testing a synapse alert - response to cell change" );

        nbCmd( context, "define x cell a + b;", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,b,x;", NB_CMDOPT_ECHO );
        nbCmd( context, "assert a=13,b=100;", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,b,x;", NB_CMDOPT_ECHO );

        showTermValue( context, "x" );
        xCell = nbTermLocate( context, "x" );
        if( xCell == NULL )
        {
                nbLogMsg( context, 0, 'E', "Unable to local x cell" );
                return( 1 );
        }

        synapseAlertCell = nbSynapseOpen( context, NULL, NULL, xCell, myAlert );

        nbCmd( context, "show a,b,x;", NB_CMDOPT_ECHO );
        nbCmd( context, "assert a=14;", NB_CMDOPT_ECHO );
        nbCmd( context, "show a,b,x;", NB_CMDOPT_ECHO );

	TEST( "Testing synapse alert on time condition along with a synapse alarm" );

        nbCmd( context, "assert x==~(4s);", NB_CMDOPT_ECHO );

        synapseAlarmCell = nbSynapseOpen( context, NULL, NULL, NULL, myAlarm );
        nbSynapseSetTimer( context, synapseAlarmCell, 3 );

	// because we have not called nbServe yet, we need to tell the rule engine about advancing time

	for(i=0;i<20;i++)
	{
		sleep(1);
		nbClockAlert();  // WARNING: This function is deprecated and will be replaced in 0.9.04
	}         

	TEST( "Testing a synapse alert and alarm with the rule engine in control of time" );

	nbCmd( context, "define EndIt when(~(10s)):stop;", NB_CMDOPT_ECHO );

        nbSynapseSetTimer( context, synapseAlarmCell, 5 );

        char *argvServe[2]={"eSynapse","-s"};

        nbServe( context, 2, argvServe );

        synapseAlertCell = nbSynapseClose( context, synapseAlertCell );  // release the synapse
        synapseAlarmCell = nbSynapseClose( context, synapseAlarmCell );  // release the synapse

	return ( nbStop( context ) );
}
