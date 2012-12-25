/*
* Copyright (C) 1998-2012 The Boeing Company
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
* File:     nbspawn.c 
*
* Title:    Routines that Spawn Child Processes 
*
* Function:
*
*   This file provides routines to support NodeBrain commands that spawn
*   new processes for skulls and shell commands.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void nbSpawnChild(nbCELL context,int options,char *command)
*   void nbSpawnSkull(nbCELL context,char *command)
* 
*
* Description
*
*   All we are doing here is starting new processes or threads to issues
*   commands to a host shell or NodeBrain skull.  A skull is just another
*   copy of the NodeBrain interpreter.
*
*   Because we are not currently using a UNIX development tool on Windows,
*   we have separate code to implement these functions on Windows.
*
*     nbSpawnChild() - Invoke child process using the Medulla API.
*     nbSpawnSkull() - Create a new NodeBrain process and direct output
*                      according to the "set out" variable.
*
*   The interpreter passes any command prefixed by "-" or "=" to the 
*   nbSpawnChild() function.  This function leaves most of the work to the
*   Medulla.
*
*   The nbSpawnSkull() function is invoked by a server node brain to service
*   requests from a client such as an encrypted file transfer or a proxied
*   client connection.                      
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002/10/06 Ed Trettevik (split out from nb.c in 0.4.1)
* 2002/11/08 eat - version 0.4.1 A9
*             1) fiddled for HP3000
* 2002/12/06 eat - version 0.4.3 B3
*             1) Corrected SIGCLD in spawnSkull
*
* 2003/03/03 eat 0.5.1  Conditional compile for Mac OS X [ see defined(MACOS) ]
* 2003/03/15 eat 0.5.1  Inserted includes so we can use make file
* 2003/05/22 eat 0.5.3  Added calls to closeListeners()
* 2003/05/22 eat 0.5.3  Dropped references to obselete server_socket variable
* 2003/05/22 eat 0.5.3  Modified calls to CreateProcess to not inherit handles
* 2003/07/13 eat 0.5.4  Modified calls to CreateProcess to inherit handles 
* 2003/07/19 eat 0.5.4  Server sockets are made uninheritable by chlisten().
* 2004/10/04 eat 0.6.1  Conditionals for FreeBSD, OpenBSD, and NetBSD
* 2005/10/11 eat 0.6.4  Modified spawnSystem() to support "-<" command;
*            When the first character of the command passed to spawnSystem is
*            "<", the output of the system command is interpreted as NodeBrain
*            commands.
* 2005/12/12 eat 0.6.4  Changed handling of SIGCHLD
* 2005/12/31 eat 0.6.4  spawnExec is now called with = unconsumed
* 2006/03/26 eat 0.6.4  spawnExec and spawnSystem merged into nbSpawnChild
* 2006/03/26 eat 0.6.4  spawnSkull replaced with nbSpawnSkull
* 2006/04/12 eat 0.6.4  Modified nbCmdMsgReader and nbLogMsgReader to be quiet on NB_CMDOPT_HUSH
* 2007/12/26 eat 0.6.8  Added null closer to nbMedullaProcessOpen call
* 2008/03/27 eat 0.7.0  Removed old spawnSkull function no longer used
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2012/01/16 dtl 0.8.5  Checker updates.
* 2012-10-18 eat 0.8.12 Replaced rand with random
* 2012-10-19 eat 0.8.12 Replaced random with pid plus counter since we needed unique instead of random
*=============================================================================
*/
#include <nb/nbi.h>

extern char mypath[];      /* path to executing program */

// Read commands from child
int nbCmdMsgReader(nbPROCESS process,int pid,void *session,char *msg){
  nbCELL context=(nbCELL)session;
  if(!(((NB_Node *)((NB_Term *)context)->def)->cmdopt&NB_CMDOPT_HUSH)){
    if(process->status&NB_MEDULLA_PROCESS_STATUS_BLOCKING) outPut("[%d: %s\n",process->pid,msg);
    else outMsg(0,'I',"[%d: %s",process->pid,msg);
    }
  nbCmd(context,msg,0);
  return(0);
  }
// Read and log messages from child
int nbLogMsgReader(nbPROCESS process,int pid,void *session,char *msg){
  nbCELL context=(nbCELL)session;
  if(!(((NB_Node *)((NB_Term *)context)->def)->cmdopt&NB_CMDOPT_HUSH)){
    if(process->status&NB_MEDULLA_PROCESS_STATUS_BLOCKING) outPut("[%d| %s\n",process->pid,msg);
    else outMsg(0,'I',"[%d| %s",process->pid,msg);
    }
  return(0);
  }

//

//#if defined(WIN32)
//HANDLE nbpTimer=NULL;      /* NBP timeout timer handle */
//LARGE_INTEGER alarmTime;   /* timer intervals - SetWaitableTimer */
//                           /*    1 sec = -10000000 (7 zeros) */
//#endif

int nbSpawnChild(nbCELL context,int options,char *cursor){
  char outname[1024],msgbuf[NB_MSGSIZE];
  nbPROCESS process;
  static unsigned short childwrap=0;

  if(!(clientIdentity->authority&AUTH_SYSTEM)){
    outMsg(0,'E',"Identity \"%s\" does not have system authority.",clientIdentity->name->value);
    return(0);
    }

  // add code to check command against the grant and deny commands specified for the user 
  // perhaps that should actually be done within the medulla after parsing the command
  // or perhaps it should be done at the command intepreter to cover all commands
  // We have to decide if we want special controls on the system commands
  
  childwrap=(childwrap+1)%1000; 
  sprintf(outname,"%sservant.%.10u.%.5u.%.3u.out",outDirName(NULL),(unsigned int)time(NULL),getpid(),childwrap);
  process=nbMedullaProcessOpen(options,cursor,outname,(NB_Term *)context,NULL,NULL,nbCmdMsgReader,nbLogMsgReader,msgbuf);
  if(process==NULL){
    outMsg(0,'E',"%s",msgbuf);
    return(0);
    }
  else if(process->status&NB_MEDULLA_PROCESS_STATUS_BLOCKING){
    if(process->status&NB_MEDULLA_PROCESS_STATUS_GENFILE)
      outPut("[%d] Started: %c=\"%s\" %s%s\n",nb_mode_check ? 0:process->pid,'%',outname,process->prefix,process->cmd);
    else outPut("[%d] Started: %s%s\n",nb_mode_check ? 0:process->pid,process->prefix,process->cmd);
    nbMedullaProcessReadBlocking(process);  // read stdin and stdout using blocking IO
    nbMedullaProcessWait(process);  // wait for the process to end
    }
  else{
    if(process->status&NB_MEDULLA_PROCESS_STATUS_GENFILE)
      outMsg(0,'I',"[%d] Started: %c=\"%s\" %s%s",nb_mode_check ? 0:process->pid,'%',outname,process->prefix,process->cmd);
    else outMsg(0,'I',"[%d] Started: %s%s",nb_mode_check ? 0:process->pid,process->prefix,process->cmd);
    }
  return(process->pid);
  }


// Spawn a process executing a copy of the current program
//   cursor points to a single parameter string which we will
//   quote after escaping any internal quotes
int nbSpawnSkull(nbCELL context,char *oar,char *cursor){
  char command[NB_BUFSIZE],*curcmd;
  char filename[514];
  time_t systemTime;
  static int count=0;
  int n;

  time(&systemTime);
  count++;
  sprintf(filename,"%sskull.%.10u.%.3u.txt",outDirName(NULL),(unsigned int)systemTime,count%1000);
  if(oar!=NULL && *oar) sprintf(command,"=>\"%s\" @\"%s\" \"%s\" ",filename,mypath,oar);
  else sprintf(command,"=>\"%s\" @\"%s\" ",filename,mypath);
  if(*cursor!=0){
    strcat(command,"\"");
    if(strchr(cursor,'"')==NULL){if(((n=strlen(cursor))+strlen(command))<NB_BUFSIZE) strncat(command,cursor,n);} //2012-01-16 dtl used strncat
    else{
      curcmd=command+strlen(command);
      while(*cursor!=0){
        if(*cursor=='"'){
          *curcmd='\\';
          curcmd++;
          }
        *curcmd=*cursor;
        curcmd++;
        cursor++;
        }
      *curcmd=0;
      }
    strcat(command,"\"");
    }
  return(nbSpawnChild(context,NB_CHILD_NOCLOSE,command));
  }
