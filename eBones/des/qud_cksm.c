/* qud_cksm.c */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: qud_cksm.c,v 1.2 1994/07/19 19:22:02 g89r4222 Exp $
 */

/* From "Message Authentication"  R.R. Jueneman, S.M. Matyas, C.H. Meyer
 * IEEE Communications Magazine Sept 1985 Vol. 23 No. 9 p 29-40
 * This module in only based on the code in this paper and is
 * almost definitely not the same as the MIT implementation.
 */
#include "des_locl.h"

/* bug fix for dos - 7/6/91 - Larry hughes@logos.ucs.indiana.edu */
#define B0(a)	(((unsigned long)(a)))
#define B1(a)	(((unsigned long)(a))<<8)
#define B2(a)	(((unsigned long)(a))<<16)
#define B3(a)	(((unsigned long)(a))<<24)

/* used to scramble things a bit */
/* Got the value MIT uses via brute force :-) 2/10/90 eay */
#define NOISE	((unsigned long)83653421)

unsigned long des_quad_cksum(input,output,length,out_count,seed)
des_cblock *input;
des_cblock *output;
long length;
int out_count;
des_cblock *seed;
	{
	unsigned long z0,z1,t0,t1;
	int i;
	long l=0;
	unsigned char *cp;
	unsigned char *lp;

	if (out_count < 1) out_count=1;
	lp=(unsigned char *)output;

	z0=B0((*seed)[0])|B1((*seed)[1])|B2((*seed)[2])|B3((*seed)[3]);
	z1=B0((*seed)[4])|B1((*seed)[5])|B2((*seed)[6])|B3((*seed)[7]);

	for (i=0; ((i<4)&&(i<out_count)); i++)
		{
		cp=(unsigned char *)input;
		l=length;
		while (l > 0)
			{
			if (l > 1)
				{
				t0= (unsigned long)(*(cp++));
				t0|=(unsigned long)B1(*(cp++));
				l--;
				}
			else
				t0= (unsigned long)(*(cp++));
			l--;
			/* add */
			t0+=z0;
			t0&=0xffffffff;
			t1=z1;
			/* square, well sort of square */
			z0=((((t0*t0)&0xffffffff)+((t1*t1)&0xffffffff))
				&0xffffffff)%0x7fffffff; 
			z1=((t0*((t1+NOISE)&0xffffffff))&0xffffffff)%0x7fffffff;
			}
		if (lp != NULL)
			{
			/* I believe I finally have things worked out.
			 * The MIT library assumes that the checksum
			 * is one huge number and it is returned in a
			 * host dependant byte order.
			 */
			static unsigned long l=1;
			static unsigned char *c=(unsigned char *)&l;

			if (c[0])
				{
				l2c(z0,lp);
				l2c(z1,lp);
				}
			else
				{
				lp=output[out_count-i-1];
				l2n(z1,lp);
				l2n(z0,lp);
				}
			}
		}
	return(z0);
	}

