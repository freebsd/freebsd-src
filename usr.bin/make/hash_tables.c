/*
 * DO NOT EDIT
 * $FreeBSD$
 * auto-generated from FreeBSD: src/usr.bin/make/parse.c,v 1.108 2005/05/24 15:30:03 harti Exp 
 * DO NOT EDIT
 */
#include <sys/types.h>

#include "hash_tables.h"

/*
 * d=2
 * n=40
 * m=19
 * c=2.09
 * maxlen=1
 * minklen=2
 * maxklen=9
 * minchar=97
 * maxchar=119
 * loop=0
 * numiter=1
 * seed=
 */

static const signed char directive_g[] = {
	8, 0, 0, 5, 6, -1, 17, 15, 10, 6,
	-1, -1, 10, 0, 0, -1, 18, 2, 3, 0,
	7, -1, -1, -1, 0, 14, -1, -1, 11, 16,
	-1, -1, 0, -1, 0, 0, 17, 0, -1, 1,
};

static const u_char directive_T0[] = {
	26, 14, 19, 35, 10, 34, 18, 27, 1, 17,
	22, 37, 12, 12, 36, 21, 0, 6, 1, 25,
	9, 4, 19, 
};

static const u_char directive_T1[] = {
	25, 22, 19, 0, 2, 18, 33, 18, 30, 4,
	30, 9, 21, 19, 16, 12, 35, 34, 4, 19,
	9, 33, 16, 
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

	f0 %= 40;
	f1 %= 40;

	return (directive_g[f0] + directive_g[f1]) % 19;
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
