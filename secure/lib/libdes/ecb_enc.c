/* lib/des/ecb_enc.c */
/* Copyright (C) 1995 Eric Young (eay@mincom.oz.au)
 * All rights reserved.
 * 
 * This file is part of an SSL implementation written
 * by Eric Young (eay@mincom.oz.au).
 * The implementation was written so as to conform with Netscapes SSL
 * specification.  This library and applications are
 * FREE FOR COMMERCIAL AND NON-COMMERCIAL USE
 * as long as the following conditions are aheared to.
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.  If this code is used in a product,
 * Eric Young should be given attribution as the author of the parts used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Eric Young (eay@mincom.oz.au)
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "des_locl.h"
#include "spr.h"

const char *DES_version="libdes v 3.22 - 95/11/29 - eay";

void des_ecb_encrypt(input, output, ks, encrypt)
des_cblock (*input);
des_cblock (*output);
des_key_schedule ks;
int encrypt;
	{
	register unsigned long l0,l1;
	register unsigned char *in,*out;
	unsigned long ll[2];

	in=(unsigned char *)input;
	out=(unsigned char *)output;
	c2l(in,l0); ll[0]=l0;
	c2l(in,l1); ll[1]=l1;
	des_encrypt(ll,ks,encrypt);
	l0=ll[0]; l2c(l0,out);
	l1=ll[1]; l2c(l1,out);
	l0=l1=ll[0]=ll[1]=0;
	}

void des_encrypt(data, ks, encrypt)
unsigned long *data;
des_key_schedule ks;
int encrypt;
	{
	register unsigned long l,r,t,u;
#ifdef DES_USE_PTR
	register unsigned char *des_SP=(unsigned char *)des_SPtrans;
#endif
#ifdef MSDOS
	union fudge {
		unsigned long  l;
		unsigned short s[2];
		unsigned char  c[4];
		} U,T;
#endif
	register int i;
	register unsigned long *s;

	u=data[0];
	r=data[1];

	IP(u,r);
	/* Things have been modified so that the initial rotate is
	 * done outside the loop.  This required the
	 * des_SPtrans values in sp.h to be rotated 1 bit to the right.
	 * One perl script later and things have a 5% speed up on a sparc2.
	 * Thanks to Richard Outerbridge <71755.204@CompuServe.COM>
	 * for pointing this out. */
	l=(r<<1)|(r>>31);
	r=(u<<1)|(u>>31);

	/* clear the top bits on machines with 8byte longs */
	l&=0xffffffffL;
	r&=0xffffffffL;

	s=(unsigned long *)ks;
	/* I don't know if it is worth the effort of loop unrolling the
	 * inner loop */
	if (encrypt)
		{
		for (i=0; i<32; i+=4)
			{
			D_ENCRYPT(l,r,i+0); /*  1 */
			D_ENCRYPT(r,l,i+2); /*  2 */
			}
		}
	else
		{
		for (i=30; i>0; i-=4)
			{
			D_ENCRYPT(l,r,i-0); /* 16 */
			D_ENCRYPT(r,l,i-2); /* 15 */
			}
		}
	l=(l>>1)|(l<<31);
	r=(r>>1)|(r<<31);
	/* clear the top bits on machines with 8byte longs */
	l&=0xffffffffL;
	r&=0xffffffffL;

	FP(r,l);
	data[0]=l;
	data[1]=r;
	l=r=t=u=0;
	}

void des_encrypt2(data, ks, encrypt)
unsigned long *data;
des_key_schedule ks;
int encrypt;
	{
	register unsigned long l,r,t,u;
#ifdef DES_USE_PTR
	register unsigned char *des_SP=(unsigned char *)des_SPtrans;
#endif
#ifdef MSDOS
	union fudge {
		unsigned long  l;
		unsigned short s[2];
		unsigned char  c[4];
		} U,T;
#endif
	register int i;
	register unsigned long *s;

	u=data[0];
	r=data[1];

	/* Things have been modified so that the initial rotate is
	 * done outside the loop.  This required the
	 * des_SPtrans values in sp.h to be rotated 1 bit to the right.
	 * One perl script later and things have a 5% speed up on a sparc2.
	 * Thanks to Richard Outerbridge <71755.204@CompuServe.COM>
	 * for pointing this out. */
	l=(r<<1)|(r>>31);
	r=(u<<1)|(u>>31);

	/* clear the top bits on machines with 8byte longs */
	l&=0xffffffffL;
	r&=0xffffffffL;

	s=(unsigned long *)ks;
	/* I don't know if it is worth the effort of loop unrolling the
	 * inner loop */
	if (encrypt)
		{
		for (i=0; i<32; i+=4)
			{
			D_ENCRYPT(l,r,i+0); /*  1 */
			D_ENCRYPT(r,l,i+2); /*  2 */
			}
		}
	else
		{
		for (i=30; i>0; i-=4)
			{
			D_ENCRYPT(l,r,i-0); /* 16 */
			D_ENCRYPT(r,l,i-2); /* 15 */
			}
		}
	l=(l>>1)|(l<<31);
	r=(r>>1)|(r<<31);
	/* clear the top bits on machines with 8byte longs */
	l&=0xffffffffL;
	r&=0xffffffffL;

	data[0]=l;
	data[1]=r;
	l=r=t=u=0;
	}
