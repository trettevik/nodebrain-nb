/*
* Copyright (C) 1998-2010 The Boeing Company
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
* File:     nbservice.c 
*
* Title:    Service Interface
*
* Function:
*
*   This file provides code enabling NodeBrain to run as a Unix daemon or
*   Windows service.  For Windows, routines are included to install,
*   uninstall, start and stop a service.
*
* Synopsis:
*
*   #include "nb.h"
*
*   int  nbwService(int argc, char *argv[]);
*   void nbwCommand(nbCELL context,char *cmdverb,char *command);
*   BOOL nbwStartService(char *name,int argc,char * argv[]);
*   BOOL nbwStopService(char *name);
*   BOOL nbwCreateService(char *name,char *title);
*   BOOL nbwDeleteService(char *name);
*
* Syntax:
*
*   NodeBrain commands for invoking the windows service routines are
*   supported by nbwCommand() which is called to interpret commands
*   in the "windows" context.  (The windows context is currently
*   implemented as a verb, so you can not address windows using the
*   "address" verb.) 
*
*     @> windows <command>
*
*   The commands supported by nbwCommand() are shown here.
*
*     createService <name>
*     deleteService <name>
*     startService  <name>
*     stopService   <name>
*
* Description:
*
*   When NodeBrain starts on Windows, it first checks to see if it
*   should run as a Windows service.  It determines this by the name
*   you have assigned to the NodeBrain interpreter.  The standard name
*   for NodeBrain on windows is "nb.exe".  To run it as a service, you
*   must create a copy of nb.exe and store it under the name of your
*   service.
*
*      <name>.exe
*
*   The start-up NodeBrain configuration file must be placed in the same 
*   directory, again using the service name.
*
*      <name>.nb
*
*   When installing a service using the "windows createService" command,
*   you must provide a service.ini file in the same directory as nb.exe.
*   The syntax is shown below.  Provide a section for each service.  The
*   title and description provide the values you want displayed in the
*   Windows services GUI.
*
*      [name]
*      Title=<title>
*      Description=<description>
*
* Future:
*
*   This code should be cleaned up, as it was created using cut and paste
*   from sample code.
*
* Credits:
*
*   This code was derived from XYNTService by Xiangyang Liu as
*   posted in an article on www.codeproject.com
*
*   Equivalent code is available in Microsoft sample code.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2002-10-08 Ed Trettevik (introduced in 0.4.1 A6)
* 2002-10-18 eat - version 0.4.1 A7
*             1) Cleaned up the windows command syntax and included the
*                service.ini file to specify service attributes.
* 2003-03-16 eat 0.5.2  Moved daemonize() here and moved alternate main() out.
* 2004-10-06 eat 0.6.1  Conditionals for FreeBSD, OpenBSD, and NetBSD
* 2008-11-11 eat 0.7.3  Change failure exit code to NB_EXITCODE_FAIL
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#include "nbi.h"

// included for creating dll
#if defined(_WINDOWS)
BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved){
	switch(fdwReason){
	  case DLL_PROCESS_ATTACH: break;
	  case DLL_THREAD_ATTACH: break;
	  case DLL_THREAD_DETACH: break;
	  case DLL_PROCESS_DETACH: break;
	}
  return(TRUE);
  }
#endif

extern int agent;
extern char *myname;

#if defined(WIN32)

#define strSize 512

int (*serviceStart)(int argc,char *argv[]);
int serviceArgc;
char **serviceArgv;

PROCESS_INFORMATION* pProcInfo = 0;

SERVICE_STATUS          serviceStatus; 
SERVICE_STATUS_HANDLE   hServiceStatusHandle; 

VOID WINAPI nbwServiceMain(DWORD dwArgc,LPTSTR *lpszArgv);
VOID WINAPI nbwServiceEventHandler(DWORD fdwControl);


BOOL nbwStopService(char* pName){ 
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	if(schSCManager==0) outMsg(0,'E',"OpenSCManager failed, error code = %X", GetLastError());
	else{
		SC_HANDLE schService = OpenService( schSCManager, pName, SERVICE_ALL_ACCESS);
		if(schService==0) outMsg(0,'E',"OpenService failed, error code = %X", GetLastError()); 
		else{
			SERVICE_STATUS status;
			if(ControlService(schService,SERVICE_CONTROL_STOP,&status)){
				CloseServiceHandle(schService); 
				CloseServiceHandle(schSCManager); 
				return TRUE;
				}
			else outMsg(0,'E',"ControlService failed, error code = %X",GetLastError()); 
			CloseServiceHandle(schService); 
			}
		CloseServiceHandle(schSCManager); 
		}
	return FALSE;
	}

BOOL nbwStartService(char* pName, int nArg, char** pArg){  
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	if (schSCManager==0) outMsg(0,'E',"OpenSCManager failed, error code = %X",GetLastError());
	else{
		SC_HANDLE schService = OpenService( schSCManager, pName, SERVICE_ALL_ACCESS);
		if (schService==0) outMsg(0,'E',"OpenService failed, error code = %X",GetLastError()); 
		else{
			if(StartService(schService,nArg,(const char**)pArg)){
				CloseServiceHandle(schService); 
				CloseServiceHandle(schSCManager); 
				return TRUE;
				}
			else outMsg(0,'E',"StartService failed, error code = %X", GetLastError()); 
			CloseServiceHandle(schService); 
			}
		CloseServiceHandle(schSCManager); 
		}
	return FALSE;
	}

void nbwServiceStopped(void){
	serviceStatus.dwWin32ExitCode = 0; 
	serviceStatus.dwCurrentState  = SERVICE_STOPPED;
	serviceStatus.dwCheckPoint    = 0; 
	serviceStatus.dwWaitHint      = 0;		
  if(!SetServiceStatus(hServiceStatusHandle,&serviceStatus)){
    fprintf(stderr,"SetServiceStatus failed, error code = %X",GetLastError());
    fflush(stderr);
    }   
  }

/*
*  Service Startup
*/

VOID WINAPI nbwServiceStart(DWORD argc,LPTSTR *argv){
	DWORD   status = 0;
  DWORD   specificError = 0xfffffff;
	char startupLog[strSize];
  char outdir[512],logname[512];

	nb_Service=1;
  nb_opt_daemon=1;  // Running as a service implies -d (daemon) option
	SetCurrentDirectory(windowsPath);

  // allocate a console because we don't get one as a service
  // this is necessary so the Medualla can manage child processes as console groups
  AllocConsole(); 

  sprintf(startupLog,"%s\\Service\\%s\\%s.out",windowsPath,serviceName,serviceName);

  freopen("nul","r",stdin);   // may not need to do this
  SetHandleInformation((HANDLE)_get_osfhandle(_fileno(stdin)),HANDLE_FLAG_INHERIT,0);
  freopen("null","w",stdout);  // may not need to do this
  SetHandleInformation((HANDLE)_get_osfhandle(_fileno(stdout)),HANDLE_FLAG_INHERIT,0);
  freopen(startupLog,"w",stderr);
  SetHandleInformation((HANDLE)_get_osfhandle(_fileno(stderr)),HANDLE_FLAG_INHERIT,0);

  // provide default log file and output directory
  outInit();
  sprintf(logname,"%s\\Service\\%s\\log\\%s.log",windowsPath,serviceName,serviceName);
  outLogName(logname);
  sprintf(outdir,"%s\\/Service\\%s\\out\\",windowsPath,serviceName);
  outDirName(outdir);

  outStream(0,&stdPrint);
  outStream(1,&logPrint);
//  outStream(2,&clientPrint);
	
  serviceStatus.dwServiceType        = SERVICE_WIN32; 
  serviceStatus.dwCurrentState       = SERVICE_START_PENDING; 
  serviceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE; 
  serviceStatus.dwWin32ExitCode      = 0; 
  serviceStatus.dwServiceSpecificExitCode = 0; 
  serviceStatus.dwCheckPoint         = 0; 
  serviceStatus.dwWaitHint           = 0; 

  hServiceStatusHandle = RegisterServiceCtrlHandler(serviceName,nbwServiceEventHandler); 
  if(hServiceStatusHandle==0){ 
    outMsg(0,'E',"RegisterServiceCtrlHandler failed, error code = %X",GetLastError()); 
    return; 
    } 
    
  // Handle error condition 
  status = GetLastError();
  status=NO_ERROR; /* debug */
  if (status!=NO_ERROR){
    outMsg(0,'T',"nbwServiceMain() bailing out with error 1");
    serviceStatus.dwCurrentState       = SERVICE_STOPPED; 
    serviceStatus.dwCheckPoint         = 0; 
    serviceStatus.dwWaitHint           = 0; 
    serviceStatus.dwWin32ExitCode      = status; 
    serviceStatus.dwServiceSpecificExitCode = specificError; 
    SetServiceStatus(hServiceStatusHandle, &serviceStatus);
    outMsg(0,'T',"nbwServiceMain() bailing out with error 2");
    return; 
    } 
  // Initialization complete - report running status 
  serviceStatus.dwCurrentState       = SERVICE_RUNNING; 
  serviceStatus.dwCheckPoint         = 0; 
  serviceStatus.dwWaitHint           = 0;  
  if(!SetServiceStatus(hServiceStatusHandle, &serviceStatus))
    outMsg(0,'E',"SetServiceStatus failed, error code = %X",GetLastError()); 

  // This should be the first atexit and the last called
  //atexit(nbwServiceStopped);

  if(argc>1) (*serviceStart)(argc,argv);
  else (*serviceStart)(serviceArgc,serviceArgv);
  }

/*
*  Service Event Handler - start/stop
*/

VOID WINAPI nbwServiceEventHandler(DWORD fdwControl){
  //outMsg(0,'T',"Service handler called - control=%d",fdwControl);
	switch(fdwControl){ 
		case SERVICE_CONTROL_PAUSE:
			serviceStatus.dwCurrentState = SERVICE_PAUSED; 
			break;
		case SERVICE_CONTROL_CONTINUE:
			serviceStatus.dwCurrentState = SERVICE_RUNNING; 
			break;
		case SERVICE_CONTROL_STOP:
      outMsg(0,'T',"Service control stop request");
      nb_medulla->serving=0;   // ask Medulla to stop
			serviceStatus.dwCurrentState  = SERVICE_STOP_PENDING; 
      break;
    case SERVICE_CONTROL_SHUTDOWN:
      // We did not register to receive this control
      break;
		case SERVICE_CONTROL_INTERROGATE:
      if(nb_ServiceStopped){
		  	serviceStatus.dwWin32ExitCode = 0; 
			  serviceStatus.dwCurrentState  = SERVICE_STOPPED; 
			  serviceStatus.dwCheckPoint    = 0; 
		  	serviceStatus.dwWaitHint      = 0;		
			  if (!SetServiceStatus(hServiceStatusHandle,&serviceStatus))
			  	outMsg(0,'E',"SetServiceStatus failed, error code = %X",GetLastError());
			  return;
        } 
			break;
		default: 
			outMsg(0,'E',"Unrecognized service control code %X",fdwControl); 
		};
  if(!SetServiceStatus(hServiceStatusHandle,&serviceStatus))
    outMsg(0,'E',"SetServiceStatus failed, error code = %X",GetLastError()); 
	}

/*
*  Delete Service
*/
VOID nbwDeleteService(char* service){
	SC_HANDLE schSCManager=OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS); 
	if (schSCManager==0) outMsg(0,'E',"OpenSCManager failed, error code = %X",GetLastError());
	else{
		SC_HANDLE schService=OpenService(schSCManager,service,SERVICE_ALL_ACCESS);
		if (schService==0) outMsg(0,'E',"OpenService failed, error code=%X",GetLastError()); 
		else{
			if(!DeleteService(schService)) outMsg(0,'E',"Unable to delete %s service",service); 
			else outMsg(0,'I',"Service %s deleted",service); 
			CloseServiceHandle(schService); 
			}
		CloseServiceHandle(schSCManager);	
		}
	}

/*
*  Create Service
*/
void nbwCreateService(char* service){
    SERVICE_DESCRIPTION serviceDescription;
	SC_HANDLE schSCManager;
	char iniFileName[strSize];
	char title[strSize];
	char description[strSize];
	char executable[strSize];
	char command[strSize];
	if(strcmp(service,"nodebrain")==0){
	  strcpy(command,windowsPath);
	  sprintf(command,"%s\\nb.exe service=%s service\\%s\\%s.nb",windowsPath,service,service,service);
	  strcpy(title,"NodeBrain Agent");
	  strcpy(description,"Provides event correlation and state monitoring");
	  }
	else{
	  strcpy(executable,windowsPath);
	  strcat(executable,"\\");
	  strcat(executable,service);
	  strcpy(iniFileName,windowsPath);
	  strcat(iniFileName,"\\service.ini");
	  GetPrivateProfileString(service,"Command",executable,command,strSize,iniFileName);
	  GetPrivateProfileString(service,"Title",service,title,strSize,iniFileName);
	  GetPrivateProfileString(service,"Description","?",description,strSize,iniFileName);
	  }
	if((schSCManager=OpenSCManager(NULL,NULL,SC_MANAGER_CREATE_SERVICE))==0) 
	  outMsg(0,'E',"OpenSCManager failed, error code = %X",GetLastError());
	else{
	  SC_HANDLE schService = CreateService(
			schSCManager,	/* SCManager database      */ 
			service,		/* name of service         */ 
			title,	        /* service name to display */ 
			SERVICE_ALL_ACCESS,        /* desired access          */ 
			SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS , /* service type            */ 
			SERVICE_AUTO_START,      /* start type              */ 
			SERVICE_ERROR_NORMAL,      /* error control type      */ 
			command,	               /* service's binary        */ 
			NULL,                      /* no load ordering group  */ 
			NULL,                      /* no tag identifier       */ 
			NULL,                      /* no dependencies         */ 
			NULL,                      /* LocalSystem account     */ 
			NULL
		);                     /* no password             */ 
		if(schService==0) outMsg(0,'E',"Unable to create %s service",service); 
		else{
			serviceDescription.lpDescription=description;
			ChangeServiceConfig2(schService,SERVICE_CONFIG_DESCRIPTION,&serviceDescription);
			outMsg(0,'I',"Service %s created",service);
			CloseServiceHandle(schService); 
			}
		CloseServiceHandle(schSCManager);
		}	
	}

/* 
*  Windows Specific Command Processor
*/
void nbwCommand(nbCELL context,char *cmdverb,char *cursor){
  char symid,verb[strSize],service[strSize];

  symid=nbParseSymbol(verb,&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expected verb not found");
    return;
    }
  if(strcmp(verb,"showenv")==0){  /* show some things about the environment */
    TCHAR  buf[MAX_PATH];
    DWORD  bufCharCount,bufsize=1024;
 
    // Get and display the name of the computer. 
    bufCharCount=bufsize;
    if(!GetComputerName(buf,&bufCharCount)) strcpy(buf,"??");
    outPut("Computer name:      %s\n",buf); 
 
    // Get and display the user name. 
    bufCharCount=bufsize;
    if(!GetUserName(buf,&bufCharCount)) strcpy(buf,"??");
    outPut("User name:          %s\n",buf); 
 
    // Get and display the system directory. 
    if(!GetSystemDirectory(buf,bufsize)) strcpy(buf,"??");
    outPut("System Directory:   %s\n",buf); 
 
    // Get and display the Windows directory. 
    if(!GetWindowsDirectory(buf,bufsize)) strcpy(buf,"??");
    outPut("Windows Directory:  %s\n",buf);

    if(SUCCEEDED(SHGetFolderPath(NULL,CSIDL_PERSONAL|CSIDL_FLAG_CREATE,NULL,0,buf))){
      outPut("Personal Folder:    %s\n",buf);
/*
      PathAppend(szPath,TEXT("New Doc.txt"));
      HANDLE hFile = CreateFile(szPath, ...);
*/
      }

    if(SUCCEEDED(SHGetFolderPath(NULL,CSIDL_APPDATA,NULL,0,buf)))
      outPut("Application data:   %s\n",buf);
    if(SUCCEEDED(SHGetFolderPath(NULL,CSIDL_LOCAL_APPDATA,NULL,0,buf)))
      outPut("Local Application Data:     %s\n",buf);
    }
  else if(strstr(verb,"Service")!=NULL){  /* service commands - keep this last */
    if((symid=nbParseSymbol(service,&cursor))==';') strcpy(service,"nodebrain");
    else if(symid!='t'){
      outMsg(0,'E',"Expecting service name");
      return;
      }
    if(strcmp(verb,"createService")==0) nbwCreateService(service);
      else if(strcmp(verb,"deleteService")==0) nbwDeleteService(service);
      else if(strcmp(verb,"startService")==0){
  	char *argv[1];
  	if(nbwStartService(service,0,argv)) outMsg(0,'I',"Started %s service",service);
	else outMsg(0,'E',"Unable to start %s service",service);
  	} 
      else if(strcmp(verb,"stopService")==0){
        if(nbwStopService(service)) outMsg(0,'I',"Stopped %s service",service);
        else outMsg(0,'E',"Unable to stop %s service",service);
        }
    else outMsg(0,'E',"Windows service command verb \"%s\" not recognized",verb);
    }
  else outMsg(0,'E',"Windows context verb \"%s\" not recognized",verb);
  }

#if defined(WIN32)
_declspec (dllexport)
#else
extern 
#endif
int nbService(int (*serviceMain)(int argc,char *argv[]),int argc,char *argv[]){
  DWORD   len;
  char *cursor;
  SERVICE_TABLE_ENTRY   DispatchTable[] ={ 
	  {serviceName, nbwServiceStart}, 
	  {NULL, NULL}
	  };

  serviceStart=serviceMain;
  serviceArgc=argc;
  serviceArgv=argv;
  len=GetModuleFileName(NULL,windowsPath,strSize);
  cursor=windowsPath+len-1;
  while(cursor>windowsPath && *cursor!='/' && *cursor!='\\') cursor--;
  if(cursor>windowsPath){*cursor=0;cursor++;}

  *serviceName=0;
  // Note: arv[0] will also be the service name when running as a service
  if(argc>1 && strncmp(argv[1],"service=",8)==0) strcpy(serviceName,argv[1]+8);
  if(*serviceName==0) return((*serviceMain)(argc,argv));
  else return(StartServiceCtrlDispatcher(DispatchTable));
  }
#endif

/*
* Turn into a daemon
*/
#if defined(WIN32)
void daemonize(void){
  char *log=outLogName(NULL);
  FILE *file;

  outMsg(0,'I',"NodeBrain %s[%d] daemonizing",myname,GetCurrentProcessId());

  if(nb_Service || *log!=0){
    // make sure we can open the log file before reopening stderr
    file=fopen(log,"a");
	if(file==NULL){
      outMsg(0,'E',"Unable to open log file '%s' - errno=%d %s",log,errno,strerror(errno));
      outMsg(0,'E',"NodeBrain %s[%d] terminating with severe error - exit code=%d",myname,GetCurrentProcessId(),NB_EXITCODE_FAIL);
      outFlush();
      exit(NB_EXITCODE_FAIL);
	  }
	else fclose(file);
	if(!nb_Service){
      outMsg(0,'I',"Redirecting output to log file");
      outMsg(0,'I',"Enter Ctl-C to terminate foreground agent");
      }
    outFlush();
    fflush(NULL);
    file=freopen("nul","r",stdin);   // may not need to do this
	if(file==NULL) outMsg(0,'E',"Unable to redirect stdin");
    else SetHandleInformation((HANDLE)_get_osfhandle(_fileno(stdin)),HANDLE_FLAG_INHERIT,0);
    file=freopen("nul","w",stdout);  // may not need to do this
	if(file==NULL) outMsg(0,'E',"Unable to redirect stdout");
	else SetHandleInformation((HANDLE)_get_osfhandle(_fileno(stdout)),HANDLE_FLAG_INHERIT,0);
    if(*log==0) file=freopen("nul","w",stderr);
    else file=freopen(log,"a",stderr);
	if(file==NULL) exit(NB_EXITCODE_FAIL);
    SetHandleInformation((HANDLE)_get_osfhandle(_fileno(stderr)),HANDLE_FLAG_INHERIT,0);
    }
  else{  // interactive and no log file
    outMsg(0,'I',"No log file specified---will log to console");
    outMsg(0,'I',"Enter Ctl-C to terminate foreground agent\n\n");
    outFlush();
    fflush(NULL);
    }

  showHeading();

  agent=1;
  }

#else

//extern struct sigaction sigact;

void daemonize(){
  char *log=outLogName(NULL);
  pid_t pid,ppid;
  void (*sighandler)(int);
  int fd;

  pid=getpid();
  ppid=getppid();
  outMsg(0,'I',"NodeBrain %s[%d] daemonizing",myname,pid);
  // make sure we can open the log file before reassigning stderr
  // tests may show the code below to be sufficient - comment this out to test
  if(*log!=0){
	if((fd=open(log,O_CREAT|O_RDWR|O_APPEND,S_IRUSR|S_IWUSR|S_IRGRP))<0){
      outMsg(0,'E',"Unable to open log file '%s' - errno=%d %s",log,errno,strerror(errno));
	  outMsg(0,'E',"NodeBrain %s[%d] terminating with severe error - exit code=%d",myname,pid,NB_EXITCODE_FAIL);
      outFlush();
	  exit(NB_EXITCODE_FAIL);
	  }
	else close(fd);
    }
  outFlush();
  fflush(NULL);
  if(ppid!=1){
    sighandler=signal(SIGHUP,SIG_IGN);
    pid = fork();
    if(pid > 0)  exit(0);                /* parent */
    if(pid < 0){
      outMsg(0,'E',"Fork to create daemon failed - errno=%d",errno);
      outFlush();
      exit(NB_EXITCODE_FAIL);
      }
    setsid();
    signal(SIGHUP,sighandler);  // restore SIGHUP signal handler
    }
  /* redirect stdin/stdout/stderr to /dev/null or daemon log */
  close(0);
  close(1);
  open("/dev/null",O_RDWR);
  if(*log!=0 && open(log,O_CREAT|O_RDWR|O_APPEND,S_IRUSR|S_IWUSR|S_IRGRP)!=1){
    outMsg(0,'E',"Unable to open log file '%s' - errno=%d %s",log,errno,strerror(errno));
    outMsg(0,'E',"NodeBrain %s[%d] terminating with severe error - exit code=%d",myname,pid,NB_EXITCODE_FAIL);
    outFlush();
    exit(NB_EXITCODE_FAIL);
    } 
  else dup(0);
  close(2);
  dup(1);
  //umask(077);
  umask(S_IWGRP|S_IRWXO);  // 2008-06-11 eat - avoid group write and all other user access
  //sleep(1);    // 2005-10-11 eat - what is this for?
  agent=1;
  pid=getpid();
  ppid=getppid();
  showHeading();
  if(trace){
    // 2006-01-07 make sure we don't have unnecessary files open
    // We need to assume that whatever files are open, that they
    // should remain open when we daemonize.  For example, we can
    // have Medulla processes running.
    fprintf(stderr,"checking for open files\n");
    for(fd=getdtablesize();fd>2;fd--){
      if(fcntl(fd,F_GETFD)>=0){
        fprintf(stderr,"file %d is open\n",fd);
        }
      }
    }
  return;
  }
#endif
