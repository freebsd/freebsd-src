/* rand_key.c */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: rand_key.c,v 1.1.1.1 1994/09/30 14:49:51 csgr Exp $
 */

#include "des_locl.h"

int des_random_key(ret)
des_cblock ret;
	{
	des_key_schedule ks;
	static unsigned long c=0;
	static unsigned short pid=0;
	static des_cblock data={0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
	des_cblock key;
	unsigned char *p;
	unsigned long t;

#ifdef MSDOS
	pid=1;
#else
	if (!pid) pid=getpid();
#endif
	p=key;
	t=(unsigned long)time(NULL);
	l2c(t,p);
	t=(unsigned long)((pid)|((c++)<<16));
	l2c(t,p);

	des_set_odd_parity((des_cblock *)data);
	des_set__key((des_cblock *)data,ks);
	des_cbc_cksum((des_cblock *)key,(des_cblock *)key,
		(long)sizeof(key),ks,(des_cblock *)data);
	des_set_odd_parity((des_cblock *)key);
	des_cbc_cksum((des_cblock *)key,(des_cblock *)key,
		(long)sizeof(key),ks,(des_cblock *)data);
	des_set_odd_parity((des_cblock *)key);

	bcopy(key,ret,sizeof(key));
	bzero(key,sizeof(key));
	bzero(ks,sizeof(ks));
	t=0;
	return(0);
	}
