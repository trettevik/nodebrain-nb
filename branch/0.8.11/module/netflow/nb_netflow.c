/*
* Copyright (C) 2004-2010 The Boeing Company
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
*
* Name:   nb_netflow.c
*
* Title:  Netflow Export Datagram Monitor 
*
* Function:
*
*   This program is a NodeBrain skill module for monitoring network
*   traffic using Netflow Export Datagrams (UDP packets).
*   
* Description:
*
*   Our primary goal is to identify common worm behavior. By this,
*   we mean machines that generate traffic to a large number of
*   other machines in a relatively short period of time.  For this
*   purpose we define a flow to be a subset of the fields in a
*   netflow flow.
*
*      fromAddr,toAddr,protocol,toPort
*   
*   Secondly we define a record for maintaining counters for any
*   given address.
*
*      addr,fromFlows,toFlows
*   
*   We maintain a hash table of flows and addresses.  For every
*   flow recieved from netflow, we check the flow hash table to see
*   if we already know about it.  If so, we do nothing.  If not,
*   we increment the appropriate counter in the address hash table
*   for the source and destination addresses.
*
*   When an address's fromFlows counter hits a threshold we 
*   analyze the flows for the address to determin if it looks
*   like a worm or an infrustructure server.  If it looks like
*   an infrustructure server, we flag the address to ignore
*   all future flows for the address.  If it looks like a worm
*   we issue an alarm and flag the address to avoid issuing
*   more alarms for the same address.  If we aren't sure, we
*   leave the address unflagged, for re-evaluation in a future
*   interval.
*
*   Every T seconds we clear the cache tables and start over.
*   This does not include the address attribute cache table
*
*=====================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2004-04-13 Ed Trettevik - original prototype version
* 2005-03-29 eat 0.6.2  Adjusted for compiling on Windows
* 2005-04-09 eat 0.6.2  adapted to API changes
* 2005-05-01 eat 0.6.2  updated conditionals for FREEBSD
* 2005-05-14 eat 0.6.3  netflowBind() modified to accept moduleHandle
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
*=====================================================================
*/
#include <config.h>
#include <nb.h>

struct NB_MOD_NETFLOW_METRIC_ARRAY{
  double protocolPkts[256];
  double protocolBytes[256];
  double tcpPortPkts[65536];
  double tcpPortBytes[65536];
  double udpPortPkts[65536];
  double udpPortBytes[65536];
  };

struct NB_MOD_NETFLOW_PERIOD{
  struct NB_MOD_NETFLOW_METRIC_ARRAY average;
  struct NB_MOD_NETFLOW_METRIC_ARRAY last;
  };

struct NB_MOD_NETFLOW_MEASURE{
  double aveval;       
  double avedev;
  double variance;
  double e;            /* expected value */
  double eStep;        /* expected value step */
  double r;            /* range */
  double rStep;        /* range step */
  };

struct NB_MOD_NETFLOW_VOLUME{
  double packets;
  double bytes;
  };

struct NB_MOD_NETFLOW{          /* Netflow node descriptor */
  unsigned int   socket;        /* server socket for datagrams */
  unsigned short port;          /* UDP port of listener */
  int            hfile;         /* history file */
  char          *hfilename;     /* history file name */
  nbCELL         streamAlerts;      /* stream alert messages to consoles */
  nbCELL         streamEngineStats; /* stream engine stats to consoles */
  nbCELL         streamFlows;   /* stream flows to consoles */
  unsigned char  trace;         /* trace option */
  unsigned char  format;        /* option to format flows in trace */
  unsigned char  dump;          /* option to dump packets in trace */
  unsigned char  null;          /* don't apply rules - just count flows */
  unsigned char  display;       /* display flows on next reset */
  unsigned int   flowThresh;    /* flow threshold for analysis */
  unsigned int   flowCount;     /* flow count in interval */
  unsigned int   flowCountPrev; /* flow count for previous interval */
  unsigned int   flowCountMon;  /* count of flows monitored in an interval */
  unsigned int   routerAddr;    /* router address */
  struct NB_MOD_NETFLOW_DEVICE *device; 
  struct NB_MOD_NETFLOW_HASH *hashFlow;
  struct NB_MOD_NETFLOW_HASH *hashAddr;
  struct NB_MOD_NETFLOW_HASH *hashAttr;

  int periodNumber;
  int intervalNumber;
  int intervalsPerPeriod;       /* n */
  int minutesPerSum;
  int secondsPerCheck;
  int sumsPerHour;
  int checksPerSum;
  
  struct NB_MOD_NETFLOW_VOLUME protocolSum[256];
  struct NB_MOD_NETFLOW_VOLUME tcpPortSum[65536];
  struct NB_MOD_NETFLOW_VOLUME udpPortSum[65536];
  struct NB_MOD_NETFLOW_MEASURE protocolPkts[256];
  struct NB_MOD_NETFLOW_MEASURE protocolBytes[256];
  struct NB_MOD_NETFLOW_MEASURE tcpPortPkts[65536];
  struct NB_MOD_NETFLOW_MEASURE tcpPortBytes[65536];
  struct NB_MOD_NETFLOW_MEASURE udpPortPkts[65536];
  struct NB_MOD_NETFLOW_MEASURE udpPortBytes[65536];
  struct NB_MOD_NETFLOW_PERIOD periodProfile;
  };

typedef struct NB_MOD_NETFLOW NB_MOD_Netflow;

struct NB_MOD_NETFLOW_DEVICE{         /* source netflow device */
  struct NB_MOD_NETFLOW_DEVICE *next;
  unsigned int  address;              /* device address */
  unsigned char engineid;             /* slot number of flow switching engine */
  unsigned char name[63];             /* device name */
  unsigned int  v5pkts;               /* version 5 packets in last interval */
  unsigned int  v7pkts;               /* version 7 packets in last interval */
  unsigned int  pkts;                 /* total packet in last interval */
  unsigned int  flowSeqRef;           /* reference flow sequence number */
  unsigned int  flowSeqLast;          /* last flow sequence number */
  };

struct NB_MOD_NETFLOW_ATTR{ 
  struct NB_MOD_NETFLOW_ATTR *next;
  unsigned int  address;
  unsigned short flags;
  };

#define ON_NONE     0
#define ON_IGNORE   1
#define OFF_NONE    0xffff
#define OFF_IGNORE  OFF_NONE^ON_IGNORE

#define VARBYTESIZE 8   /* experiment with powers of 2 from 4 to 32 */

struct NB_MOD_NETFLOW_ADDR{
  struct NB_MOD_NETFLOW_ADDR *next;
  unsigned int  address;
  unsigned int  fromFlows;
  unsigned int  toFlows;
  u_short variation; /* number of bits 0-255 on */
  char varbyte[VARBYTESIZE];  /* variation hash bits */
  };

struct NB_MOD_NETFLOW_FLOW{
  struct NB_MOD_NETFLOW_FLOW *next;
  unsigned int   packets;
  unsigned int   bytes;
  unsigned int   fromAddr;
  unsigned int   toAddr;
  unsigned char  protocol;
  unsigned char  pad1;
  unsigned short toPort;
  };

struct NB_MOD_NETFLOW_HASH{
  long  modulo;
  void *free;
  void *vector;
  };

/*================================================================================*/
 
/*
*  Netflow Datagram Format - Version 5 Header and Flow
*/
struct nfv5hdr{
  short version;    /* packet format version number */
  unsigned short count;      /* number of flows in packet */
  int   sysuptime;  /* time in milliseconds since the export device booted */
  int   unixtime;   /* time in seconds since epoch */
  int   unixnsecs;  /* residual nanoseconds since epoch */ 
  unsigned int   flowseq;    /* flow sequence number - total flows seen */
  unsigned char  enginetype; /* engine type */
  unsigned char  engineid;   /* slot number of the flow switching engine */
  short reserved;   /* reserved - zero */
  };

struct nfv5flow{
  unsigned int  srcaddr;    /* source address */
  unsigned int  dstaddr;    /* destination address */
  int   nexthop;    /* next hop address */
  short input;      /* SNMP index of input interface */
  short output;     /* SNMP index of output interface */
  unsigned int  packets;    /* Packets in the flow */
  unsigned int  bytes;      /* total number of layer 3 bytes */
  int   first;      /* SysUptime at start of flow */
  int   last;       /* SysUptime at last packet */
  unsigned short srcport;    /* TCP/UDP source port */
  unsigned short dstport;    /* TCP/UDP destination prot */
  char  pad1;       /* Unused (zero) byte */
  unsigned char tcp_flags;  /* cumulative or of TCP flags */
  unsigned char protocol;   /* IP protocol type (TCP=6, UDP=17, ICMP=1) */
  unsigned char tos;        /* IP type of server (ToS) */
  short srcas;              /* source autonomous sytem number */
  short dstas;              /* destination autonomous system number */
  unsigned char srcmask;    /* source address prefix mask bits */
  unsigned char dstmask;    /* destination address prefix mask bits */
  short pad2;               /* unused (zero) bytes */
  };
  
/*
*  Netflow Datagram Format - Version 7 Header and Flow
*/
struct nfv7hdr{
  short version;    /* packet format version number */
  unsigned short count;      /* number of flows in packet */
  int   sysuptime;  /* time in milliseconds since the export device booted */
  int   unixtime;   /* time in seconds since epoch */
  int   unixnsecs;  /* residual nanoseconds since epoch */
  unsigned int   flowseq;    /* flow sequence number - total flows seen */
  int   reserved;   /* reserved - zero */
  };

struct nfv7flow{
  unsigned int  srcaddr;    /* source address */
  unsigned int  dstaddr;    /* destination address */
  int   nexthop;    /* next hop address */
  short input;      /* SNMP index of input interface */
  short output;     /* SNMP index of output interface */
  unsigned int  packets;    /* Packets in the flow */
  unsigned int  bytes;      /* total number of layer 3 bytes */
  int   first;      /* SysUptime at start of flow */
  int   last;       /* SysUptime at last packet */
  unsigned short srcport;   /* TCP/UDP source port */
  unsigned short dstport;   /* TCP/UDP destination prot */
  unsigned char fieldflags; /* Unused (zero) byte */
  unsigned char tcp_flags;  /* cumulative or of TCP flags */
  unsigned char protocol;   /* IP protocol type (TCP=6, UDP=17, ICMP=1) */
  unsigned char tos;        /* IP type of server (ToS) */
  short srcas;              /* source autonomous sytem number */
  short dstas;              /* destination autonomous system number */
  unsigned char srcmask;    /* source address prefix mask bits */
  unsigned char dstmask;    /* destination address prefix mask bits */
  unsigned short flowflags; /* invalid flow flags */
  unsigned int   router;    /* bypassed router address */ 
  };

/*================================================================================*/

/*
*  Open a history file - initialize if needed
*
*      open(logname,O_CREAT|O_RDWR,S_IREAD|S_IWRITE);
*/
int openHistory(char *filename,int periods,size_t len){
  int file;  
  char *buffer;

#if !defined(FREEBSD) && !defined(mpe) && !defined(MACOS) && !defined(WIN32)
  if((file=open(filename,O_RDWR|O_SYNC))<0){
#else
  if((file=open(filename,O_RDWR))<0){
#endif

#if defined(WIN32)
    if((file=open(filename,O_RDWR|O_CREAT,S_IREAD|S_IWRITE))<=0){
#elif defined(FREEBSD) || defined(mpe) || defined(MACOS) || defined(WIN32)
    if((file=open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<=0){
#else
    if((file=open(filename,O_RDWR|O_CREAT|O_SYNC,S_IRUSR|S_IWUSR))<=0){
#endif
      return(file);
      }
    buffer=malloc(len);
    memset(buffer,0,len);
    while(periods>0){
      write(file,buffer,len);
      periods--;
      }
    free(buffer);
    }
  return(file);
  }

/*
*  Read history period 
*/
int readHistory(int file,void *buffer,int period,size_t len){
  long pos=period*len;

  if(!file) return(0);
  if(lseek(file,pos,SEEK_SET)!=pos) return(0);
  if(read(file,buffer,len)!=(int)len) return(0);
  return(1);
  }

/*
*  Write history period
*/
int writeHistory(int file,char *buffer,int period,size_t len){
  long pos=period*len;

  if(lseek(file,pos,SEEK_SET)!=pos) return(0);
  if(write(file,buffer,len)!=(int)len) return(0);
  return(1);
  }

/*
*  Format a Version 5 Netflow Export Datagram to the log file
*/
void format5(nbCELL context,unsigned char *buf,int len){
  struct nfv5hdr *hdr=(void *)buf;
  struct nfv5flow *flow=(void *)(buf+24);
  int f,n;
  char srcaddr[16],dstaddr[16];
  unsigned short srcport,dstport;
  unsigned char *ipaddress;         

  n=hdr->count;
  nbLogPut(context,"Version=%d Count=%d\n",hdr->version,n);
  for(f=0;f<n;f++){
    ipaddress=(unsigned char *)&flow->srcaddr;
    sprintf(srcaddr,"%3.3u.%3.3u.%3.3u.%3.3u",*ipaddress,*(ipaddress+1),*(ipaddress+2),*(ipaddress+3));
    ipaddress=(unsigned char *)&flow->dstaddr;
    sprintf(dstaddr,"%3.3u.%3.3u.%3.3u.%3.3u",*ipaddress,*(ipaddress+1),*(ipaddress+2),*(ipaddress+3));
    srcport=ntohs(flow->srcport);
    dstport=ntohs(flow->dstport);
    nbLogPut(context,"%s:%5.5u -> %s:%5.5u protocol=%u flags=%2.2x packets=%d bytes=%d\n",srcaddr,srcport,dstaddr,dstport,flow->protocol,flow->tcp_flags,ntohl(flow->packets),ntohl(flow->bytes));
    flow++;
    }
  nbLogPut(context,"\n");
  }
 
/*
*  Format a version 7 Netflow Export Datagram to the log file
*/
void format7(nbCELL context,unsigned char *buf,int len){
  struct nfv7hdr *hdr=(void *)buf;
  struct nfv7flow *flow=(void *)(buf+24);
  int f,n;
  char srcaddr[16],dstaddr[16];
  unsigned short srcport,dstport;
  unsigned char *ipaddress;

  n=hdr->count;
  nbLogPut(context,"Version=%d Count=%d\n",hdr->version,n);
  for(f=0;f<n;f++){
    ipaddress=(unsigned char *)&flow->srcaddr;
    sprintf(srcaddr,"%3.3u.%3.3u.%3.3u.%3.3u",*ipaddress,*(ipaddress+1),*(ipaddress+2),*(ipaddress+3));
    ipaddress=(unsigned char *)&flow->dstaddr;
    sprintf(dstaddr,"%3.3u.%3.3u.%3.3u.%3.3u",*ipaddress,*(ipaddress+1),*(ipaddress+2),*(ipaddress+3));
    srcport=ntohs(flow->srcport);
    dstport=ntohs(flow->dstport);
    nbLogPut(context,"%s:%5.5u -> %s:%5.5u protocol=%u flags=%2.2x packets=%d bytes=%d\n",srcaddr,srcport,dstaddr,dstport,flow->protocol,flow->tcp_flags,ntohl(flow->packets),ntohl(flow->bytes));
    flow++;
    }
  nbLogPut(context,"\n");
  }

/***********************************************************************************
*  Hash routines 
***********************************************************************************/
void *hashNew(int modulo){
  struct NB_MOD_NETFLOW_HASH *hash;
  int vectsize=modulo*sizeof(void *);
  hash=malloc(sizeof(struct NB_MOD_NETFLOW_HASH)-sizeof(void *)+vectsize);
  hash->modulo=modulo;
  hash->free=NULL;
  memset(&hash->vector,0,vectsize);
  return(hash);
  }

/*
*  Reset a hash - move all used entries to the free entry list
*
*    Note: This works for both ADDR and FLOW hashes because the next pointer
*          is the first element of both structures.
*/
void hashReset(struct NB_MOD_NETFLOW_HASH *hash){
  struct NB_MOD_NETFLOW_ADDR **entryP=(struct NB_MOD_NETFLOW_ADDR **)&(hash->vector),*entry,*next;
  int i;
  
  for(i=0;i<hash->modulo;i++){
    for(entry=*entryP;entry!=NULL;entry=next){
      next=entry->next;
      entry->next=hash->free;
      hash->free=entry;
      } 
    *entryP=0;
    entryP++;
    }
  }

void hashFree(struct NB_MOD_NETFLOW_HASH *hash){
  //struct NB_MOD_NETFLOW_ADDR **entryP=(struct NB_MOD_NETFLOW_ADDR **)&(hash->free),*entry,*next;
  struct NB_MOD_NETFLOW_ADDR *entry,*next;
  hashReset(hash);
  for(entry=hash->free;entry!=NULL;entry=next){
    next=entry->next; 
    free(entry);
    }
  free(hash);
  }

/********************************************************************************
*  Cache table routines
********************************************************************************/

/*
*  increment volume counters
*/
void incrementVolume(nbCELL context,NB_MOD_Netflow *netflow,unsigned char protocol,unsigned short port,unsigned int packets,unsigned int bytes){
  static int spud=0;
  if(spud<30){ 
    nbLogMsg(context,0,'T',"incrementVolume called: protocol=%u,port=%u,packets=%u,bytes=%u",protocol,port,packets,bytes);
    spud++;
    }
  netflow->protocolSum[protocol].packets+=packets;
  netflow->protocolSum[protocol].bytes+=bytes;
  if(protocol==6){  /* TCP */
    netflow->tcpPortSum[port].packets+=packets;
    netflow->tcpPortSum[port].bytes+=bytes;
    }
  else if(protocol==17){ /* UDP */
    netflow->udpPortSum[port].packets+=packets;
    netflow->udpPortSum[port].bytes+=bytes;
    }
  }

/*
*  Set flow sequence number
*/
void setSeq(NB_MOD_Netflow *netflow,unsigned int address,unsigned char engineid,unsigned int seq,unsigned short count,short version){
  struct NB_MOD_NETFLOW_DEVICE *device,**deviceP;
  deviceP=&(netflow->device);
  for(device=*deviceP;device!=NULL && (address>device->address || (address==device->address && engineid>device->engineid));device=*deviceP)
    deviceP=(struct NB_MOD_NETFLOW_DEVICE **)&(device->next);
  if(device==NULL || address<device->address || engineid<device->engineid){
    device=malloc(sizeof(struct NB_MOD_NETFLOW_DEVICE));
    device->address=address;
    device->engineid=engineid;
    nbIpGetName(address,(char *)device->name,sizeof(device->name));
    device->v5pkts=0;
    device->v7pkts=0;
    device->pkts=1;
    switch(version){
      case 5: device->v5pkts=1; break;
      case 7: device->v7pkts=1; break;
      }
    device->flowSeqRef=seq-count;
    device->flowSeqLast=seq;
    device->next=*deviceP;
    *deviceP=device;
    }
  else{
    device->pkts++;
    switch(version){
      case 5: device->v5pkts++; break;
      case 7: device->v7pkts++; break;
      }
    device->flowSeqLast=seq;
/*
    if(seq>device->flowSeqLast) device->flowSeqLast=seq;
    else if(seq<device->flowSeqRef){
      if(seq>0x000fffff || device->flowSeqRef<0x000fffff) device->flowSeqRef=seq;
      else device->flowSeqLast=seq;
      }
*/
    }
  }
/*
*  Sum sequence numbers
*/
unsigned int getSeq(nbCELL context,NB_MOD_Netflow *netflow){
  char streamMsg[1024];
  struct NB_MOD_NETFLOW_DEVICE *device;
  char caddr[16];
  unsigned int routerFlows=0,engines=0;
  unsigned int v5pkts=0,v7pkts=0;
  
  nbLogMsg(context,0,'T',"Netflow Engine Table:");
  for(device=netflow->device;device!=NULL;device=device->next){
    engines++;
    sprintf(streamMsg,"Router=%s Engine=%2.2u V5Pkts=%5.5u V7Pkts=%5.5u TotPkts=%8.8u,FirstSeq=%10.10u LastSeq=%10.10u Name=%s\n",nbIpGetAddrString(caddr,device->address),device->engineid,device->v5pkts,device->v7pkts,device->pkts,device->flowSeqRef,device->flowSeqLast,device->name);
    nbStreamPublish(netflow->streamEngineStats,streamMsg);
    nbLogPut(context,"Router=%s Engine=%2.2u V5Pkts=%5.5u V7Pkts=%5.5u TotPkts=%8.8u,FirstSeq=%10.10u LastSeq=%10.10u Name=%s\n",nbIpGetAddrString(caddr,device->address),device->engineid,device->v5pkts,device->v7pkts,device->pkts,device->flowSeqRef,device->flowSeqLast,device->name);
    v5pkts+=device->v5pkts;
    v7pkts+=device->v7pkts;
    device->v5pkts=0;
    device->v7pkts=0;
    routerFlows+=device->flowSeqLast-device->flowSeqRef;
/*
    if(device->flowSeqRef<device->flowSeqLast) routerFlows+=device->flowSeqLast-device->flowSeqRef;
    else if(device->flowSeqRef>device->flowSeqLast)
      routerFlows+=device->flowSeqLast+(0xffffffff-device->flowSeqRef)+1; 
*/
    }
  nbLogMsg(context,0,'T',"Engines=%u v5pkts=%5.5u v7pkts=%5.5u packets=%6.6u",engines,v5pkts,v7pkts,v5pkts+v7pkts);
  return(routerFlows);
  }

/*
*  Set address attribute flags
*/
unsigned short setAttr(NB_MOD_Netflow *netflow,unsigned int address,unsigned short maskOn,unsigned short maskOff){
  struct NB_MOD_NETFLOW_HASH *hash=netflow->hashAttr;
  struct NB_MOD_NETFLOW_ATTR *attr,**attrP;
  unsigned long index;
  index=address%hash->modulo;
  attrP=(struct NB_MOD_NETFLOW_ATTR **)&hash->vector+index;
  for(attr=*attrP;attr!=NULL && address>attr->address;attr=*attrP)
    attrP=(struct NB_MOD_NETFLOW_ATTR **)&(attr->next);
  if(attr==NULL || address<attr->address){
    if(NULL!=(attr=hash->free)) hash->free=attr->next;
    else attr=malloc(sizeof(struct NB_MOD_NETFLOW_ATTR));
    attr->address=address;
    attr->flags=0;
    attr->next=*attrP;
    *attrP=attr;
    }
  attr->flags|=maskOn;
  attr->flags&=maskOff;
  return(attr->flags);
  }

/*
*  Get address attribute flags
*/
unsigned short getAttr(NB_MOD_Netflow *netflow,unsigned int address){
  struct NB_MOD_NETFLOW_HASH *hash=netflow->hashAttr;
  struct NB_MOD_NETFLOW_ATTR *attr,**attrP;
  unsigned long index;
  index=address%hash->modulo;
  attrP=(struct NB_MOD_NETFLOW_ATTR **)&hash->vector+index;
  for(attr=*attrP;attr!=NULL && address>attr->address;attr=*attrP)
    attrP=(struct NB_MOD_NETFLOW_ATTR **)&(attr->next);
  if(attr==NULL || address<attr->address) return(0);
  return(attr->flags);
  }

/*
*  Analyze flows for a given address
*/
void analyzeFlows(nbCELL context,NB_MOD_Netflow *netflow,unsigned int address){
  struct NB_MOD_NETFLOW_HASH *flowHash=netflow->hashFlow;
  struct NB_MOD_NETFLOW_FLOW *flow,**flowP;
  int i;
  time_t atime;  // alert time
  char srcaddr[16],dstaddr[16];
  unsigned int  savf=0,davf=0,spvf=0,dpvf=0,ssvf=0,dsvf=0;
  unsigned char maskbit[8]={1,2,4,8,16,32,64,128};
  unsigned char savfbyte[8192],davfbyte[8192],spvfbyte[8192],dpvfbyte[8192],ssvfbyte[256],dsvfbyte[256];
  unsigned int  bit,byte,proto=0,port=0;
  char cmd[1024],caddr[16],rcaddr[16],ctype[256];

  atime=time(NULL);
  memset(savfbyte,0,8192);
  memset(davfbyte,0,8192);
  memset(spvfbyte,0,8192);
  memset(dpvfbyte,0,8192);
  memset(ssvfbyte,0,256);
  memset(dsvfbyte,0,256);
  nbLogPut(context,"%s Flow Table:\n",nbIpGetAddrString(srcaddr,address));
  flowP=(struct NB_MOD_NETFLOW_FLOW **)&(flowHash->vector);
  for(i=0;i<flowHash->modulo;i++){
    for(flow=*flowP;flow!=NULL;flow=flow->next){
      if(flow->fromAddr==address || flow->toAddr==address){
        proto=flow->protocol;
        port=flow->toPort;
        nbLogPut(context,"%s -> %s %3.3u:%8.8u\n",nbIpGetAddrString(srcaddr,flow->fromAddr),nbIpGetAddrString(dstaddr,flow->toAddr),flow->protocol,flow->toPort);
        if(flow->fromAddr==address){
          byte=(flow->toAddr&0x0000fff8)>>3;
          bit=flow->toAddr&0x00000007;
          if(!(savfbyte[byte]&maskbit[bit])){
            savf++;
            savfbyte[byte]|=maskbit[bit];
            }
          byte=(flow->toPort&0x0000fff8)>>3;
          bit=flow->toPort&0x00000007;
          if(!(spvfbyte[byte]&maskbit[bit])){
            spvf++;
            spvfbyte[byte]|=maskbit[bit];
            }
          if(ssvfbyte[flow->protocol]==0){
            ssvf++;
            ssvfbyte[flow->protocol]=1;
            }
          }
        else{
          byte=(flow->fromAddr&0x0000fff8)>>3;
          bit=flow->fromAddr&0x00000007;
          if(!(davfbyte[byte]&maskbit[bit])){
            davf++;
            davfbyte[byte]|=maskbit[bit];
            }
          byte=(flow->toPort&0x0000fff8)>>3;
          bit=flow->toPort&0x00000007;
          if(!(dpvfbyte[byte]&maskbit[bit])){
            dpvf++;
            dpvfbyte[byte]|=maskbit[bit];
            }
          if(dsvfbyte[flow->protocol]==0){
            dsvf++;
            dsvfbyte[flow->protocol]=1;
            }
          }
        }
      }
    flowP++;
    }
  nbLogPut(context,"Source factors: target=%u protocols=%u ports=%u\n",savf,ssvf,spvf);
  nbLogPut(context,"Target factors: source=%u protocols=%u ports=%u\n",davf,dsvf,dpvf);
  if(savf>=45 && spvf<=2 && ssvf<=2 && davf<=5){
    setAttr(netflow,address,ON_IGNORE,OFF_NONE);
    switch(proto){
      case 1: strcpy(ctype,"SweepIcmp"); break;
      case 6: sprintf(ctype,"SweepTcp%u",port); break;
      case 17: sprintf(ctype,"SweepUdp%u",port); break;
      default: sprintf(ctype,"Sweep%uP%u",proto,port);
      }
    sprintf(cmd,"alert time=%d,severity=3,type=\"%s\",fromIp=\"%s\",toIp=\"\",toProto=%u,toPort=%u,router=\"%s\";",(int)atime,ctype,nbIpGetAddrString(caddr,address),proto,port,nbIpGetAddrString(rcaddr,netflow->routerAddr));
    nbCmd(context,cmd,1);
    nbStreamPublish(netflow->streamAlerts,cmd);
    }
  else{
    /* for now let's just analyze an address once */
    setAttr(netflow,address,ON_IGNORE,OFF_NONE);
    }
  }

/*
*  Check partial sums for anomalies
*/
void partialSum(nbCELL context,char *title,struct NB_MOD_NETFLOW_VOLUME *volume,int n,double min,struct NB_MOD_NETFLOW_MEASURE *packets,struct NB_MOD_NETFLOW_MEASURE *bytes){
  int i;
  char packetLcFlag,packetUcFlag,byteUcFlag,byteLcFlag;

  nbLogMsg(context,0,'T',"%s Parial Sum Table:",title);
  nbLogPut(context,"Index  LCL           Packets      UCL           LCL          Bytes         UCL\n");
  nbLogPut(context,"-----  ------------ ------------  ------------  ------------ ------------  ------------\n");
  for(i=0;i<n;i++){
    if(volume->packets<packets->e-packets->r) packetLcFlag='*';
    else packetLcFlag=' ';
    if(volume->packets>packets->e+packets->r) packetUcFlag='*';
    else packetUcFlag=' ';
    if(volume->bytes<bytes->e-bytes->r) byteLcFlag='*';
    else byteLcFlag=' ';
    if(volume->bytes>bytes->e+bytes->r) byteUcFlag='*';
    else byteUcFlag=' ';
    if(volume->packets>=min){
      nbLogPut(context,"%5.5d %c%e %e %c%e %c%e %e %c%e\n",i,packetLcFlag,packets->e-packets->r,volume->packets,packetUcFlag,packets->e+packets->r,byteLcFlag,bytes->e-bytes->r,volume->bytes,byteUcFlag,bytes->e+bytes->r);
      }
    volume++;
    packets++;
    bytes++;
    }
  }

/*
*  Display volume sum table
*/
void displaySum(nbCELL context,char *title,struct NB_MOD_NETFLOW_VOLUME *volume,int n,double min){
  int i;

  nbLogMsg(context,0,'T',"%s Table:",title);
  nbLogPut(context,"Index Packets      Bytes\n");
  nbLogPut(context,"----- ------------ ------------\n");
  for(i=0;i<n;i++){
    if(volume->packets>=min){
      nbLogPut(context,"%5.5d %e %e\n",i,volume->packets,volume->bytes);
      }
    volume++;
    }
  }

/*
*  Display variation distribution
*/
void displayDist(nbCELL context,struct NB_MOD_NETFLOW_HASH *hash){
  struct NB_MOD_NETFLOW_ADDR **entryP=(struct NB_MOD_NETFLOW_ADDR **)&(hash->vector),*entry;
  int i;
  int dist[256];

  memset(dist,0,VARBYTESIZE*8*sizeof(int));
  for(i=0;i<hash->modulo;i++){
    for(entry=*entryP;entry!=NULL;entry=entry->next){
      dist[entry->variation]++;
      }
    entryP++;
    }
  for(i=0;i<VARBYTESIZE*8;i++){
    nbLogPut(context,"%3.3u %10.10u\n",i,dist[i]);
    }
  }

/*
*  Increment a flow counter for an address
*/
struct NB_MOD_NETFLOW_ADDR *assertAddr(nbCELL context,NB_MOD_Netflow *netflow,unsigned int address,int to,u_int peerAddr,u_char protocol,u_short port){
  struct NB_MOD_NETFLOW_HASH *hash=netflow->hashAddr;
  struct NB_MOD_NETFLOW_ADDR *addr,**addrP;  
  unsigned long index;
  unsigned int byte,bit;
  unsigned char bitmask;

  index=address%hash->modulo;
  addrP=(struct NB_MOD_NETFLOW_ADDR **)&hash->vector+index;
  for(addr=*addrP;addr!=NULL && address>addr->address;addr=*addrP)
    addrP=(struct NB_MOD_NETFLOW_ADDR **)&(addr->next);
  if(addr==NULL || address<addr->address){
    if(NULL!=(addr=hash->free)) hash->free=addr->next;
    else addr=malloc(sizeof(struct NB_MOD_NETFLOW_ADDR));
    addr->address=address;
    addr->fromFlows=0;
    addr->toFlows=0;
    addr->variation=0;
    memset(addr->varbyte,0,VARBYTESIZE);
    addr->next=*addrP;
    *addrP=addr;
    }
  if(to){
    addr->toFlows++;
    if(addr->toFlows==netflow->flowThresh && addr->fromFlows<netflow->flowThresh) analyzeFlows(context,netflow,address);
    }
  else{
    addr->fromFlows++;
/*
    if(addr->fromFlows==netflow->flowThresh && addr->toFlows<netflow->flowThresh) analyzeFlows(context,netflow,address);
*/
    /* calculate variation for given source address */
    index=(protocol*port*(peerAddr&0x000000ff))&0x000000ff; /* number from 0 to 255 */
    bit=index&0x07;
    byte=(index>>3)%VARBYTESIZE;
    bitmask=0x01<<bit;
    if(!(addr->varbyte[byte]&&bitmask)){
      addr->varbyte[byte]&=bitmask;
      addr->variation++;
      }
    if(addr->variation==50 && addr->toFlows<netflow->flowThresh) analyzeFlows(context,netflow,address);
    }
  return(addr);
  }

/*
*  Assert a flow
*/
struct NB_MOD_NETFLOW_FLOW *assertFlow(nbCELL context,NB_MOD_Netflow *netflow,unsigned int packets,unsigned int bytes,unsigned int fromAddr,unsigned int toAddr,unsigned char protocol,unsigned short toPort){
  struct NB_MOD_NETFLOW_HASH *hash=netflow->hashFlow;
  struct NB_MOD_NETFLOW_FLOW *flow,**flowP;
  struct NB_MOD_NETFLOW_ADDR *sAddr,*dAddr;
  unsigned long index;
/*
  printf("assertFlow called fromAddr=%4.4x toAddr=%4.4x protocol=%u toPort=%u\n",fromAddr,toAddr,protocol,toPort);
*/
  index=((((fromAddr%hash->modulo)*(toAddr%hash->modulo))%hash->modulo)*toPort)%hash->modulo;
  flowP=(struct NB_MOD_NETFLOW_FLOW **)&hash->vector+index;
  for(flow=*flowP;flow!=NULL && fromAddr>=flow->fromAddr && toAddr>=flow->toAddr && protocol>=flow->protocol && toPort>flow->toPort;flow=*flowP){
    flowP=(struct NB_MOD_NETFLOW_FLOW **)&(flow->next);
    }
  if(flow==NULL || fromAddr<flow->fromAddr || toAddr<flow->toAddr || protocol<flow->protocol || toPort<flow->toPort){
    if(NULL!=(flow=hash->free)) hash->free=flow->next;
    else flow=malloc(sizeof(struct NB_MOD_NETFLOW_FLOW));
    flow->packets=packets;
    flow->bytes=bytes;
    flow->fromAddr=fromAddr;
    flow->toAddr=toAddr;
    flow->protocol=protocol;
    flow->toPort=toPort;
    flow->next=*flowP;
    *flowP=flow;
    sAddr=assertAddr(context,netflow,fromAddr,0,toAddr,protocol,toPort);
    dAddr=assertAddr(context,netflow,toAddr,1,fromAddr,protocol,toPort); 
    netflow->flowCountMon++;
    }
  else flow->packets+=packets;
  return(flow);
  }

/*
*  Display the address and flow cache table
*/
void displayFlow(nbCELL context,NB_MOD_Netflow *netflow){
  struct NB_MOD_NETFLOW_HASH *flowHash=netflow->hashFlow,*addrHash=netflow->hashAddr;
  struct NB_MOD_NETFLOW_ADDR *addr,**addrP;
  struct NB_MOD_NETFLOW_FLOW *flow,**flowP;
  int i;
  char srcaddr[16],dstaddr[16];

  nbLogPut(context,"Address Table:\n");
  addrP=(struct NB_MOD_NETFLOW_ADDR **)&(addrHash->vector);
  for(i=0;i<addrHash->modulo;i++){
    for(addr=*addrP;addr!=NULL;addr=addr->next){
      nbLogPut(context,"%s %8.8u %8.8u\n",nbIpGetAddrString(srcaddr,addr->address),addr->fromFlows,addr->toFlows);
      }
    addrP++;
    }
  nbLogPut(context,"Flow Table:\n");             
  flowP=(struct NB_MOD_NETFLOW_FLOW **)&(flowHash->vector);
  for(i=0;i<flowHash->modulo;i++){
    for(flow=*flowP;flow!=NULL;flow=flow->next){
      nbLogPut(context,"%s -> %s %3.3u:%8.8u\n",nbIpGetAddrString(srcaddr,flow->fromAddr),nbIpGetAddrString(dstaddr,flow->toAddr),flow->protocol,flow->toPort);
      }
    flowP++;
    }
  }
/*
*  Display the address and flow cache table
*/
void streamFlows(nbCELL context,NB_MOD_Netflow *netflow){
  struct NB_MOD_NETFLOW_HASH *flowHash=netflow->hashFlow;
  struct NB_MOD_NETFLOW_FLOW *flow,**flowP;
  int i;
  char srcaddr[16],dstaddr[16];
  char flowMsg[1024];
  char utctime[12];

  sprintf(utctime,"%u",(unsigned int)time(NULL));
  flowP=(struct NB_MOD_NETFLOW_FLOW **)&(flowHash->vector);
  for(i=0;i<flowHash->modulo;i++){
    for(flow=*flowP;flow!=NULL;flow=flow->next){
      sprintf(flowMsg,"Netflow.Flow;%s;%u;%u;%s;%s;%u;%u\n",utctime,flow->packets,flow->bytes,nbIpGetAddrString(srcaddr,flow->fromAddr),nbIpGetAddrString(dstaddr,flow->toAddr),flow->protocol,flow->toPort);
      nbStreamPublish(netflow->streamFlows,flowMsg);
      }
    flowP++;
    }
  }

/*
*  Reset flow cache
*/
void resetFlow(nbCELL context,NB_MOD_Netflow *netflow){
  unsigned int seqCount;

  seqCount=getSeq(context,netflow);
  if(seqCount==0){
    nbLogMsg(context,0,'T',"No packets received in this interval.");
    return;
    }
  nbLogMsg(context,0,'T',"Interval statistics: monitoredFlows=%u intervalFlows=%u sensorFlows=%u engineFlows=%u difference=%u (%d%c)",netflow->flowCountMon,netflow->flowCount-netflow->flowCountPrev,netflow->flowCount,seqCount,seqCount-netflow->flowCount,(double)((seqCount-netflow->flowCount)/seqCount)*100,'%');
  if(netflow->display){
    nbLogMsg(context,0,'T',"calling displayFlow");
    displayFlow(context,netflow);
    netflow->display=0;  /* clear display option */
    }
  displayDist(context,netflow->hashAddr);
  streamFlows(context,netflow);
  hashReset(netflow->hashAddr);
  hashReset(netflow->hashFlow);
  /* reset interval flow counters */
  netflow->flowCountMon=0;
  netflow->flowCountPrev=netflow->flowCount;
  }

void loadPeriod(nbCELL context,NB_MOD_Netflow *netflow){
  long weektime,period;
  
  weektime=time(NULL)%(7*24*60*60); /* compute time within week */
  period=weektime/(60*60);      /* period - hour of week */

  if(0>readHistory(netflow->hfile,&netflow->periodProfile,period,sizeof(struct NB_MOD_NETFLOW_PERIOD))){
    nbLogMsg(context,0,'L',"Unable to read history file - using null history period");
    memset(&netflow->periodProfile,0,sizeof(struct NB_MOD_NETFLOW_PERIOD));
    } 
  }

void sumInterval(nbCELL context,NB_MOD_Netflow *netflow){
  }
 
/*
*  Check for statistical anomalies
*/
void checkInterval(nbCELL context,NB_MOD_Netflow *netflow){
  static int first=1;
  static int n=5,N=5;  /* test at 5 minute sum interval */
 
  if(first){
    loadPeriod(context,netflow);
    return;
    }
  /* displaySum(context,"Protocol",netflow->protocolSum,256,1.0); */
  partialSum(context,"Protocol",netflow->protocolSum,256,1.0,netflow->protocolPkts,netflow->protocolBytes);
  partialSum(context,"TCP Port",netflow->tcpPortSum,65536,2000.0,netflow->tcpPortPkts,netflow->tcpPortBytes);
  partialSum(context,"UDP Port",netflow->udpPortSum,65536,1000.0,netflow->udpPortPkts,netflow->udpPortBytes);
  n--;
  if(n<=0){
    memset(netflow->protocolSum,0,sizeof(netflow->protocolSum));
    memset(netflow->tcpPortSum,0,sizeof(netflow->tcpPortSum));
    memset(netflow->udpPortSum,0,sizeof(netflow->udpPortSum));
    n=N;
    sumInterval(context,netflow);
    }
  }

void handleV5(nbCELL context,NB_MOD_Netflow *netflow,void *buffer,int len){
  struct nfv5hdr *hdr=(void *)buffer;
  struct nfv5flow *flow=(struct nfv5flow *)((char *)buffer+sizeof(struct nfv5hdr));
  int f;

  for(f=0;f<hdr->count;f++){
    incrementVolume(context,netflow,flow->protocol,flow->dstport,flow->packets,flow->bytes);
    /* ignore flows involving addressses we have flagged to ignore */
    if(!(getAttr(netflow,flow->srcaddr)&ON_IGNORE || getAttr(netflow,flow->dstaddr)&ON_IGNORE))
      assertFlow(context,netflow,flow->packets,flow->bytes,flow->srcaddr,flow->dstaddr,flow->protocol,flow->dstport);
    flow++;
    }
  }

void handleV7(nbCELL context,NB_MOD_Netflow *netflow,void *buffer,int len){
  struct nfv7hdr *hdr=(void *)buffer;
  struct nfv7flow *flow=(void *)((char *)buffer+sizeof(struct nfv7hdr));
  int f;

  for(f=0;f<hdr->count;f++){
    incrementVolume(context,netflow,flow->protocol,flow->dstport,flow->packets,flow->bytes);
    /* ignore flows involving addressses we have flagged to ignore */
    if(!(getAttr(netflow,flow->srcaddr)&ON_IGNORE || getAttr(netflow,flow->dstaddr)&ON_IGNORE))
      assertFlow(context,netflow,flow->packets,flow->bytes,flow->srcaddr,flow->dstaddr,flow->protocol,flow->dstport);
    flow++;
    }

  }

/*================================================================================*/
/*
*  Read incoming packets
*/
void netflowRead(nbCELL context,int serverSocket,void *handle){
  NB_MOD_Netflow *netflow=handle;
  unsigned char buffer[NB_BUFSIZE];
  size_t buflen=NB_BUFSIZE;
  int  len;
  unsigned short rport;
  char daddr[40],raddr[40];
  struct nfv5hdr *hdr=(void *)buffer;
  //struct nfv5flow *flow=(void *)(buffer+24);

  nbIpGetSocketAddrString(serverSocket,daddr);
  len=nbIpGetDatagram(context,serverSocket,&netflow->routerAddr,&rport,buffer,buflen);
  if(netflow->trace){
    nbLogMsg(context,0,'I',"Datagram %s:%5.5u -> %s len=%d version=%d\n",nbIpGetAddrString(raddr,netflow->routerAddr),rport,daddr,len,hdr->version);
    if(netflow->dump) nbLogDump(context,buffer,len);
    if(netflow->format){
      switch(hdr->version){
        case 5: format5(context,buffer,len); break;
        case 7: format7(context,buffer,len); break;
        }
      }
    }
  /* assert the flows in this packet */
  netflow->flowCount+=hdr->count;
  /* set the flow sequence number for the router */
  setSeq(netflow,netflow->routerAddr,hdr->engineid,hdr->flowseq,hdr->count,hdr->version);
/*
  nbLogPut(context,"router %s enginetype=%u engineid=%u flowseq=%u\n",nbIpGetAddrString(daddr,netflow->routerAddr),hdr->enginetype,hdr->engineid,hdr->flowseq);
*/
  if(!netflow->null){
    switch(hdr->version){
      case 5: handleV5(context,netflow,buffer,len); break;
      case 7: handleV7(context,netflow,buffer,len); break;
      }
    }
  }

/*
*  Subscription handler for EngineStats (and possibly others if it is just a stub)
*/
void netflowSubscribe(nbCELL context,void *handle,char *topic,int state){
  /* ignore subscriptions for now */
  }

/*
*  construct() method
*
*    define <term> node <skill>[(<args>)][:<text>]
*
*    define netflow node netflow(9985);
*/
void *netflowConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  NB_MOD_Netflow *netflow;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  double r,d;
  unsigned int port;
  int trace=0,dump=0,format=0,null=0;
  int hfile=0;
  char *hfilename="";

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(!cell || nbCellGetType(context,cell)!=NB_TYPE_REAL){
    nbLogMsg(context,0,'E',"Expecting numeric UDP port number as first argument");
    return(NULL);
    }
  r=nbCellGetReal(context,cell);
  nbCellDrop(context,cell);
  port=(unsigned int)r;
  d=port;
  if(d!=r || d==0){
    nbLogMsg(context,0,'E',"Expecting non-zero integer UDP port number as first argument");
    return(NULL);
    }
  cell=nbListGetCellValue(context,&argSet);
  if(cell){
    if(nbCellGetType(context,cell)!=NB_TYPE_STRING){
      nbLogMsg(context,0,'E',"Expecting string argument for history file name");
      return(NULL);
      }
    hfilename=nbCellGetString(context,cell);
    hfilename=strdup(hfilename);
    nbCellDrop(context,cell);
    hfile=openHistory(hfilename,7*24,sizeof(struct NB_MOD_NETFLOW_PERIOD));
    if(hfile<0){
      nbLogMsg(context,0,'E',"Unable to open history file");
      return(NULL);
      }
    cell=nbListGetCellValue(context,&argSet);
    if(cell!=NULL){
      nbLogMsg(context,0,'W',"Unexpected argument - third argument and beyond ignored");
      }
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim==NULL) delim=strchr(cursor,0);
    saveDelim=*delim;
    *delim=0;
    if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"format")==0){trace=1;format=1;}
    else if(strcmp(cursor,"trace")==0) trace=1; 
    else if(strcmp(cursor,"null")==0) null=1; 
    *delim=saveDelim;
    cursor=delim;
    if(*cursor==',') cursor++;
    while(*cursor==' ') cursor++;
    }
  netflow=malloc(sizeof(NB_MOD_Netflow));
  netflow->socket=0;
  netflow->port=port;
  netflow->hfile=hfile;
  netflow->hfilename=hfilename;
  netflow->trace=trace;
  netflow->dump=dump;
  netflow->format=format;
  netflow->null=null;
  netflow->device=NULL;
  netflow->flowThresh=100;
  netflow->flowCount=0;
  netflow->flowCountPrev=0;
  netflow->flowCountMon=0;
  netflow->hashFlow=hashNew(9601);
  netflow->hashAddr=hashNew(9601);
  netflow->hashAttr=hashNew(9601);
  memset(netflow->protocolSum,0,sizeof(netflow->protocolSum));
  memset(netflow->tcpPortSum,0,sizeof(netflow->tcpPortSum));
  memset(netflow->udpPortSum,0,sizeof(netflow->udpPortSum));
  memset(netflow->protocolPkts,0,sizeof(netflow->protocolPkts));
  memset(netflow->protocolBytes,0,sizeof(netflow->protocolBytes));
  memset(netflow->tcpPortPkts,0,sizeof(netflow->tcpPortPkts));
  memset(netflow->tcpPortBytes,0,sizeof(netflow->tcpPortBytes));
  memset(netflow->udpPortPkts,0,sizeof(netflow->udpPortPkts));
  memset(netflow->udpPortBytes,0,sizeof(netflow->udpPortBytes));
  memset(&netflow->periodProfile,0,sizeof(netflow->periodProfile));

  netflow->periodNumber=-1;            /* indicates we have not started yet */
  netflow->intervalNumber=0;
  netflow->minutesPerSum=5;            /* set for debugging - should be a parameter with a default of 15 or 30 */
  netflow->secondsPerCheck=60;         /* set for debugging - should be a parameter with a default=60 */
  netflow->sumsPerHour=60/netflow->minutesPerSum;  /* debug with 5 minute sum intervals  */
  netflow->checksPerSum=netflow->minutesPerSum*60/netflow->secondsPerCheck;

  netflow->streamAlerts=nbStreamProducerOpen(context,"Netflow.Alert",netflow,netflowSubscribe);
  netflow->streamEngineStats=nbStreamProducerOpen(context,"Netflow.EngineStats",netflow,netflowSubscribe);
  netflow->streamFlows=nbStreamProducerOpen(context,"Netflow.Flow",netflow,netflowSubscribe); 
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(netflow);
  }

/*
*  enable() method
*
*    enable <node>
*/
int netflowEnable(nbCELL context,void *skillHandle,NB_MOD_Netflow *netflow){
  if((netflow->socket=nbIpGetUdpServerSocket(context,"",netflow->port))<0){
    nbLogMsg(context,0,'E',"Unable to listen on port %s\n",netflow->port);
    return(1);
    }
  nbListenerAdd(context,netflow->socket,netflow,netflowRead);
  nbLogMsg(context,0,'I',"Listening on port %u for Netflow Export Datagrams",netflow->port);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int netflowDisable(nbCELL context,void *skillHandle,NB_MOD_Netflow *netflow){
  nbListenerRemove(context,netflow->socket);
  close(netflow->socket);
  netflow->socket=0;
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*
*    <node>:check,reset,display
*/
int *netflowCommand(nbCELL context,void *skillHandle,NB_MOD_Netflow *netflow,nbCELL arglist,char *text){
  if(netflow->trace){
    nbLogMsg(context,0,'T',"nb_netflow:netflowCommand() text=[%s]\n",text);
    }
  if(strstr(text,"check")) checkInterval(context,netflow);
  else if(strstr(text,"reset")) resetFlow(context,netflow);
  else if(strstr(text,"display")) netflow->display=1;
  /* process commands here */
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int netflowDestroy(nbCELL context,void *skillHandle,NB_MOD_Netflow *netflow){
  nbLogMsg(context,0,'T',"netflowDestroy called");
  if(netflow->socket!=0) netflowDisable(context,skillHandle,netflow);
  hashFree(netflow->hashFlow);
  hashFree(netflow->hashAddr);
  free(netflow);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *netflowBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,netflowConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,netflowDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,netflowEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,netflowCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,netflowDestroy);
  return(NULL);
  }
