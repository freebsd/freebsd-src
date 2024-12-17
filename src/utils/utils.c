/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/utils/utils.h>

/*
 * Return 1 in 'check' if first 'len' bytes of both buffers a and b are equal, 0 otherwise.
 * It returns 0 if success, -1 on error. 'check' is only relevant on success.
 *
 * The test is done in constant time.
 */
int are_equal(const void *a, const void *b, u32 len, int *check)
{
	const u8 *la = (const u8*)a, *lb = (const u8*)b;
	int ret;
	u32 i;

	MUST_HAVE((a != NULL) && (b != NULL) && (check != NULL), ret, err);

	*check = 1;
	for (i = 0; i < len; i++) {
		(*check) &= (*la == *lb);
		la++;
		lb++;
	}

	ret = 0;

err:
	return ret;
}

/*
 * This function is a simple (non-optimized) reimplementation of memcpy()
 * Returns 0 on success, -1 on error.
 */
int local_memcpy(void *dst, const void *src, u32 n)
{
	const u8 *lsrc = (const u8*)src;
	u8 *ldst = (u8*)dst;
	u32 i;
	int ret;

	MUST_HAVE((dst != NULL) && (src != NULL), ret, err);

	for (i = 0; i < n; i++) {
		*ldst = *lsrc;
		ldst++;
		lsrc++;
	}

	ret = 0;

err:
	return ret;
}

/*
 * This function is a simple (non-optimized) reimplementation of memset()
 * Returns 0 on success, -1 on error.
 */
int local_memset(void *v, u8 c, u32 n)
{
	volatile u8 *p = (volatile u8*)v;
	u32 i;
	int ret;

	MUST_HAVE((v != NULL), ret, err);

	for (i = 0; i < n; i++) {
		*p = c;
		p++;
	}

	ret = 0;

err:
	return ret;
}

/*
 * Return 1 in 'check' if strings are equal, 0 otherwise.
 * It returns 0 if success, -1 on error. 'check' is only relevant on success.
 *
 */
int are_str_equal(const char *s1, const char *s2, int *check)
{
	const char *ls1 = s1, *ls2 = s2;
	int ret;

	MUST_HAVE((s1 != NULL) && (s2 != NULL) && (check != NULL), ret, err);

	while (*ls1 && (*ls1 == *ls2)) {
		ls1++;
		ls2++;
	}

	(*check) = (*ls1 == *ls2);

	ret = 0;

err:
	return ret;
}

/*
 * Return 1 in 'check' if strings are equal up to maxlen, 0 otherwise.
 * It returns 0 if success, -1 on error. 'check' is only relevant on success.
 *
 */
int are_str_equal_nlen(const char *s1, const char *s2, u32 maxlen, int *check)
{
	const char *ls1 = s1, *ls2 = s2;
	u32 i = 0;
	int ret;

	MUST_HAVE((s1 != NULL) && (s2 != NULL) && (check != NULL), ret, err);

	while (*ls1 && (*ls1 == *ls2) && (i < maxlen)) {
		ls1++;
		ls2++;
		i++;
	}

	(*check) = (*ls1 == *ls2);
	ret = 0;

err:
	return ret;
}



/*
 * This function is a simple (non-optimized) reimplementation of strlen()
 * Returns the lenth in 'len'.
 * It returns 0 if success, -1 on error. 'len' is only relevant on success.
 */
int local_strlen(const char *s, u32 *len)
{
	u32 i = 0;
	int ret;

	MUST_HAVE((s != NULL) && (len != NULL), ret, err);

	while (s[i]) {
		i++;
	}
	(*len) = i;

	ret = 0;

err:
	return ret;
}

/*
 * This function is a simple (non-optimized) reimplementation of strnlen()
 * Returns the lenth in 'len'.
 * It returns 0 if success, -1 on error. 'len' is only relevant on success.
 */
int local_strnlen(const char *s, u32 maxlen, u32 *len)
{
	u32 i = 0;
	int ret;

	MUST_HAVE((s != NULL) && (len != NULL), ret, err);

	while ((i < maxlen) && s[i]) {
		i++;
	}
	(*len) = i;

	ret = 0;

err:
	return ret;
}

/*
 * This functin is a simple (non-optimized) reimplementation of strncpy()
 */
int local_strncpy(char *dst, const char *src, u32 n)
{
	u32 i;
	int ret;

	MUST_HAVE((dst != NULL) && (src != NULL), ret, err);

	for (i = 0; (i < n) && src[i]; i++) {
		dst[i] = src[i];
	}
	for (; i < n; i++) {
		dst[i] = 0;
	}

	ret = 0;
err:
	return ret;
}

/*
 * This functin is a simple (non-optimized) reimplementation of strncat()
 */
int local_strncat(char *dst, const char *src, u32 n)
{
	u32 dst_len, i;
	int ret;

	MUST_HAVE((dst != NULL) && (src != NULL), ret, err);

	ret = local_strlen(dst, &dst_len); EG(ret, err);
	for (i = 0; (i < n) && src[i]; i++) {
		dst[dst_len + i] = src[i];
	}
	dst[dst_len + i] = 0;

	ret = 0;
err:
	return ret;
}
