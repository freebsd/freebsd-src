/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2017 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUXKPI_LINUX_STRING_H_
#define	_LINUXKPI_LINUX_STRING_H_

#include <sys/ctype.h>

#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/bitops.h> /* for BITS_PER_LONG */
#include <linux/overflow.h>
#include <linux/stdarg.h>

#include <sys/libkern.h>

#define	strnicmp(...) strncasecmp(__VA_ARGS__)

static inline int
match_string(const char *const *table, int n, const char *key)
{
	int i;

	for (i = 0; i != n && table[i] != NULL; i++) {
		if (strcmp(table[i], key) == 0)
			return (i);
	}
	return (-EINVAL);
}

static inline void *
memdup_user(const void *ptr, size_t len)
{
	void *retval;
	int error;

	retval = malloc(len, M_KMALLOC, M_WAITOK);
	error = linux_copyin(ptr, retval, len);
	if (error != 0) {
		free(retval, M_KMALLOC);
		return (ERR_PTR(error));
	}
	return (retval);
}

static inline void *
memdup_user_nul(const void *ptr, size_t len)
{
	char *retval;
	int error;

	retval = malloc(len + 1, M_KMALLOC, M_WAITOK);
	error = linux_copyin(ptr, retval, len);
	if (error != 0) {
		free(retval, M_KMALLOC);
		return (ERR_PTR(error));
	}
	retval[len] = '\0';
	return (retval);
}

static inline void *
kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *dst;

	dst = kmalloc(len, gfp);
	if (dst != NULL)
		memcpy(dst, src, len);
	return (dst);
}

/* See slab.h for kvmalloc/kvfree(). */
static inline void *
kvmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *dst;

	dst = kvmalloc(len, gfp);
	if (dst != NULL)
		memcpy(dst, src, len);
	return (dst);
}

static inline char *
strndup_user(const char __user *ustr, long n)
{
	if (n < 1)
		return (ERR_PTR(-EINVAL));

	return (memdup_user_nul(ustr, n - 1));
}

static inline char *
kstrdup(const char *string, gfp_t gfp)
{
	char *retval;
	size_t len;

	if (string == NULL)
		return (NULL);
	len = strlen(string) + 1;
	retval = kmalloc(len, gfp);
	if (retval != NULL)
		memcpy(retval, string, len);
	return (retval);
}

static inline char *
kstrndup(const char *string, size_t len, gfp_t gfp)
{
	char *retval;

	if (string == NULL)
		return (NULL);
	retval = kmalloc(len + 1, gfp);
	if (retval != NULL)
		strncpy(retval, string, len);
	return (retval);
}

static inline const char *
kstrdup_const(const char *src, gfp_t gfp)
{
	return (kmemdup(src, strlen(src) + 1, gfp));
}

static inline char *
skip_spaces(const char *str)
{
	while (isspace(*str))
		++str;
	return (__DECONST(char *, str));
}

/*
 * This function trims whitespaces at the end of a string and returns a pointer
 * to the first non-whitespace character.
 */
static inline char *
strim(char *str)
{
	char *end;

	end = str + strlen(str);
	while (end >= str && (*end == '\0' || isspace(*end))) {
		*end = '\0';
		end--;
	}

	return (skip_spaces(str));
}

static inline void *
memchr_inv(const void *start, int c, size_t length)
{
	const u8 *ptr;
	const u8 *end;
	u8 ch;

	ch = c;
	ptr = start;
	end = ptr + length;

	while (ptr != end) {
		if (*ptr != ch)
			return (__DECONST(void *, ptr));
		ptr++;
	}
	return (NULL);
}

static inline size_t
str_has_prefix(const char *str, const char *prefix)
{
	size_t len;

	len = strlen(prefix);
	return (strncmp(str, prefix, len) == 0 ? len : 0);
}

static inline char *
strreplace(char *str, char old, char new)
{
	char *p;

	p = strchrnul(str, old);
	while (p != NULL && *p != '\0') {
		*p = new;
		p = strchrnul(str, old);
	}
	return (p);
}

static inline ssize_t
strscpy(char* dst, const char* src, size_t len)
{
	size_t i;

	if (len <= INT_MAX) {
		for (i = 0; i < len; i++)
			if ('\0' == (dst[i] = src[i]))
				return ((ssize_t)i);
		if (i != 0)
			dst[--i] = '\0';
	}

	return (-E2BIG);
}

static inline ssize_t
strscpy_pad(char* dst, const char* src, size_t len)
{

	bzero(dst, len);

	return (strscpy(dst, src, len));
}

static inline char *
strnchr(const char *cp, size_t n, int ch)
{
	char *p;

	for (p = __DECONST(char *, cp); n--; ++p) {
		if (*p == ch)
			return (p);
		if (*p == '\0')
			break;
	}

	return (NULL);
}

static inline void *
memset32(uint32_t *b, uint32_t c, size_t len)
{
	uint32_t *dst = b;

	while (len--)
		*dst++ = c;
	return (b);
}

static inline void *
memset64(uint64_t *b, uint64_t c, size_t len)
{
	uint64_t *dst = b;

	while (len--)
		*dst++ = c;
	return (b);
}

static inline void *
memset_p(void **p, void *v, size_t n)
{

	if (BITS_PER_LONG == 32)
		return (memset32((uint32_t *)p, (uintptr_t)v, n));
	else
		return (memset64((uint64_t *)p, (uintptr_t)v, n));
}

static inline void
memcpy_and_pad(void *dst, size_t dstlen, const void *src, size_t len, int ch)
{

	if (len >= dstlen) {
		memcpy(dst, src, dstlen);
	} else {
		memcpy(dst, src, len);
		/* Pad with given padding character. */
		memset((char *)dst + len, ch, dstlen - len);
	}
}

#define	memset_startat(ptr, bytepat, smember)				\
({									\
	uint8_t *_ptr = (uint8_t *)(ptr);				\
	int _c = (int)(bytepat);					\
	size_t _o = offsetof(typeof(*(ptr)), smember);			\
	memset(_ptr + _o, _c, sizeof(*(ptr)) - _o);			\
})

#define	memset_after(ptr, bytepat, smember)				\
({									\
	uint8_t *_ptr = (uint8_t *)(ptr);				\
	int _c = (int)(bytepat);					\
	size_t _o = offsetofend(typeof(*(ptr)), smember);		\
	memset(_ptr + _o, _c, sizeof(*(ptr)) - _o);			\
})

static inline void
memzero_explicit(void *p, size_t s)
{
	memset(p, 0, s);
	__asm__ __volatile__("": :"r"(p) :"memory");
}

#endif	/* _LINUXKPI_LINUX_STRING_H_ */
