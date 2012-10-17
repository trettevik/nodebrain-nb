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
* File:     nbvli.h 
*
* Title:    Very Large Integer Routines
*
* Function:
*
*   This header provides routines that implement NodeBrain "very large
*   integer" arithmetic (up to 1 million bits).  These routines are intended
*   for encryption algorithms requiring integer arithmetic on numbers larger
*   than the maximimum supported by the host machine.  These routines are used
*   for public key encryption.   
*
* Synopsis:
*
*   #include "nb.h"
*
*   void vlicopy(vli x, vli y);
*   void vligetb(vli x, unsigned char *b, unsigned int l);
*   void vliputb(vli x, unsigned char *b, unsigned int l);
*   void vligetd(vli x, unsigned char *s);
*   void vliputd(vli x, unsigned char *s);
*   void vligetx(vli x, unsigned char *s);
*   void vliputx(vli x, unsigned char *s);
*
*   void vlihlf(vli x);
*   void vlisqr(vli x, vli p);
*   void vliadd(vli x, vli y);
*   void vlisub(vli x, vli y);
*   void vlimul(vli x, vli y, vli p);
*   void vlimod(vli x, vli m);
*   void vlidiv(vli x, vli y, vli q);
*   void vlipow(vli x, vli m, vli e);
*
*   void vlirand(vli x, unsigned int i);
*   void vlipprime(vli x);
*   void vlirprime(vli x);
*
* Description
*
*   The vli format is variable length.  The first word contains the length
*   in words.  The words are in reverse order of significance.
*
*      [word_count][least_significant_word]....[most_significant_word]
*
*   The word size for this implementation is 16 bits (unsigned short).  To
*   avoid dependence on this implementation, you should use the data types
*   defined in this header.  These types are based on bit size, independent
*   of the word size.
*
*      vli, vli0, vli8, vli16, vli32, vli64, vli128, vli512, vli1024, vli2048
*
*   The data types ending with numbers are defined as arrays, while vli is
*   a simple pointer.  However, all are treated as pointers and can be passed
*   to functions without the use of "&" in the call argument, or "*" in the
*   function parameter.  This is illustrated by the following sample code.
* 
*      void times25(x,p) vli x,p; {   
*        vli8 y;
*        vligetd(y,"25");   // assign value 25 to "not so vli" y         
*        vlimul(x,y,p);     // multiply vli x of unknown size by 25 
*        }
*
*      void main(){
*        vli2048 c,p;
*        vligetd(c,"234563456756786342");   // get vli from decimal string
*        times25(c,p);
*        }      
*
*   To allocate space for a very very large integer, you may use the malloc
*   function as shown below.
*
*            
*   In this description, x, y, m, p, q, and e are vli numbers, s is a standard
*   null delimited character string, b is an unsigned char array, and l is
*   an unsigned integer.
*
*      Assignment functions:
*
*        vlicopy(x,y)    - Copy vli y to vli x.
*        vligeti(x,l)    - Convert the integer l to vli x.
*        vligetb(x,b,l)  - Convert the byte array b of length l to vli x.     
*        vligetd(x,s)    - Convert the decimal string s to vli x.
*        vligetx(x,s)    - Convert the hexidecmal string s to vli x.     
*        vliputb(x,b,l)  - Convert the vli x to byte array b of length l.
*        vliputd(x,s)    - Convert the vli x to decimal string s.
*        vliputx(x,s)    - Convert the vli x to hexidecimal string s.
*
*      Arithmetic functions:
*
*        vlihlf(x)       - x=x/2
*        vliinc(x)       - x++
*        vlidec(x)       - x--
*        vlisqr(x,p)     - p=x^2
*
*        vliadd(x,y)     - x+=y
*        vlisub(x,y)     - x-=y
*
*        vlimul(x,y,p)   - p=x*y
*        vlimod(x,m)     - x=x%m
*        vlidiv(x,y,q)   - x=x%y  If q!=NULL then q=floor(x/m)
*        vlipow(x,m,e)   - x=(x^e)%m
*
*        vlirand(x,l)    - x is set to an l bit random number.
*        vlipprime(x)    - x is set to the next "probable" prime.
*        vlirprime(x,y)  - x is set to the next relative prime to y.
*
*     Other functions:
*     
*        vliprint(x,s)   - Print x in hex using label s.  This is intended for
*                          debugging only.
*
*        vlinew(l)       - Allocate vli with l bits and return address.
*        vlistr(l)       - Allocate string to hold decimal character
*                          representation of l bit vli.
*        vlisize(x)      - Returns the number of bytes required to hold a vli
*                          value as returned by vligetb.
*        vlibits(x)      - Return the number of used bits.
*        vlibytes(x)     - Return the number of used bytes.
*
*
*   
* Exit Codes:  none
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2000-03-16 Ed Trettevik (original prototype version)
* 2000-05-28 eat - version 0.2
*             1) Performance enhancements
*             2) Changed word size from 8 bits to 16 bits.  Provided new 
*                data types to reduces calling programs dependence on the
*                structure.
*             3) Renamed some of the functions.
*             4) Created additional functions for converting to and from
*                binary and hexidecimal integer representations.
*             5) Included functions (e.g. vlipprime, vlirprime) used by the pke
*                routines which may have other applications.
* 2001-03-04 eat - version 0.2.4
*             1) Improved performance of vlimod function to make larger
*                key sizes practical for the current authentication scheme
*                which requires authentication on every command.
*             2) Increased the size of temporary vli variables implemented
*                in the stack since larger sizes are now expected to be
*                more common.  (Changed from 512 bit to 2048 bit.)
*             3) The old vlimod function has been renamed vlidiv, and is
*                still used when a quotient is needed.  A quotient is not
*                computed by vlimod to reduce overhead.  However, the vlidiv
*                function should now be modified for efficiency based on the
*                approach used in vlimod, or perhaps vlimod should be modified
*                to support an optional quotient if it can be done efficiently.
* 2005-06-09 eat 0.6.3  Change long to int for 64 bit machines
* 2008-11-11 eat 0.7.3  Changed failure exit code to NB_EXITCODE_FAIL
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-02-26 eat 0.7.9  Fixed bug in vlidiv - not yet tested
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2012-02-07 dtl Checker updates
* 2012-06-16 eat 0.8.10 Replaced rand with random
* 2012-08-31 dtl 0.8.12 handled error
* 2012-12-13 eat 0.8.12 Replaced exit with nbExit
* 2012-12-13 eat 0.8.12 Replace printf with fprintf(stderr,...) in case we are a servant
*=============================================================================
*/
#include "nb.h"
#include "nbvli.h"
#include "nbrand.h"

/*
*  Print vli x (for debugging only)
*/
void vliprint(vliWord *x,char *label){
  vliWord *cx,*ex=x+*x;    /* cursors */

  if(*x==0){
    fprintf(stderr,"%s=[0];\n",label);
    return;
    }
  fprintf(stderr,"%s=[%u]",label,*x);
  for(cx=x+1;cx<ex;cx++){
    fprintf(stderr,"%4.4x.",*cx);
    }
  fprintf(stderr,"%4.4x;\n",*cx);
  }

/*
*  Allocate a vli large enough to hold an l bit number.
*/
vli vlinew(unsigned int l){
  unsigned int bytes;
  vliWord *x;
  
  bytes=l/8+2;
  if(l&15) bytes++;
  x=(vliWord *)malloc(bytes);
  if(!x) nbExit("vlinew: out of memory - terminating.");
  return(x);
  }

/*
*  Allocate a string large enough to hold a decimal representation of an l bit number
*/  
char *vlistr(unsigned int l){
  char *s;

  s=(char *)malloc(6+l/3);
  if(!s) nbExit("vlistr: out of memory - terminting.");
  return(s);
  }  
    
/*
*  Get size of a vli in bytes
*/
unsigned int vlisize(vliWord *x){
  return((unsigned int)*x*sizeof(vliWord));
  }

/*
*  Get size of a vli in used bytes.
*/
unsigned int vlibytes(vli x){
  
  if(*(x+*x)>0xff) return(*x*sizeof(vliWord));
  else return(*x*sizeof(vliWord)-1);
  }
  
/*
*  Get size of a vli in used bits.
*/
unsigned int vlibits(vli x){
  unsigned int i,m=0x8000;
  
  for(i=0;i<16 && (*(x+*x)&m)==0;i++) m=m>>1;
  return(*x*sizeof(vliWord)*8-i);
  }

/*
*  Make x a random number with l used bits.
*
*  The random number is from  2^(l-1) to 2^l-1
*
*  This function does not assume the work has been
*  initialized, and can not check the size of x.
*  The calling function is expected to ensure that
*  x can hold l bits, just as in other function calles
*  a character buffer must be as large a claimed in a
*  size_t length parameter.
*    
*/
void vlirand(vliWord *x,unsigned int l){
  unsigned int i;
  unsigned short m=0xffff;
  vliWord *ex;

  *x=l/16;
  if(l&15) (*x)++;
  ex=x+*x;  
  for(x=x+1;x<ex;x++) *x=nbRandom();
  if((i=l&15)!=0){            /* if 16 does not divide l */
    m=m>>(16-i);              /* mask for extra bits */
    *x=(nbRandom()&m)|((m>>1)+1); /* random extra bits with last bit forced on */ 
    }
  else *x=*x|0x8000;    
  }
    
/*
*  Assign vli x from y
*/
void vlicopy(vliWord *x,vliWord *y){
  memcpy(x,y,(size_t)((*y+1)*sizeof(vliWord)));
  }

/*
*  Make a vli number x from an unsigned int l
*
*/
void vligeti(vliWord *x,unsigned int l){
  unsigned int a=l;

  *(x+1)=a;
  a=a>>16;
  if(a){
    *x=2;
    *(x+2)=a;
    }
  else *x=1;
  }
 
/*
*  Divide a vli x by 2  (shift right 1 bit)
*/ 
void vlihlf(vliWord *x){
  unsigned int a;
  vliWord *cx=x+1,*ex=x+*x;

  a=*cx;
  a=a>>1;
  while(cx<ex){
    a=a|((unsigned int)(*(cx+1))<<15);
    *cx=a;
    a=a>>16;
    cx++;
    }
  if(a==0) *x=*x-1;  
  else *cx=a;  
  }
       
/*
*  Increment vli x by 1 
*/
void vliinc(vliWord *x){
//  unsigned long a=1;          /* accumulator */
  unsigned int a=1;          /* accumulator */
  vliWord *cx=x+1,*ex=x+*x+1; /* cursors */

  while(cx<ex){
    a+=*cx;
    *cx=(unsigned short)a;      /* assign low order 16 bits */
    if((a=a>>16)==0) return;  /* x needs no further modification */
    cx++;
    }  
  *cx=1;
  *x=cx-x;
  }
 
/*
*  Add vli y to x (result in x)
*/
void vliadd(vliWord *x,vliWord *y){
//  unsigned long a=0;          /* accumulator */
  unsigned int a=0;          /* accumulator */
  vliWord *cx=x+1,*cy=y+1,*ex=x+*x+1,*ey=y+*y+1,*eb=ex; /* cursors */

  if(*y<*x) eb=x+*y+1;
  while(cx<eb){
    a+=*cx+*cy;
    *cx=(unsigned short)a;      /* assign low order 16 bits */
    a=a>>16;    /* shift to get high order 16 bits */
    cx++;
    cy++;
    }
  while(cy<ey){
    a+=*cy;
    *cx=(unsigned short)a;
    a=a>>16;
    cx++;
    cy++;
    }
  while(cx<ex){
    a+=*cx;
    *cx=(unsigned short)a;
    if((a=a>>16)==0) return;  /* x needs no further modification */
    cx++;
    }  
  if(a){
    *cx=(unsigned short)a;
    cx++;
    }
  *x=cx-x-1;
/*  while(*x>0 && *(x+*x)==0) *x-=1; */  /* fix unnormalized vli values */
  }

/*
*  Decrement vli x (result in x)
*/
void vlidec(vliWord *x){
  vliWord *cx=x+1,*ex=x+*x+1;   /* cursors */

  if(*x==0) return;  /* don't dec zero value */
  while(cx<ex && *cx==0){
    *cx=0xffff;
    }
  if(cx==ex) nbExit("vlidec: value is not in normal form - terminating.");
  (*cx)--;
  if(*(x+*x)==0) *x-=1;  /* normalize vli x */
  }
   
/*
*  Subtract vli y from x (result in x)
*/
void vlisub(vliWord *x,vliWord *y){
  unsigned int a,m,b=0;  /* accumulator, minus value and borrow */
  vliWord *cx=x+1,*cy=y+1,*ex=x+*x+1,*ey=y+*y+1; /* cursors */

  if(*y>*x) nbExit("vlisub: y may not be greater than x - terminating.");
  while(cy<ey){
    a=*cx;
    m=*cy+b;
    if(a<m) b=1;
    else b=0;
    a-=m;         /* will automatically borrow if necessary */
    *cx=a;
    cx++;
    cy++;
    }
  if(b){
    if(cx==ex) nbExit("vlisub: y may not be greater than x - terminating.");
    (*cx)--;
    }
  else cx--;
  while(*x>0 && *(x+*x)==0) *x-=1;  /* normalize vli x */
  }
     
/*
*  Multiply vli x by y (result in p)
*
*/  
void vlimul(vliWord *x,vliWord *y,vliWord *p){
  vliWord *cx,*cy,*c=p+1,*cp;             /* vli product and cursors */
  vliWord *ex=x+*x+1,*ey=y+*y+1,*ep=p+*x+*y+1;        /* end cursors */
  unsigned int a=0,b;                       /* accumulator, register */
 
  for(cp=p+1;cp<ep;cp++) *cp=0;    /* initialize the product to zero */
  for(cx=x+1;cx<ex;cx++){          /* multiply word by word */
    if(*cx>0){                     /* if non-zero word */
      b=*cx;
      a=0;
      cp=c;
      for(cy=y+1;cy<ey;cy++){      
        a+=b*(unsigned int)*cy+*cp;
        *cp=a;
        a=a>>16;
        cp++;
        }
      *cp=a;      
      }      
    c++;
    }
  while(cp>p && *cp==0) cp--;      /* normalize product */
  *p=cp-p;
  }

/*
*  Square vli x (result in p)
*    This cuts off about 1/3 of the time for vlimul(x,x,p);
*/  
void vlisqr(vliWord *x,vliWord *p){
  vliWord *cx,*cy,*c=p+1,*cp;        /* vli product and cursors */
  vliWord *ex=x+*x+1,*ep=p+*x*2+1;   /* end cursors */
  unsigned int a=0,b,C;              /* accumulator, register, carry */

  for(cp=p+1;cp<ep;cp++) *cp=0; /* initialize the product to zero */
  for(cx=x+1;cx<ex;cx++){       /* multiply word by word */
    if(*cx>0){                  /* if non-zero word */
      b=*cx;
      b*=b;                     /* square first word */
      a=(b&0xffff)+*c;          /* add to current product */
      *c=a;
      a=(a>>16)+(b>>16);        /* carry */
      cp=c+1;
      for(cy=cx+1;cy<ex;cy++){
        b=*cx;
        b*=*cy;
        C=(b&0xffff0000)>>15;   /* product high word * 2 */
        a+=((b&0xffff)<<1)+*cp;    /* product low word *2 + running product + carry */ 
        *cp=a;                  
        a=(a>>16)+C;            /* new carry */
        cp++;
        }
      if(a>0){  
        a+=*cp;  
        *cp=a;
        while((a=a>>16)>0){
          cp++;
          a+=*cp;  
          *cp=a;
          }
        }
      }      
    c+=2;
    }
  while(cp>p && *cp==0) cp--;      /* drop leading zeros */
  *p=cp-p;
  }

/*
*  Mod function x=x mod n
*/
unsigned int vlimod(vli x,vli n){
  unsigned short xl=*x,nl=*n,*xc,*nc,*xh=x+*x,*nh=n+*n;
//  unsigned long a,b,ah,m,d=*(n+*n)+1,p,loop=0,w;
  unsigned int a,b,ah,m,d=*(n+*n)+1,p,loop=0,w;

  if(nl==0) nbExit("vlimod: zero modulus is invalid - terminating.");
  while(1){
    if(xl<nl) return(loop);
    /* compare high order part of x to n */
    /* warning: if conditions are order sensitive */
    nc=nh;
    for(xc=xh;xc>x && *xc==*nc;xc--) nc--;
    if(nc==n) xh=xc;            /* highpart(x)==n - chop off n*2^? */
    else if(xl==nl && *xc<*nc) return(loop); /* x<n - we're done */
    else if(xc==xh || *xc<*nc){   /* x>n, highword(x)!=highword(n) (not close) - compute multiplier */
      xc=xh;
      a=*xc;
      if(xl>nl && *xh<d){
        xc--;
        a=(a<<16)|*xc;
        w=0;
        }
      else w=1;
      m=a/d;  /* multiplier - the above test should ensure m<=64k, remember d=*nh+1 */
      if(m>1){           /* subtract n*m*2^? from x */
        nc=n+1;
        b=0;
        xc=x+w+(xl-nl);
        a=*xc;
        for(;xc<xh;xc++){
          ah=*(xc+1);
          if(ah<b){
            ah=0x10000-b;
            b=1;
            }
          else{
            ah-=b;
            b=0;
            }
          a=(ah<<16)|a;
          p=*nc*m;
          if(a<p) b++;
          a-=p;
          *xc=(short)a;
          a=a>>16;
          nc++;
          }
        if(w) a-=*nc*m;
        *xc=(short)a;
        }
      else{          /* subtract n*2^? from x */
        nc=n+1;
        b=0;
        for(xc=x+w+(xl-nl);xc<=xh;xc++){
          a=*xc;
          p=*nc+b;
          if(a<p) b=1;
          else b=0;
          a-=p;
          *xc=(short)a;
          nc++;
          }
        if(w==0) xh--;  /* we are borrowing 1 from 1 */  
        }
      }
    else{ /* x>n, highpart(x)<n, highwords(x)==highwords(n)- subtract up to equal part and chop equal part*/
      xh=xc;
      b=0;
      p=nc-n;
      p--;
      nc=n+1;
      for(xc-=p;xc<=xh;xc++){
        a=*xc;
        p=*nc+b;
        if(a<p) b=1;
        else b=0;
        a-=p;
        *xc=(short)a;
        nc++;
        }
      }
    while(xh>x && *xh==0) xh--;  /* normalize x */
    xl=xh-x;
    *x=xl;
    loop++;
    }
  }
  
/*
*  Divide vli x by modulus m (remainder in x, quotient in q)
*
*/  
  
unsigned int vlidiv(vliWord *x,vliWord *m,vliWord *q){
  vli2048 P2048,G2048;
  vliWord *P=P2048,*G=G2048,*cG,*eG,*cx,*cm;   /* product, guess and cursors */
  unsigned int nx,nm,rx,rm,f=0;
  double dx,dm;
  unsigned int loop=0;
  vliWord *debug;
   
  if(q!=NULL) *q=0;
  if(*x==0) return(loop);  /* special case when x is zero */
  if(*m==0) nbExit("vlidiv: zero modulus is invalid - terminating.");
  if(*x>256){
    /* Note: the +12 in the malloc below is crutching a bug that I have not
       been able to locate.  This should be removed and debugged when time
       is available.  Without the +12, for a particular 33 word value with
       a modulus of 10 (as in vliputd), a memory error causes a core dump
       when the area is freed via free(G).  One would assume that a logic
       error is causing an update beyond the end of g.  But dumping this
       area has not revealed the problem.
    */
    G=(vliWord *)malloc(2*(*x+1)*sizeof(vliWord)+300);
    if(!G) nbExit("vlidiv: out of memory - terminating.");
    P=G+((*x+1)*sizeof(vliWord));
    debug=P+((*x+1)*sizeof(vliWord));
    }
  nm=*m;
  while(1){ 
    nx=*x;
    /* return x if it is smaller than m */
    if(nx<nm){
      if(G!=G2048) free(G);
      return(loop);
      }
    eG=G+nx-nm+1;           /* assume rx and rm have equal number of words */
    cx=x+nx;
    cm=m+nm;
    rx=*cx;
    rm=*cm;
    if(nm==1){              /* length m = 1 */
      if(nx==1){            /* length x = 1 */
        if(rx<=rm){
          if(rx==rm){         /* x = m */
            *x=0;                    /* x is equal to m */  
            if(q!=NULL) vliinc(q);   /* inc quotient */
            }
          if(G!=G2048){
            //fprintf(stderr,"vlidiv debug=");
            //for(cx=debug;cx<debug+8;cx++) fprintf(stderr,"%4.4x",*debug);
            //fprintf(stderr,"\n");
            free(G);
            }
          return(loop);
          }
        else f=rx/rm;
        }    
      else{                 /* length x > 1 */
        cx--;
        rx=(rx<<16)+*cx;
        eG--;               /* shift down one guess digit */
        f=rx/rm;
        }        
      }
    else{  /* nx and nm are at least 2 */ 
      cx--;
      rx=(rx<<16)+*cx;
      cm--;
      rm=(rm<<16)+*cm;
      if(nx==nm){
        if(rx<rm){
          if(G!=G2048) free(G);
          return(loop);   /* x is less than m */
          }
        else if(rx==rm){          /* check for equal values */
          if(nm==2){                 /* x==m */
            *x=0;                    /* set x to zero */  
            if(q!=NULL) vliinc(q);   /* inc quotient */
            if(G!=G2048) free(G);
            return(loop);            /* remainder is zero (string length of zero) */
            }
          else{
            cx--;
            cm--;
            while(cm>m && *cx==*cm){cx--;cm--;}
            if(cm==m){                 /* if we compared all digits, x==m  */
              *x=0;                    /* set x to zero */  
              if(q!=NULL) vliinc(q);   /* inc quotient */
              if(G!=G2048) free(G);
              return(loop);            /* remainder is zero (string length of zero) */
              }
            else if(*cx<*cm){
              if(G!=G2048) free(G);
              return(loop);  /* x is less than m */
              }
            else f=1;                       /* factor of one */
            }
          }        
        else if(nm>2){
          f=rx/(rm+1);
          if(f==0) f=1;
          }
        else f=rx/rm;
        }  
      else{                   /* nx>nm */
        dm=rm;
        if(nm>2) dm++;        /* make sure we don't guess to high */
        cx--;
        dx=rx;
        dx*=0x10000;
        dx+=*cx;
        f=(unsigned int)(dx/dm);
        eG--;                 /* shift down one quess digit */
        if(f==0) f=1;         /* this shouldn't happen */
        }
      } 

    if(f==0){
      if(G!=G2048) free(G);
      return(loop);    /* factor is 0, x is smaller than m */
      }

    for(cG=G+1;cG<eG;cG++) *cG=0;
    *cG=f;
    if((f=f>>16)>0){
      // 2010-02-26 eat 0.7.9 - changed from *cg++ to cG++
      cG++;
      *cG=f;
      }
    *G=cG-G;
    if(*G==0) nbExit("vlidiv: len(x)=%d,len(m)=%d,*G=%d,rx=%d,rm=%d,f=rx/rm=%d - terminating\n",nx,nm,*G,rx,rm,f);
    if(q!=NULL) vliadd(q,G);    /* build quotient */
    if(*G==1 && *(G+1)==1){     /* avoid multipliplying by 1 */
      vlisub(x,m);
      }
    else{
      vlimul(G,m,P);            /* get product of m and guess */
      vlisub(x,P);              /* subtract from x */
      }
    loop++;
    }
  }

/* 
*  Raise a vli x to a power e modulo m.
*
*     x^(a*b) mod m = ((x^a mod m) * (x^b mod m)) mod m 
*/    
void vlipow(vliWord *x,vliWord *m,vliWord *e){
  vli2048 X2048,P2048;
  vliWord *X=X2048,*P=P2048,*ce=e+*e;
  unsigned int n;
  unsigned short b;

  if(*x>256){
    X=(vliWord *)malloc((size_t)((*x+1)*sizeof(vliWord))); 
    if(!X) nbExit("vlipow: out of memory - terminating.");
    }
  if(m!=NULL) n=*m*2;
  else{
    vligeti(X2048,*x);
    vlimul(X2048,e,P2048);
    if(*P2048>1){
      vliprint(P2048,"NB000E length(x)*e");
      nbExit("vlipow: exponent is too large for call without modulus - terminating.");
      } 
    n=*(P2048+1);   /* *x*e */  
    } 
  if(n>256){
    P=(vliWord *)malloc((size_t)((n+1)*sizeof(vliWord)));
    if(!P) nbExit("vlipow: out of memory - terminating.");
    }
  vlicopy(X,x);   /* make a copy of x for use by vlipower */    
  if(*e==0){      /* x^0=1 */
    vligeti(x,1);
    return;
    }
  else if(*e==1 && *(e+1)==1) return;     /* x^1=x */
  for(b=0x8000;b>0 && !(b&*ce);b=b>>1);   /* mask for highest used bit */ 
  if(b==0) nbExit("vlipow exponent is not normalized - terminating.");
  b=b>>1;
  for(;ce>e;ce--){  
    while(b>0){
      vlisqr(x,P);     /* square x for every power of 2 in e */
      vlicopy(x,P);
      if(m!=NULL) vlimod(x,m);     /* x^2(e/2) mod m */
      if(b&*ce){       /* if the masked bit is on */
        vlimul(x,X,P); /* multiply accumulator by x */
        vlicopy(x,P);
        if(m!=NULL) vlimod(x,m);   /* x^e mod m */ 
        }
      b=b>>1;  
      }
    b=0x8000;  
    }

  if(X!=X2048) free(X);        /* free allocated memory */
  if(P!=P2048) free(P);
  }

/*
*  Make a vli number x from a byte array
*
*    [least_significant_byte]...[most_significant_byte]
*/
void vligetb(vliWord *x,unsigned char *b,unsigned int l){
  vliWord a,*cx=x+1;
  unsigned char *eb=b+l-1;

  while(b<eb){      /* handle each word - flip bytes */
    a=*(b+1);
    a=(a<<8)|*b;
    *cx=a;
    b+=2;
    cx++;
    }
  if(b==eb){        /* handle odd byte */
    a=*b;
    *cx=a;
    cx++;
    }
  *x=cx-x-1;        /* set vli length */     
  }

/*
*  Convert a vli number x to a byte array
*
*    [least_significant_byte]...[most_significant_byte]
*/
void vliputb(vliWord *x,unsigned char *b,unsigned int l){
  vliWord a,*cx=x+1,*ex=x+*x+1;  /* accumulator and cursors */
  unsigned char *eb=b+l-1;       /* end of byte array */

  while(b<eb && cx<ex){  /* handle each word - flip bytes */
    a=*cx;
    *b=(unsigned char)a;
    *(b+1)=(a>>8);
    b+=2;
    cx++;
    }
  if(b==eb && cx<ex){    /* handle odd number of bytes */
    a=*cx;
    *b=(unsigned char)a;
    b++;
    }
  while(b<=eb){          /* pad byte array */
    *b=0;
    b++;
    }
  }
  
/*
*  Make a vli number x from a decimal number character string
*    This can be modified to use much larger powers of ten.
*    It should use several digits at a time.
*/
void vligetd(vliWord *x,unsigned char *s){
  vliWord *P,ten[2],digit[2];

  P=(vliWord *)malloc(4+strlen((char *)s)); /* this could be refined to use less space */
  if(!P) nbExit("vligetd: out of memory - terminating.");
  *ten=1;
  *(ten+1)=10;
  *digit=1;
  *x=1;
  *(x+1)=*s-'0';
  s++;
  while(*s>0){
    vlimul(x,ten,P);
    vlicopy(x,P);
    *(digit+1)=*s-'0';
    vliadd(x,digit);
    s++;
    }
  if(*x==1 && *(x+1)==0) *x=0; 
  free(P);   
  }

/*
*  Convert a vli to decimal character string 
*    This can be modified to use much larger powers of ten.
*    It should use several digits at a time.
*    "ssize" is sizeof string "s" 
*/
void vliputd(vliWord *x,unsigned char *s,size_t ssize){
  vliWord ten[2],*Q,*X;
  unsigned char *c,*ds,*es;
  int n;
  
  *ten=1;
  *(ten+1)=10;
  if(*x==0){
    if(ssize>1) *((char *)s)='0',*((char *)s+1)=0; //2012-02-07 dtl: replaced strcpy(s,"0")
    return;
    }
  ds=(unsigned char *)malloc((*x*5+1));
  if(!ds) nbExit("vliputd: out of memory - terminating.");
  es=ds+*x*5+1;
  Q=(vliWord *)malloc((*x+1)*sizeof(vliWord));
  if(!Q) nbExit("vliputd: out of memory - terminating.");
  X=(vliWord *)malloc((*x+1)*sizeof(vliWord));
  if(!X) nbExit("vliputd: out of memory - terminating.");
  vlicopy(X,x);
  *(es-1)=0;
  for(c=es-2;*X>0 && c>=ds;c--){
    vlidiv(X,ten,Q);
    if(*X==0) *c='0';
    else{
      if(*(X+1)>10) vliprint(X,"vliputd funny remainder");
      *c=*(X+1)+'0';
      }
    vlicopy(X,Q);
    }
  c++;
  if((n=strlen((char *)c))<ssize) strncpy((char *)s,(char *)c,n),*(s+n)=0; //2012-02-07 dtl: replaced strcpy
  else strncpy((char *)s,(char *)c,ssize),*(s+ssize-1)=0;          //dtl: copy upto ssize
  free(ds);
  free(Q);
  free(X);
  }

/*
*  Convert a hexidecimal string s to vli number x
*
*/
int vligetx(vliWord *x,unsigned char *s){
  vliWord a,b,*cx=x+1;     /* accumulator and cursor */
  unsigned short i;      /* nibble counter */

  while(*s!=0){
    a=0;
    for(i=0;i<4 && *s!=0;i++){
      if(*s>='0' && *s <='9') b=*s-'0';
      else if(*s>='a' && *s<='f') b=10+*s-'a';
      else return(-1);
      a=a|(b<<(4*i));
      s++;
      }
    *cx=a;
    cx++;
    }
  *x=cx-x-1;
  return(1);
  }  

/*
*  Convert a vli number x to a hexidecimal string
*  "s" must have size at least of 5 bytes  
*/
void vliputx(vliWord *x,char *s){
  vliWord a,*cx=x+1,*ex=x+*x+1;    /* accumulator and cursors */
  unsigned short i;                /* nibble counter */

  if(*x==0){
    sprintf(s,"0"); //2012-02-07 dtl: replaced strcpy (sizeof s defined 512)
    return;
    }
  while(cx<ex){          /* handle each word - reverse nibbles */
    a=*cx;
    for(i=0;i<4;i++){
      sprintf(s,"%1.1x",a&15); //print 1 byte to s (1 hex value)
      a=a>>4;                  //shift left 1 hex
      s++;                     //move to next byte
      }
    cx++;
    }
  while(*(s-1)=='0') s--;  /* drop trailing zeros */
  *s=0;                    /* null delimiter */  
  }

/*
*   vlipprime: Find the next "probable" prime.
*
*   Fermat's (Little) Theorem:  If p is a prime and a is an integer, then
*   a^p=a (mod p).  In particluar, if p does not divide a, then a^(p-1) = 1 (mod p).
*
*   The converse [if a^(p-1) = 1 mod p, then p is prime] is not true.  However, we
*   can use this theorem to test for probable primes.  In fact, if a number x, less than
*   341,550,071,728,321 passes this test for a=2,3,5,7,9,11,13 and 17, it is prime
*   [Jaeschke93]. Above this value, we are only making a good guess.    
*/
void vlipprime(vli x){
  unsigned int count=0,i,A[7];
  vli1024 a,e;
  
  A[0]=2;
  A[1]=3;
  A[2]=5;
  A[3]=7;
  A[4]=11;
  A[5]=13;
  A[6]=17;

  while(count<1000){
    vliinc(x);
    (*(x+*x))=*(x+*x)|1;
    vlicopy(e,x);
    vlidec(e);  
    for(i=0;i<7;i++){                    /* for a=2,3,5,7,11,13,17 */ 
      vligeti(a,A[i]); 
      vlipow(a,x,e);                     /* compute a^(x-1) mod x */
      if(*a!=1 || *(a+1)!=1) break;
      }    
    if(i==7) return;
    }
  nbExit("vlipprime exceeded iteration limit - terminating.");
  }

/*
*  vlirprime: Find the next relative prime.
*/
void vlirprime(vli x,vli y){
  vli2048 a,b,r;  

  vliinc(x);
  while(*x<*y || *(x+*x)<*(y+*y)){
    vlicopy(a,y);
    vlicopy(b,x);
    while(*a>1 || (*b==1 && *(b+1)>1)){
      vlicopy(r,a);
      vlimod(r,b);     /* r=mod(a/b) */
      vlicopy(a,b);    /* a=b */
      vlicopy(b,r);    /* b=r */
      }
    if(*b==1 && *(b+1)==1) return;
    vliinc(x);
    }
  }
