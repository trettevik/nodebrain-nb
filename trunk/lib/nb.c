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
* File:     nb.c 
*
* Title:    NodeBrain Agent, Client, and Utility
*
* Function:
*
*   This progarm is a command line user interface to the NodeBrain Library,
*   which provides an interpreter for a small declarative rule-based language
*   designed for the construction of state and event monitoring applications.
*   This program may run as a daemon, and interactive line mode client, or
*   a batch utility.  It depends entirely on the NodeBrain Library to provide
*   functionality.   
*
* References:
*
*   1) See http://www.nodebrain.org for official NodeBrain documentation.
*   2) See the README file for information on this release.
*
* Description:
*
*   This file uses the NodeBrain C API supported by the NodeBrain library
*   (libnb.so) to make the interpreter available as a program.  All NodeBrain
*   functionality is provided by the library.  This program also serves as
*   an example for anyone wanting to include NodeBrain in their own program.
*  
*   How do I extend NodeBrain?
*
*   The NodeBrain Library provides a C API for use by plug-ins called 
*   "node modules" that add functionality.  See the NodeBrain Library manual
*   for more information on writing node modules.  
*
*   If you are imbedding NodeBrain in your own program, you can still use
*   dynamic node modules, but also have the option of including node "skill"
*   functions statically in your program.
*   
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2003/03/17 eat 0.5.2  New main routine as simple API example
* 2005/04/09 eat 0.6.2  modified for enhanced API
* 2005/04/09 eat 0.6.2  Renamed from nbmain.c to nb.c (the original name)
* 2005/04/09 eat 0.6.2  Moved Windows service fussing here from nbStart() 
*                       This is in preparation for allowing multiple calls
*                       to nbStart() within a program.  There should be only
*                       call to setup the Windows service environment, so this
*                       is the logical place for it.
* 2005/04/09 eat 0.6.2  Moved factory "+:" command interpreter to nbplus.c
*                       This provides cleaner modularization for someone
*                       wanting to experiment with providing extended commands
*                       through this mechanism.
* 2005/12/05 eat 0.6.4  Modified signal handling to exit when not agent
* 2006/01/06 eat 0.6.4  Moved signal handling into nbstem.c
* 2013-02-03 eat 0.8.13 Included support for nbkit command
*=============================================================================
*/
#include <nb/nb.h>

int main(int argc,char *argv[]){
#if defined(WIN32)                     /* enable running as Windows service */ 
  int nbMain(int argc,char *argv[]);   /* define service start function */
  return(nbService(nbMain,argc,argv)); /* schedule service start if running as service */
  }                                    /* otherwise, just call it as alternate main */
int nbMain(int argc,char *argv[]){     /* You may omit this conditional block if you */
#endif                                 /* don't need a Windows service */

  nbCELL context=NULL;

/*
*  See if we should operate as nbkit instead of nb. This
*  provides just enough support for nbkit functionality
*  to avoid having to install the old nbkit perl script.
*/
  if(argc>0 && strlen(argv[0])>0){
    char *name=argv[0];
    char *basename=name+strlen(name)-1;
    while(basename>name && *basename!='/') basename--;
    if(*basename=='/') basename++;
    if(strcmp(basename,"nbkit")==0){
      return(nbKit(argc,argv));       // 2013-02-03 eat - CID 971428 Intentional - argc and argv tainted
      }
    }
/*
*  You might process the parameters here if this where
*  a custom program using the NodeBrain library.  Then
*  you could start NodeBrain with arguments you build
*  yourself.
*/
  context=nbStart(argc,argv);          /* Start up NodeBrain and process arguments */
  if(context==NULL) return(1);         /* Bail out if unsuccessful */

  nbServe(context,argc,argv);          // process arguments and enter final mode
/*
*  If you were writing a new program using the NodeBrain
*  library (libnb.a) this is where you might include other
*  API function calls.  You would use the nbStart() arguments
*  to initialize the NodeBrain environment and then pass
*  it commands based on the requirements of your application.
*  In addition to passing it commands, you could call other API
*  functions to interface with the NodeBrain environment.
*/
/* Your API calls go here like this, only mixed in with your application logic
  nbCmd(context,"assert a==x+y;",1);
  nbLogMsg(context,2,'W',"This is a sample message");
  nbCmd(context,"assert x=2,y=3;",1);
  nbCmd(context,"show -cells",1);
*/

  return(nbStop(context));             /* Stop NodeBrain and use the return code */
  }
