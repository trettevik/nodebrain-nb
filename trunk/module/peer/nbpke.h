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
void pkePrint(unsigned char *ciphertext);
unsigned int pkeCipher(unsigned char *ciphertext,vli exponent,vli modulus);
unsigned int pkeEncrypt(unsigned char *ciphertext,vli exponent,vli modulus,unsigned char *plaintext,unsigned int length);
unsigned int pkeDecrypt(unsigned char *ciphertext,vli exponent,vli modulus,unsigned char *plaintext,unsigned int length);
void pkeGenKey(unsigned int l,vli e,vli n,vli d);

void pkegetj(vli j,vli x,vli y);
void pkegetk(vli k,vli x,vli y);
void pkeTestKey(int c,vli e,vli n,vli d);
