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
* File:     nbrand.c
*
* Title:    Random Number Functions
*
* Function:
*
*   Random number generation functions that use the OpenSSL library.
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2012-10-16 Ed Trettevik - Introduced in 0.8.12
*=============================================================================
*/
#include <nb/nb.h>
#include <openssl/rand.h>

long int nbRandom(void){
  long int randy;
  unsigned char byte[sizeof(long int)];
  int i;

  if(!RAND_bytes(byte,sizeof(long int))){
    fprintf(stderr,"OpenSSL random number generator not properly seeded - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  randy=byte[0]%128;
  for(i=1;i<sizeof(long int);i++) randy=(randy<<8)|byte[i];
  return(randy);
  }

uint16_t nbRand16(void){
  uint16_t randy;
  unsigned char byte[2];

  if(!RAND_bytes(byte,2)){
    fprintf(stderr,"OpenSSL random number generator not properly seeded - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  randy=(byte[0]<<8)|byte[1];
  return(randy);
  }

uint32_t nbRand32(void){
  uint32_t randy;
  unsigned char byte[4];

  if(!RAND_bytes(byte,4)){
    fprintf(stderr,"OpenSSL random number generator not properly seeded - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  randy=(((byte[0]<<8)|byte[1])<<8|byte[2])<<8|byte[3];
  return(randy);
  }

