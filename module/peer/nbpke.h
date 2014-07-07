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
* Program:  NodeBrain
*
* File:     nbpke.h 
* 
* Title:    Public Key Encryption Header
*
* Function:
*
*   This header defines routines used for public key encryption.
*
* See nbpke.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-15 eat 0.5.1  Created to conform to new makefile
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*=============================================================================
*/
//void pkePrint(unsigned char *ciphertext);
//unsigned int pkeCipher(unsigned char *ciphertext,vli exponent,vli modulus);
unsigned int pkeEncrypt(unsigned char *ciphertext,vli exponent,vli modulus,unsigned char *plaintext,unsigned int length);
unsigned int pkeDecrypt(unsigned char *ciphertext,vli exponent,vli modulus,unsigned char *plaintext,unsigned int length);
void pkeGenKey(unsigned int l,vli e,vli n,vli d);

//static void pkegetj(vli j,vli x,vli y);
//static void pkegetk(vli k,vli x,vli y);
//void pkeTestKey(int c,vli e,vli n,vli d);
