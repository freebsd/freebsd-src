/* cfb_enc.c */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: cfb_enc.c,v 1.2 1994/07/19 19:21:48 g89r4222 Exp $
 */

#include "des_locl.h"

/* The input and output are loaded in multiples of 8 bits.
 * What this means is that if you hame numbits=12 and length=2
 * the first 12 bits will be retrieved from the first byte and half
 * the second.  The second 12 bits will come from the 3rd and half the 4th
 * byte.
 */
int des_cfb_encrypt(in,out,numbits,length,schedule,ivec,encrypt)
unsigned char *in,*out;
int numbits;
long length;
des_key_schedule schedule;
des_cblock *ivec;
int encrypt;
	{
	register unsigned long d0,d1,v0,v1,n=(numbits+7)/8;
	register unsigned long mask0,mask1;
	register long l=length;
	register int num=numbits;
	unsigned long ti[2],to[2];
	unsigned char *iv;

	if (num > 64) return(0);
	if (num > 32)
		{
		mask0=0xffffffff;
		if (num == 64)
			mask1=mask0;
		else
			mask1=(1L<<(num-32))-1;
		}
	else
		{
		if (num == 32)
			mask0=0xffffffff;
		else
			mask0=(1L<<num)-1;
		mask1=0x00000000;
		}

	iv=(unsigned char *)ivec;
	c2l(iv,v0);
	c2l(iv,v1);
	if (encrypt)
		{
		while (l-- > 0)
			{
			ti[0]=v0;
			ti[1]=v1;
			des_encrypt((unsigned long *)ti,(unsigned long *)to,
					schedule,DES_ENCRYPT);
			c2ln(in,d0,d1,n);
			in+=n;
			d0=(d0^to[0])&mask0;
			d1=(d1^to[1])&mask1;
			l2cn(d0,d1,out,n);
			out+=n;
			if (num > 32)
				{
				v0=((v1>>(num-32))|(d0<<(64-num)))&0xffffffff;
				v1=((d0>>(num-32))|(d1<<(64-num)))&0xffffffff;
				}
			else
				{
				v0=((v0>>num)|(v1<<(32-num)))&0xffffffff;
				v1=((v1>>num)|(d0<<(32-num)))&0xffffffff;
				}
			}
		}
	else
		{
		while (l-- > 0)
			{
			ti[0]=v0;
			ti[1]=v1;
			des_encrypt((unsigned long *)ti,(unsigned long *)to,
					schedule,DES_ENCRYPT);
			c2ln(in,d0,d1,n);
			in+=n;
			if (num > 32)
				{
				v0=((v1>>(num-32))|(d0<<(64-num)))&0xffffffff;
				v1=((d0>>(num-32))|(d1<<(64-num)))&0xffffffff;
				}
			else
				{
				v0=((v0>>num)|(v1<<(32-num)))&0xffffffff;
				v1=((v1>>num)|(d0<<(32-num)))&0xffffffff;
				}
			d0=(d0^to[0])&mask0;
			d1=(d1^to[1])&mask1;
			l2cn(d0,d1,out,n);
			out+=n;
			}
		}
	iv=(unsigned char *)ivec;
	l2c(v0,iv);
	l2c(v1,iv);
	v0=v1=d0=d1=ti[0]=ti[1]=to[0]=to[1]=0;
	return(0);
	}

