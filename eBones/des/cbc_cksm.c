/* cbc_cksm.c */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: cbc_cksm.c,v 1.2 1994/07/19 19:21:45 g89r4222 Exp $
 */

#include "des_locl.h"

unsigned long des_cbc_cksum(input,output,length,schedule,ivec)
des_cblock *input;
des_cblock *output;
long length;
des_key_schedule schedule;
des_cblock *ivec;
	{
	register unsigned long tout0,tout1,tin0,tin1;
	register long l=length;
	unsigned long tin[2],tout[2];
	unsigned char *in,*out,*iv;

	in=(unsigned char *)input;
	out=(unsigned char *)output;
	iv=(unsigned char *)ivec;

	c2l(iv,tout0);
	c2l(iv,tout1);
	for (; l>0; l-=8)
		{
		if (l >= 8)
			{
			c2l(in,tin0);
			c2l(in,tin1);
			}
		else
			c2ln(in,tin0,tin1,l);
			
		tin0^=tout0;
		tin1^=tout1;
		tin[0]=tin0;
		tin[1]=tin1;
		des_encrypt((unsigned long *)tin,(unsigned long *)tout,
			schedule,DES_ENCRYPT);
		/* fix 15/10/91 eay - thanks to keithr@sco.COM */
		tout0=tout[0];
		tout1=tout[1];
		}
	if (out != NULL)
		{
		l2c(tout0,out);
		l2c(tout1,out);
		}
	tout0=tin0=tin1=tin[0]=tin[1]=tout[0]=tout[1]=0;
	return(tout1);
	}
