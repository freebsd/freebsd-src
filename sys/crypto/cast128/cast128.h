/*	$FreeBSD: src/sys/crypto/cast128/cast128.h,v 1.7 2003/10/10 15:06:16 ume Exp $	*/
/*	$NetBSD: cast128.h,v 1.6 2003/08/26 20:03:57 thorpej Exp $ */
/*      $OpenBSD: cast.h,v 1.2 2002/03/14 01:26:51 millert Exp $       */

/*
 *	CAST-128 in C
 *	Written by Steve Reid <sreid@sea-to-sky.net>
 *	100% Public Domain - no warranty
 *	Released 1997.10.11
 */

#ifndef _CAST128_H_
#define _CAST128_H_

typedef struct {
	u_int32_t	xkey[32];	/* Key, after expansion */
	int		rounds;		/* Number of rounds to use, 12 or 16 */
} cast128_key;

void cast128_setkey(cast128_key *key, const u_int8_t *rawkey, int keybytes);
void cast128_encrypt(const cast128_key *key, const u_int8_t *inblock,
		     u_int8_t *outblock);
void cast128_decrypt(const cast128_key *key, const u_int8_t *inblock,
		     u_int8_t *outblock);

#endif /* _CAST128_H_ */
