/* 
 * md4.c -- Implementation of MD4 Message Digest Algorithm
 * Updated: 2/16/90 by Ronald L. Rivest
 *
 * Portability nits fixed and reformatted - 2/12/91 Phil Karn
 *
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 * 
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 * 
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 * 
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/* 
 * To use MD4:
 *   -- Include md4.h in your program
 *   -- Declare an MDstruct MD to hold the state of the digest computation.
 *   -- Initialize MD using MDbegin(&MD)
 *   -- For each full block (64 bytes) X you wish to process, call
 *          MDupdate(&MD,X,512)
 *      (512 is the number of bits in a full block.)
 *   -- For the last block (less than 64 bytes) you wish to process,
 *          MDupdate(&MD,X,n)
 *      where n is the number of bits in the partial block. A partial
 *      block terminates the computation, so every MD computation should
 *      terminate by processing a partial block, even if it has n = 0.
 *   -- The message digest is available in MD.buffer[0] ... MD.buffer[3].
 *      (Least-significant byte of each word should be output first.)
 *   -- You can print out the digest using MDprint(&MD)
 */

/* Implementation notes:
 * This implementation assumes that longs are 32-bit quantities.
 * If the machine stores the least-significant byte of an long in the
 * least-addressed byte (eg., VAX and 8086), then LOWBYTEFIRST should be
 * set to TRUE.  Otherwise (eg., SUNS), LOWBYTEFIRST should be set to
 * FALSE.  Note that on machines with LOWBYTEFIRST FALSE the routine
 * MDupdate modifies has a side-effect on its input array (the order of bytes
 * in each word are reversed).  If this is undesired a call to MDreverse(X) can
 * reverse the bytes of X back into order after each call to MDupdate.
 */
#define TRUE  1
#define FALSE 0

#if (defined(__MSDOS__) || defined(MPU8086) || defined(MPU8080) \
 || defined(vax) || defined (MIPSEL))
#define LOWBYTEFIRST TRUE	/* Low order bytes are first in memory */
#else			/* Almost all other machines are big-endian */
#define	LOWBYTEFIRST FALSE
#endif


/* Compile-time includes */
#include <stdio.h>
#include "md4.h"

/* Compile-time declarations of MD4 ``magic constants'' */
#define I0  0x67452301       /* Initial values for MD buffer */
#define I1  0xefcdab89
#define I2  0x98badcfe
#define I3  0x10325476
#define C2  013240474631     /* round 2 constant = sqrt(2) in octal */
#define C3  015666365641     /* round 3 constant = sqrt(3) in octal */
/* C2 and C3 are from Knuth, The Art of Programming, Volume 2
 * (Seminumerical Algorithms), Second Edition (1981), Addison-Wesley.
 * Table 2, page 660.
 */
#define fs1  3               /* round 1 shift amounts */
#define fs2  7   
#define fs3 11  
#define fs4 19  
#define gs1  3               /* round 2 shift amounts */
#define gs2  5   
#define gs3  9   
#define gs4 13  
#define hs1  3               /* round 3 shift amounts */
#define hs2  9 
#define hs3 11 
#define hs4 15


/* Compile-time macro declarations for MD4.
 * Note: The ``rot'' operator uses the variable ``tmp''.
 * It assumes tmp is declared as unsigned long, so that the >>
 * operator will shift in zeros rather than extending the sign bit.
 */
#define	f(X,Y,Z)             ((X&Y) | ((~X)&Z))
#define	g(X,Y,Z)             ((X&Y) | (X&Z) | (Y&Z))
#define h(X,Y,Z)             (X^Y^Z)
#define rot(X,S)             (tmp=X,(tmp<<S) | (tmp>>(32-S)))
#define ff(A,B,C,D,i,s)      A = rot((A + f(B,C,D) + X[i]),s)
#define gg(A,B,C,D,i,s)      A = rot((A + g(B,C,D) + X[i] + C2),s)
#define hh(A,B,C,D,i,s)      A = rot((A + h(B,C,D) + X[i] + C3),s)

void MDreverse __ARGS((unsigned long *X));

/* MDprint(MDp)
 * Print message digest buffer MDp as 32 hexadecimal digits.
 * Order is from low-order byte of buffer[0] to high-order byte of buffer[3].
 * Each byte is printed with high-order hexadecimal digit first.
 * This is a user-callable routine.
 */
void 
MDprint(MDp)
MDptr MDp;
{
	int i,j;

	for(i=0;i<4;i++)
		for(j=0;j<32;j=j+8)
			printf("%02lx",(MDp->buffer[i]>>j) & 0xFF);
}

/* MDbegin(MDp)
 * Initialize message digest buffer MDp. 
 * This is a user-callable routine.
 */
void 
MDbegin(MDp)
MDptr MDp;
{
	int i;

	MDp->buffer[0] = I0;  
	MDp->buffer[1] = I1;  
	MDp->buffer[2] = I2;  
	MDp->buffer[3] = I3; 
	for(i=0;i<8;i++)
		MDp->count[i] = 0;
	MDp->done = 0;
}

/* MDreverse(X)
 * Reverse the byte-ordering of every long in X.
 * Assumes X is an array of 16 longs.
 * The macro revx reverses the byte-ordering of the next word of X.
 */
#define revx { t = (*X << 16) | (*X >> 16); \
	       *X++ = ((t & 0xFF00FF00) >> 8) | ((t & 0x00FF00FF) << 8); }
void
MDreverse(X)
unsigned long *X;
{
	register unsigned long t;

	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
	revx;
}

/* MDblock(MDp,X)
 * Update message digest buffer MDp->buffer using 16-word data block X.
 * Assumes all 16 words of X are full of data.
 * Does not update MDp->count.
 * This routine is not user-callable. 
 */
static void
MDblock(MDp,X)
MDptr MDp;
unsigned long *X;
{ 
	register unsigned long tmp, A, B, C, D;

#if LOWBYTEFIRST == FALSE
	MDreverse(X);
#endif
	A = MDp->buffer[0];
	B = MDp->buffer[1];
	C = MDp->buffer[2];
	D = MDp->buffer[3];
	/* Update the message digest buffer */
	ff(A,B,C,D,0,fs1); /* Round 1 */
	ff(D,A,B,C,1,fs2); 
	ff(C,D,A,B,2,fs3); 
	ff(B,C,D,A,3,fs4); 
	ff(A,B,C,D,4,fs1); 
	ff(D,A,B,C,5,fs2); 
	ff(C,D,A,B,6,fs3); 
	ff(B,C,D,A,7,fs4); 
	ff(A,B,C,D,8,fs1); 
	ff(D,A,B,C,9,fs2); 
	ff(C,D,A,B,10,fs3); 
	ff(B,C,D,A,11,fs4); 
	ff(A,B,C,D,12,fs1); 
	ff(D,A,B,C,13,fs2); 
	ff(C,D,A,B,14,fs3); 
	ff(B,C,D,A,15,fs4); 
	gg(A,B,C,D,0,gs1); /* Round 2 */
	gg(D,A,B,C,4,gs2); 
	gg(C,D,A,B,8,gs3); 
	gg(B,C,D,A,12,gs4); 
	gg(A,B,C,D,1,gs1); 
	gg(D,A,B,C,5,gs2); 
	gg(C,D,A,B,9,gs3); 
	gg(B,C,D,A,13,gs4); 
	gg(A,B,C,D,2,gs1); 
	gg(D,A,B,C,6,gs2); 
	gg(C,D,A,B,10,gs3); 
	gg(B,C,D,A,14,gs4); 
	gg(A,B,C,D,3,gs1); 
	gg(D,A,B,C,7,gs2); 
	gg(C,D,A,B,11,gs3); 
	gg(B,C,D,A,15,gs4);  
	hh(A,B,C,D,0,hs1); /* Round 3 */
	hh(D,A,B,C,8,hs2); 
	hh(C,D,A,B,4,hs3); 
	hh(B,C,D,A,12,hs4); 
	hh(A,B,C,D,2,hs1); 
	hh(D,A,B,C,10,hs2); 
	hh(C,D,A,B,6,hs3); 
	hh(B,C,D,A,14,hs4); 
	hh(A,B,C,D,1,hs1); 
	hh(D,A,B,C,9,hs2); 
	hh(C,D,A,B,5,hs3); 
	hh(B,C,D,A,13,hs4); 
	hh(A,B,C,D,3,hs1); 
	hh(D,A,B,C,11,hs2); 
	hh(C,D,A,B,7,hs3); 
	hh(B,C,D,A,15,hs4);
	MDp->buffer[0] += A; 
	MDp->buffer[1] += B;
	MDp->buffer[2] += C;
	MDp->buffer[3] += D; 
}

/* MDupdate(MDp,X,count)
 * Input: MDp -- an MDptr
 *        X -- a pointer to an array of unsigned characters.
 *        count -- the number of bits of X to use.
 *                 (if not a multiple of 8, uses high bits of last byte.)
 * Update MDp using the number of bits of X given by count.
 * This is the basic input routine for an MD4 user.
 * The routine completes the MD computation when count < 512, so
 * every MD computation should end with one call to MDupdate with a
 * count less than 512.  A call with count 0 will be ignored if the
 * MD has already been terminated (done != 0), so an extra call with count
 * 0 can be given as a ``courtesy close'' to force termination if desired.
 */
void 
MDupdate(MDp,X,count)
MDptr MDp;
unsigned char *X;
unsigned int count;
{
	int i,bit,byte,mask;
	unsigned long tmp;
	unsigned char XX[64];
	unsigned char *p;

	/* return with no error if this is a courtesy close with count
	 * zero and MDp->done is true.
	 */
	if(count == 0 && MDp->done)
		return;
	/* check to see if MD is already done and report error */
	if(MDp->done){
		printf("\nError: MDupdate MD already done.");
		return;
	}
	/* Add count to MDp->count */
	tmp = count;
	p = MDp->count;
	while(tmp){
		tmp += *p;
		*p++ = tmp;
		tmp = tmp >> 8;
	}
	/* Process data */
	if(count == 512){
		/* Full block of data to handle */
		MDblock(MDp,(unsigned long *)X);
	} else if(count > 512){
		/* Check for count too large */
		printf("\nError: MDupdate called with illegal count value %ld.",count);
		return;
	} else {
		/* partial block -- must be last block so finish up
		 * Find out how many bytes and residual bits there are
		 */
		byte = count >> 3;
		bit =  count & 7;
		/* Copy X into XX since we need to modify it */
		for(i=0;i<=byte;i++)
			XX[i] = X[i];
		for(i=byte+1;i<64;i++)
			XX[i] = 0;
		/* Add padding '1' bit and low-order zeros in last byte */
		mask = 1 << (7 - bit);
		XX[byte] = (XX[byte] | mask) & ~( mask - 1);
		/* If room for bit count, finish up with this block */
		if(byte <= 55){
			for(i=0;i<8;i++)
				XX[56+i] = MDp->count[i];
			MDblock(MDp,(unsigned long *)XX);
		} else {
			/* need to do two blocks to finish up */
			MDblock(MDp,(unsigned long *)XX);
			for(i=0;i<56;i++)
				XX[i] = 0;
			for(i=0;i<8;i++)
				XX[56+i] = MDp->count[i];
			MDblock(MDp,(unsigned long *)XX);
		}
	/* Set flag saying we're done with MD computation */
	MDp->done = 1;
	}
}
/* End of md4.c */
