/*
 * DO NOT EDIT
 * $FreeBSD$
 * auto-generated from FreeBSD: src/usr.bin/make/parse.c,v 1.97 2005/04/28 07:55:55 harti Exp 
 * DO NOT EDIT
 */
#include <sys/types.h>

#include "directive_hash.h"

/*
 * d=2
 * n=38
 * m=18
 * c=2.09
 * maxlen=1
 * minklen=2
 * maxklen=9
 * minchar=97
 * maxchar=119
 * loop=0
 * numiter=2
 * seed=
 */

static const signed char directive_g[] = {
	16, 0, -1, 14, 5, 2, 2, -1, 0, 0,
	-1, -1, 16, 11, -1, 15, -1, 14, 7, -1,
	8, 6, 1, -1, -1, 0, 4, 6, -1, 0,
	0, 2, 0, 13, -1, 14, -1, 0, 
};

static const u_char directive_T0[] = {
	11, 25, 14, 30, 14, 26, 23, 15, 9, 37,
	27, 32, 27, 1, 17, 27, 35, 13, 8, 22,
	8, 28, 7, 
};

static const u_char directive_T1[] = {
	19, 20, 31, 17, 29, 2, 7, 12, 1, 31,
	11, 18, 11, 20, 10, 2, 15, 19, 4, 10,
	13, 36, 3, 
};


int
directive_hash(const u_char *key, size_t len)
{
	unsigned f0, f1;
	const u_char *kp = key;

	if (len < 2 || len > 9)
		return -1;

	for (f0=f1=0; kp < key + len; ++kp) {
		if (*kp < 97 || *kp > 119)
			return -1;
		f0 += directive_T0[-97 + *kp];
		f1 += directive_T1[-97 + *kp];
	}

	f0 %= 38;
	f1 %= 38;

	return (directive_g[f0] + directive_g[f1]) % 18;
}
