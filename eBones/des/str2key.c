/* str2key.c */
/* Copyright (C) 1993 Eric Young - see README for more details */

/*-
 *	$Id: str2key.c,v 1.2 1994/07/19 19:22:08 g89r4222 Exp $
 */

#include "des_locl.h"

extern int des_check_key;

int des_string_to_key(str,key)
char *str;
des_cblock *key;
	{
	des_key_schedule ks;
	int i,length;
	register unsigned char j;

	bzero(key,8);
	length=strlen(str);
#ifdef OLD_STR_TO_KEY
	for (i=0; i<length; i++)
		(*key)[i%8]^=(str[i]<<1);
#else /* MIT COMPATIBLE */
	for (i=0; i<length; i++)
		{
		j=str[i];
		if ((i%16) < 8)
			(*key)[i%8]^=(j<<1);
		else
			{
			/* Reverse the bit order 05/05/92 eay */
			j=((j<<4)&0xf0)|((j>>4)&0x0f);
			j=((j<<2)&0xcc)|((j>>2)&0x33);
			j=((j<<1)&0xaa)|((j>>1)&0x55);
			(*key)[7-(i%8)]^=j;
			}
		}
#endif
	des_set_odd_parity((des_cblock *)key);
	i=des_check_key;
	des_check_key=0;
	des_set__key((des_cblock *)key,ks);
	des_check_key=i;
	des_cbc_cksum((des_cblock *)str,(des_cblock *)key,(long)length,ks,
		(des_cblock *)key);
	bzero(ks,sizeof(ks));
	des_set_odd_parity((des_cblock *)key);
	return(0);
	}

int des_string_to_2keys(str,key1,key2)
char *str;
des_cblock *key1,*key2;
	{
	des_key_schedule ks;
	int i,length;
	register unsigned char j;

	bzero(key1,8);
	bzero(key2,8);
	length=strlen(str);
#ifdef OLD_STR_TO_KEY
	if (length <= 8)
		{
		for (i=0; i<length; i++)
			{
			(*key2)[i]=(*key1)[i]=(str[i]<<1);
			}
		}
	else
		{
		for (i=0; i<length; i++)
			{
			if ((i/8)&1)
				(*key2)[i%8]^=(str[i]<<1);
			else
				(*key1)[i%8]^=(str[i]<<1);
			}
		}
#else /* MIT COMPATIBLE */
	for (i=0; i<length; i++)
		{
		j=str[i];
		if ((i%32) < 16)
			{
			if ((i%16) < 8)
				(*key1)[i%8]^=(j<<1);
			else
				(*key2)[i%8]^=(j<<1);
			}
		else
			{
			j=((j<<4)&0xf0)|((j>>4)&0x0f);
			j=((j<<2)&0xcc)|((j>>2)&0x33);
			j=((j<<1)&0xaa)|((j>>1)&0x55);
			if ((i%16) < 8)
				(*key1)[7-(i%8)]^=j;
			else
				(*key2)[7-(i%8)]^=j;
			}
		}
	if (length <= 8) bcopy(key1,key2,8);
#endif
	des_set_odd_parity((des_cblock *)key1);
	des_set_odd_parity((des_cblock *)key2);
	i=des_check_key;
	des_check_key=0;
	des_set__key((des_cblock *)key1,ks);
	des_cbc_cksum((des_cblock *)str,(des_cblock *)key1,(long)length,ks,
		(des_cblock *)key1);
	des_set__key((des_cblock *)key2,ks);
	des_cbc_cksum((des_cblock *)str,(des_cblock *)key2,(long)length,ks,
		(des_cblock *)key2);
	des_check_key=i;
	bzero(ks,sizeof(ks));
	des_set_odd_parity(key1);
	des_set_odd_parity(key2);
	return(0);
	}
