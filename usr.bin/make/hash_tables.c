/*
 * DO NOT EDIT
 * $FreeBSD$
 * auto-generated from FreeBSD: src/usr.bin/make/parse.c,v 1.114 2008/03/12 14:50:58 obrien Exp 
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
 * n=74
 * m=35
 * c=2.09
 * maxlen=1
 * minklen=4
 * maxklen=13
 * minchar=46
 * maxchar=95
 * loop=0
 * numiter=4
 * seed=
 */

static const signed char keyword_g[] = {
	12, 18, 7, 25, 30, 5, -1, -1, -1, 7,
	-1, 0, 33, 0, 4, -1, -1, 13, 29, 0,
	-1, 28, -1, 28, -1, 0, -1, 27, 4, 34,
	-1, -1, -1, 30, 13, 10, -1, -1, 0, 10,
	24, -1, -1, -1, 0, 6, 0, 0, -1, 23,
	-1, -1, -1, 0, -1, 23, -1, -1, 19, 4,
	-1, 31, 12, 16, -1, 20, 22, 9, 0, -1,
	-1, 9, 4, 0, 
};

static const u_char keyword_T0[] = {
	34, 28, 50, 61, 14, 57, 48, 60, 20, 67,
	60, 63, 0, 24, 28, 2, 49, 64, 18, 23,
	36, 33, 40, 14, 38, 42, 71, 49, 2, 53,
	53, 37, 7, 29, 24, 21, 12, 50, 59, 10,
	43, 23, 0, 44, 47, 6, 46, 22, 48, 64,
};

static const u_char keyword_T1[] = {
	18, 67, 39, 60, 7, 70, 2, 26, 31, 18,
	73, 47, 61, 17, 38, 50, 22, 52, 13, 55,
	56, 32, 63, 4, 64, 55, 49, 21, 47, 67,
	33, 66, 60, 73, 30, 68, 69, 32, 72, 4,
	28, 49, 51, 15, 66, 68, 43, 67, 46, 56,
};


int
keyword_hash(const u_char *key, size_t len)
{
	unsigned f0, f1;
	const u_char *kp = key;

	if (len < 4 || len > 13)
		return -1;

	for (f0=f1=0; *kp; ++kp) {
		if (*kp < 46 || *kp > 95)
			return -1;
		f0 += keyword_T0[-46 + *kp];
		f1 += keyword_T1[-46 + *kp];
	}

	f0 %= 74;
	f1 %= 74;

	return (keyword_g[f0] + keyword_g[f1]) % 35;
}
