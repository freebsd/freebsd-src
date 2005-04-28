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
/*
 * d=2
 * n=67
 * m=32
 * c=2.09
 * maxlen=1
 * minklen=4
 * maxklen=12
 * minchar=46
 * maxchar=95
 * loop=0
 * numiter=2
 * seed=
 */

static const signed char keyword_g[] = {
	17, -1, 18, 13, 26, 0, 0, 29, -1, 0,
	7, -1, -1, 23, 13, 27, -1, 0, 14, 24,
	-1, -1, 0, 24, -1, 0, -1, 27, 19, 12,
	3, -1, -1, 3, 19, 28, 10, 17, -1, 8,
	-1, -1, -1, 0, 5, 8, -1, 0, -1, -1,
	0, 27, 4, -1, -1, 25, -1, 30, -1, 8,
	16, -1, 0, -1, 0, 26, 14, 
};

static const u_char keyword_T0[] = {
	17, 32, 43, 6, 64, 15, 20, 26, 30, 64,
	54, 31, 6, 61, 4, 49, 62, 37, 23, 50,
	6, 58, 29, 19, 32, 50, 56, 8, 18, 40,
	51, 36, 6, 27, 42, 3, 59, 12, 46, 23,
	9, 50, 4, 16, 44, 25, 15, 40, 62, 55,
};

static const u_char keyword_T1[] = {
	24, 38, 31, 14, 65, 31, 23, 17, 27, 45,
	32, 44, 19, 45, 18, 31, 28, 43, 0, 21,
	29, 27, 42, 55, 21, 31, 14, 13, 66, 17,
	39, 40, 5, 4, 5, 4, 52, 28, 21, 12,
	7, 54, 6, 43, 49, 24, 7, 27, 0, 24,
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

	f0 %= 67;
	f1 %= 67;

	return (keyword_g[f0] + keyword_g[f1]) % 32;
}
