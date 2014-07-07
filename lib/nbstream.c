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
* File:     nbstream.c
*
* Title:    NodeBrain Event Stream Routines
*
* Function:
*
*   This file provides routines that implement NodeBrain event streams.
*
* Synospis:
*
*   #include "nb.h"
*
*   void nbStreamInit();
*   NB_Cell *nbStreamProducerOpen();
*   int nbStreamMsg(NB_Cell *producer,char *msg);

*   int nbStreamOpen(NB_Cell *context,char *stream,void *session,void (*subscriber)());
*   int nbStreamClose(NB_Cell *context,char *stream,void *session,void (*subscriber)());
*
* Description
*
*   A stream is a simple mechanism for passing messages from publishers to
*   subscribers.  A publisher doesn't know who has subscribed, only that
*   someone has.  For a given stream there can be multiple publishers and 
*   multiple subscribers.  Subscribers can open and close subscriptions.  A
*   publisher is notified when the subscriber count toggles between zero and
*   non-zero.  A publisher may use knowledge of this state to avoid unnecessary
*   overhead to publish when there are no subscribers.
*
*   There are currently three attributes associated with a subscription.
*
*     stream     - name known to publisher and subscriber
*     session    - handle provided when a subscription is opened
*     subscriber - message handler function
*  
*   Subscriptions may be canceled by:
*
*     subscriber
*     subscriber and session
*     subscriber, session and stream
*
* Return Codes: see individual functions
* 
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2004-12-02 Ed Trettevik (original prototype version)
* 2005-04-07 eat 0.6.2  Enhanced nbStreamClose()
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
* 2012-01-16 dtl Checker updates
* 2012-10-13 eat 0.8.12 Replaced malloc with nbAlloc
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

NB_Type   *nb_StreamType;  /* stream type description */
NB_Stream *nb_StreamFree;  /* free stream structure */
NB_Stream *nb_StreamList;  /* list of streams */

NB_Type           *nb_StreamProducerType;
NB_StreamProducer *nb_StreamProducerFree;

NB_StreamSubscription *nb_StreamSubscriptionFree;

/*
*  Stream Functions
*/        
void nbStreamPrint(NB_Stream *stream){
  outPut("Stream ");
  printObject((NB_Object *)stream);
  } 

void nbStreamDestroy(NB_Stream *Stream){
  /* stub */
  }  
    
/*
*  Initialize Stream object type
*/
void nbStreamInit(NB_Stem *stem){
  nb_StreamFree=NULL;
  nb_StreamList=NULL;
  nb_StreamType=nbObjectType(stem,"Stream",0,0,nbStreamPrint,nbStreamDestroy);
  nb_StreamProducerFree=NULL;
  nb_StreamProducerType=nbObjectType(stem,"StreamProducer",0,0,NULL,NULL);
  nb_StreamSubscriptionFree=NULL;
  }

/* 
*  API
*/

/*
*  Open a stream for output
*/

void *nbStreamProducerOpen(
  NB_Cell *context,
  char *streamName,
  void *handle,
  void (*handler)(NB_Cell *context,void *handle,char *topic,int state)){
  
  NB_Stream *stream;
  NB_StreamProducer *producer;

  for(stream=nb_StreamList;stream!=NULL && strcmp(stream->name->value,streamName)!=0;stream=(NB_Stream *)stream->object.next);
  if(stream==NULL){  
    stream=(NB_Stream *)newObject(nb_StreamType,(void **)&nb_StreamFree,sizeof(NB_Stream));
    stream->name=grabObject(useString(streamName));
    stream->producer=NULL;
    stream->sub=NULL;
    stream->object.next=(NB_Object *)nb_StreamList;
    nb_StreamList=stream;
    outMsg(0,'T',"Stream \"%s\" registered",streamName);
    }
  for(producer=stream->producer;producer!=NULL && (producer->handle!=handle || producer->handler!=handler);producer=(NB_StreamProducer *)producer->object.next);
  if(producer==NULL){
    producer=(NB_StreamProducer *)newObject(nb_StreamProducerType,(void **)&nb_StreamProducerFree,sizeof(NB_StreamProducer));
    producer->stream=stream;
    producer->handle=handle;
    producer->handler=handler;
    producer->object.next=(NB_Object *)stream->producer;
    stream->producer=producer;
    }
  return((void *)producer);
  }

/*
*  Send a message to a stream
*
*  For now we are treating a message as a single character string.  In the future we may
*  expand this to an array of objects.  This will require a bit more translation when
*  sending over the network, but will enable nodes to subscribe to streams for assertions
*  without parsing.
*
*  define fred node cache(a,b,c);
*
*  fred. define joe tap <stream>(<term1>,<term2>,<term3>) where(<condition>) on(<condition>);
*
*  In the long run, we may want a stream to be a feature of any node and a subscription
*  to be specified as a cell expression in addition to explicit taps as shown above.
*/
void nbStreamPublish(NB_Cell *producer,char *msg){   
  NB_StreamSubscription *sub;

  /* outMsg(0,'T',"nbStreamMsg called"); */
  for(sub=((NB_StreamProducer *)producer)->stream->sub;sub!=NULL;sub=sub->next){ 
    outMsg(0,'T',"nbStreamMsg calling sub");
    (*sub->subscriber)(producer,sub->session,msg);
    }
  }

/*
*  Open a stream subscription
*
*  At least one producer must have the stream open for output
*
*  Returns: 
*    0 - no such stream
*    1 - subscription opened
*/
int nbStreamOpen(NB_Cell *context,char *streamName,void *session,void (*subscriber)(NB_Cell *context,void *handle,char *msg)){
  NB_Stream *stream;
  NB_StreamSubscription *sub;

  for(stream=nb_StreamList;stream!=NULL && strcmp(stream->name->value,streamName)!=0;stream=(NB_Stream *)stream->object.next);
  if(stream==NULL) return(0); 
  for(sub=stream->sub;sub!=NULL && (sub->session!=session || sub->subscriber!=subscriber);sub=sub->next);
  if(sub==NULL){
    // 2012-10-13 eat - replaced malloc
    //if((sub=(NB_StreamSubscription *)malloc(sizeof(NB_StreamSubscription)))==NULL) return(0); //dlt: add chk
    sub=(NB_StreamSubscription *)nbAlloc(sizeof(NB_StreamSubscription));
    sub->stream=stream;
    sub->session=session;
    sub->subscriber=subscriber;
    sub->next=stream->sub;
    stream->sub=sub;
    }
  return(1);
  }

/* 
*  Close a subscription for a given stream
*    This is a support function, not an API function
*/
int nbStreamCloseSub(NB_Stream *stream,void *session,void (*subscriber)(NB_Cell *context,void *session,char *data)){
  NB_StreamSubscription *sub,**subP;
  int count=0;

  subP=&stream->sub;
  for(sub=*subP;sub!=NULL && (sub->session!=session || sub->subscriber!=subscriber);sub=*subP) subP=&sub->next;
  for(sub=*subP;sub!=NULL;sub=*subP){
    if(sub->subscriber==subscriber && (session==NULL || sub->session==session)){
      *subP=sub->next;
      sub->next=nb_StreamSubscriptionFree;
      nb_StreamSubscriptionFree=sub;
      count++;
      }
    subP=&sub->next;
    }
  return(count);
  }

/*
*  Close stream subscription(s) 
*
*    streamName  session  subscriber
*    ----------  -------  ----------
*     !NULL      !NULL     !NULL   Close specific subscription
*      NULL      !NULL     !NULL   Close all subscriptions for session 
*      NULL       NULL     !NULL   Close all subscriptions for subscriber
*
*  Returns number of subscriptions closed
*
*/
int nbStreamClose(NB_Cell *context,char *streamName,void *session,void (*subscriber)(NB_Cell *context,void *handle,char *msg)){
  NB_Stream *stream;
  int count=0;

  if(streamName==NULL){   /* cancel subs to all streams */
    for(stream=nb_StreamList;stream!=NULL;stream=(NB_Stream *)stream->object.next){
       count+=nbStreamCloseSub(stream,session,subscriber);
       }
    }
  else{
    outMsg(0,'T',"removing sub from stream \"%s\"",streamName);
    for(stream=nb_StreamList;stream!=NULL && strcmp(stream->name->value,streamName)!=0;stream=(NB_Stream *)stream->object.next);
    if(stream==NULL) return(-1);
    count+=nbStreamCloseSub(stream,session,subscriber);
    }
  return(count);
  }
