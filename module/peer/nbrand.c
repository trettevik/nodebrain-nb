/*
* Copyright (C) 2012-2013 The Boeing Company
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

