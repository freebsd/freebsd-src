/* 3ecb_enc.c */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: 3ecb_enc.c,v 1.2 1994/07/19 19:21:38 g89r4222 Exp $
 */

#include "des_locl.h"

int des_3ecb_encrypt(input,output,ks1,ks2,encrypt)
des_cblock *input;
des_cblock *output;
des_key_schedule ks1,ks2;
int encrypt;
	{
	register unsigned long l0,l1,t;
	register unsigned char *in,*out;
	unsigned long ll[2];

	in=(unsigned char *)input;
	out=(unsigned char *)output;
	c2l(in,l0);
	c2l(in,l1);
	ll[0]=l0;
	ll[1]=l1;
	des_encrypt(ll,ll,ks1,encrypt);
	des_encrypt(ll,ll,ks2,!encrypt);
	des_encrypt(ll,ll,ks1,encrypt);
	l0=ll[0];
	l1=ll[1];
	l2c(l0,out);
	l2c(l1,out);
	return(0);
	}

