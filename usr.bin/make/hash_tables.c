/*
 * DO NOT EDIT
 * $FreeBSD$
 * auto-generated from FreeBSD: src/usr.bin/make/parse.c,v 1.99 2005/04/29 14:37:44 harti Exp 
 * DO NOT EDIT
 */
#include <sys/types.h>

#include "hash_tables.h"

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
/*
 * d=2
 * n=69
 * m=33
 * c=2.09
 * maxlen=1
 * minklen=4
 * maxklen=12
 * minchar=46
 * maxchar=95
 * loop=0
 * numiter=8
 * seed=
 */

static const signed char keyword_g[] = {
	-1, 17, 16, 0, -1, -1, -1, -1, 25, 18,
	2, -1, -1, 27, 28, 1, 0, 15, 16, -1,
	-1, 14, 19, 1, -1, 13, -1, 0, 1, -1,
	11, 15, 0, 8, 14, 18, 31, -1, -1, 22,
	-1, 27, -1, 0, -1, 9, -1, -1, -1, 21,
	3, 25, 0, 0, 0, -1, -1, 6, 0, 19,
	-1, -1, -1, 23, -1, 17, -1, 0, 0, 
};

static const u_char keyword_T0[] = {
	8, 30, 55, 61, 14, 13, 48, 1, 18, 12,
	0, 52, 1, 40, 44, 52, 33, 58, 29, 29,
	3, 30, 26, 42, 1, 49, 10, 26, 5, 45,
	65, 13, 6, 22, 45, 61, 7, 25, 62, 65,
	8, 34, 48, 50, 5, 63, 33, 38, 52, 33,
};

static const u_char keyword_T1[] = {
	44, 18, 49, 61, 56, 13, 1, 54, 1, 47,
	46, 17, 22, 36, 25, 66, 14, 36, 58, 51,
	60, 22, 61, 19, 43, 37, 5, 18, 50, 58,
	32, 65, 47, 12, 28, 34, 65, 29, 59, 67,
	48, 36, 15, 41, 44, 11, 39, 29, 18, 68,
};


int
keyword_hash(const u_char *key, size_t len)
{
	unsigned f0, f1;
	const u_char *kp = key;

	if (len < 4 || len > 12)
		return -1;

	for (f0=f1=0; *kp; ++kp) {
		if (*kp < 46 || *kp > 95)
			return -1;
		f0 += keyword_T0[-46 + *kp];
		f1 += keyword_T1[-46 + *kp];
	}

	f0 %= 69;
	f1 %= 69;

	return (keyword_g[f0] + keyword_g[f1]) % 33;
}
