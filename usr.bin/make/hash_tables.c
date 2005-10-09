/*
 * DO NOT EDIT
 * $FreeBSD$
 * auto-generated from FreeBSD: src/usr.bin/make/parse.c,v 1.100 2005/04/29 15:15:28 harti Exp 
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
 * n=72
 * m=34
 * c=2.09
 * maxlen=1
 * minklen=4
 * maxklen=12
 * minchar=46
 * maxchar=95
 * loop=0
 * numiter=4
 * seed=
 */

static const signed char keyword_g[] = {
	8, 15, -1, 25, 22, 20, -1, 33, 16, -1,
	21, 31, 0, 0, 0, 29, 30, 8, -1, 0,
	-1, 21, -1, 0, -1, -1, -1, -1, -1, 4,
	-1, -1, 25, 28, -1, 27, 11, 23, 0, 0,
	24, -1, -1, 0, 3, 0, -1, 24, 0, 0,
	-1, 28, 12, -1, 20, 13, -1, 5, -1, 1,
	0, 0, -1, 0, 10, 19, 13, 9, -1, 2,
	-1, -1, 
};

static const u_char keyword_T0[] = {
	32, 10, 54, 61, 2, 35, 62, 50, 52, 53,
	70, 7, 62, 18, 24, 30, 31, 66, 10, 61,
	52, 71, 56, 56, 28, 6, 33, 67, 12, 41,
	39, 45, 51, 21, 34, 53, 56, 40, 47, 52,
	21, 61, 60, 12, 7, 28, 42, 38, 38, 52,
};

static const u_char keyword_T1[] = {
	0, 39, 65, 48, 13, 62, 46, 42, 5, 50,
	69, 69, 69, 43, 2, 46, 12, 6, 11, 9,
	24, 10, 25, 64, 68, 13, 57, 55, 17, 33,
	1, 18, 0, 67, 10, 14, 57, 56, 0, 6,
	50, 13, 3, 47, 56, 22, 37, 13, 28, 48,
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

	f0 %= 72;
	f1 %= 72;

	return (keyword_g[f0] + keyword_g[f1]) % 34;
}
