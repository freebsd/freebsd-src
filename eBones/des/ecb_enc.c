/* ecb_enc.c */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: ecb_enc.c,v 1.2 1994/07/19 19:21:53 g89r4222 Exp $
 */

#include "des_locl.h"
#include "spr.h"

int des_ecb_encrypt(input,output,ks,encrypt)
des_cblock *input;
des_cblock *output;
des_key_schedule ks;
int encrypt;
	{
	register unsigned long l0,l1;
	register unsigned char *in,*out;
	unsigned long ll[2];

	in=(unsigned char *)input;
	out=(unsigned char *)output;
	c2l(in,l0);
	c2l(in,l1);
	ll[0]=l0;
	ll[1]=l1;
	des_encrypt(ll,ll,ks,encrypt);
	l0=ll[0];
	l1=ll[1];
	l2c(l0,out);
	l2c(l1,out);
	l0=l1=ll[0]=ll[1]=0;
	return(0);
	}

int des_encrypt(input,output,ks,encrypt)
unsigned long *input;
unsigned long *output;
des_key_schedule ks;
int encrypt;
	{
	register unsigned long l,r,t,u;
#ifdef ALT_ECB
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

	l=input[0];
	r=input[1];

	/* do IP */
	PERM_OP(r,l,t, 4,0x0f0f0f0f);
	PERM_OP(l,r,t,16,0x0000ffff);
	PERM_OP(r,l,t, 2,0x33333333);
	PERM_OP(l,r,t, 8,0x00ff00ff);
	PERM_OP(r,l,t, 1,0x55555555);
	/* r and l are reversed - remember that :-) - fix
	 * it in the next step */

	/* Things have been modified so that the initial rotate is
	 * done outside the loop.  This required the
	 * des_SPtrans values in sp.h to be rotated 1 bit to the right.
	 * One perl script later and things have a 5% speed up on a sparc2.
	 * Thanks to Richard Outerbridge <71755.204@CompuServe.COM>
	 * for pointing this out. */
	t=(r<<1)|(r>>31);
	r=(l<<1)|(l>>31);
	l=t;

	/* clear the top bits on machines with 8byte longs */
	l&=0xffffffff;
	r&=0xffffffff;

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
	l&=0xffffffff;
	r&=0xffffffff;

	/* swap l and r
	 * we will not do the swap so just remember they are
	 * reversed for the rest of the subroutine
	 * luckily FP fixes this problem :-) */

	PERM_OP(r,l,t, 1,0x55555555);
	PERM_OP(l,r,t, 8,0x00ff00ff);
	PERM_OP(r,l,t, 2,0x33333333);
	PERM_OP(l,r,t,16,0x0000ffff);
	PERM_OP(r,l,t, 4,0x0f0f0f0f);

	output[0]=l;
	output[1]=r;
	l=r=t=u=0;
	return(0);
	}

