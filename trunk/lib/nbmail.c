/*
* Copyright (C) 2009-2012 The Boeing Company
*                         Ed Trettevik <eat@nodebrain.org>
*=============================================================================
*
* File:    nbmail.c
*
* Title:   Mailer Library Functions
*
* Purpose:
*
*   This file provides the NodeBrain Mailer API.  Although it is available to
*   any module, it should be accessed via the mailer skill of the Mail module
*   when possible.
*
* Synopsis:
*
*   int nbMailSendForm(nbCELL context,unsigned char *message,int msglen,nbMailClient *client);
*
* Description:
*
*   This message handler converts messages to send to users
*     
*
* Credits:
*
*   This API evolved from a NodeBrain module "mailer" skill developed by
*   Cliff Bynum in April of 2010 for application called Bingo.
*
*============================================================================
* History:
*
* Date       Name/Change
* ---------- ----------------------------------------------------------------
* 2011-11-05 Ed Trettevik - based this API on Cliff's mailer
* 2012-05-20 eat - replaced socket IO with NodeBrain TLS Layer
*            This enables failover which is built into the TLS routines and
*            prepares for the support of SMTPS.
* 2012-05-20 eat - include mailTrace (traceMail) for debugging
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
*============================================================================
*/
#include <nbi.h>

int mailTrace=0;  // debugging

char *nbSymCmd(nbCELL context,char *source,char *style);

/*
*  Creat Mail Client
*/
nbMailClient *nbMailClientCreate(nbCELL context){
  nbMailClient *client;
  client=(nbMailClient *)nbAlloc(sizeof(nbMailClient));
  memset(client,0,sizeof(nbMailClient));
  client->from=nbTermLocateHere(context,"_from"); // reply address term
  if(!client->from) client->from=nbTermCreate(context,"_from",nbCellCreateString(context,"root@localhost"));
  client->to=nbTermLocateHere(context,"_to");     // destination address term
  if(!client->to) client->to=nbTermCreate(context,"_to",nbCellCreateString(context,"root@localhost"));
  client->topic=nbTermLocateHere(context,"_topic");  // topic term
  if(!client->topic) client->topic=nbTermCreate(context,"_topic",nbCellCreateString(context,""));
  client->tweet=nbTermLocateHere(context,"_tweet");  // tweet term
  if(!client->tweet) client->tweet=nbTermCreate(context,"_tweet",nbCellCreateString(context,""));
  client->form=nbTermLocateHere(context,"_form"); // form term
  if(!client->form){
    NB_Text *text=nbTextCreate("*** form not found ***");
    client->form=nbTermCreate(context,"_form",(nbCELL)text);
    }
  client->relay=nbTermOptionString(context,"Relay","tcp://localhost:25");
  client->tls=nbTlsCreate(NULL,client->relay);
  if(!client->tls){
    nbLogMsg(context,0,'E',"nbMailClientCreate: unable to create TLS handle");
    return(NULL);
    }
  if(gethostname(client->host,sizeof(client->host))) strcpy(client->host,"anonymous");
  return(client);
  }

/*
*  Write 
*/
static int nbMailWrite(nbCELL context,nbMailClient *client,char *buffer,int len){
  if(nbTlsWrite(client->tls,buffer,len)!=len){
    nbTlsClose(client->tls);
    return(-1);
    }
  if(mailTrace) nbLogMsg(context,0,'T',"Sent:\n%s",buffer);
  return(0);
  }

/*
*  Read 
*/
static int nbMailRead(nbCELL context,nbMailClient *client,char *buffer,int buflen){
  int len;
  if((len=nbTlsRead(client->tls,buffer,buflen))<=0){
    nbTlsClose(client->tls);
    return(-1);
    }
  *(buffer+len)=0;
  if(mailTrace) nbLogMsg(context,0,'T',"Reply:\n%s",buffer);
  return(0);
  }

/*
*  Send message
*
*  Returns: 0 - success, -1 - error
*    
*/
int nbMailSend(nbCELL context,nbMailClient *client,char *from,char *to,char *topic,char *msg,char *body){
  char buffer[16*1024];

  if(mailTrace) nbLogMsg(context,0,'T',"nbMailSend: called");
  if(nbTlsConnect(client->tls)){
    nbLogMsg(context,0,'E',"nbMailSend: unable to connect to relay");
    return(-1);
    }
  if(nbMailRead(context,client,buffer,sizeof(buffer))) return(-1);
  sprintf(buffer,"EHLO %s\n",client->host);
  if(nbMailWrite(context,client,buffer,strlen(buffer))) return(-1);
  if(nbMailRead(context,client,buffer,sizeof(buffer))) return(-1);
  sprintf(buffer,"MAIL From: <%s>\n",from);
  if(nbMailWrite(context,client,buffer,strlen(buffer))) return(-1);
  if(nbMailRead(context,client,buffer,sizeof(buffer))) return(-1);
  snprintf(buffer,sizeof(buffer),"RCPT To: <%s>\n",to);
  if(nbMailWrite(context,client,buffer,strlen(buffer))) return(-1);
  if(nbMailRead(context,client,buffer,sizeof(buffer))) return(-1);
  strcpy(buffer,"DATA\n");
  if(nbMailWrite(context,client,buffer,strlen(buffer))) return(-1);
  if(nbMailRead(context,client,buffer,sizeof(buffer))) return(-1);
  snprintf(buffer,sizeof(buffer),"Subject: %s%s\nTo: %s\n%s\r\n.\r\n",topic,msg,to,body);
  if(nbMailWrite(context,client,buffer,strlen(buffer))) return(-1);
  if(nbMailRead(context,client,buffer,sizeof(buffer))) return(-1);
  nbTlsClose(client->tls); // close - nbMailWrite and nbMailRead close when they fail
  return(0);
  }

/*
*  Send alarm message
*
*    This function uses the _form text object which can be
*    assigned by an alert.
*
*    define form1 text form1.txt;
*    define form2 text form2.txt;
*
*    alert _form=form1,_to="fred@nb.com",...
*
*  Returns: 
*    -1 - error
*     0 - success
*/
int nbMailSendAlarm(nbCELL context,nbMailClient *client){
  char *from,*to,*topic,*tweet,*form,*body="";
  nbCELL fromCell,toCell,topicCell,tweetCell,formCell;

  if(mailTrace) nbLogMsg(context,0,'T',"nbMailSendAlarm: called");
  fromCell=nbTermGetDefinition(context,client->from);
  toCell=nbTermGetDefinition(context,client->to);
  topicCell=nbTermGetDefinition(context,client->topic);
  tweetCell=nbTermGetDefinition(context,client->tweet);
  formCell=nbTermGetDefinition(context,client->form);
  if(!fromCell || !toCell || !topicCell || !tweetCell || !formCell){
    nbLogMsg(context,0,'T',"_from, _to, _topic, _tweet, or _form  not defined");
    return(-1);
    }
  from=nbCellGetString(context,fromCell); 
  to=nbCellGetString(context,toCell); 
  topic=nbCellGetString(context,topicCell); 
  tweet=nbCellGetString(context,tweetCell);
  if(!from || !to || !topic || !tweet){
    nbLogMsg(context,0,'T',"_from, _to, _topic, or _tweet not defined as string");
    return(-1);
    }
  form=nbCellGetText(context,formCell);
  if(form) body=nbSymCmd(context,form,"${}"); 
  if(nbMailSend(context,client,from,to,topic,tweet,body)){
    nbLogMsg(context,0,'E',"nbMailSendAlarm: Unable to send message:\nFrom: %s\nTo: %s\nSubject: %s%s\n%s",from,to,topic,tweet,body);
    return(-1);
    }
  nbLogMsg(context,0,'I',"Mail sent from %s to %s - %s%s",from,to,topic,tweet);
  return(0);
  }
