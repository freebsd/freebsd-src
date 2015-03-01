/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "nv.h"
#include "nv_impl.h"

#include "dnv.h"

#define	DNVLIST_GET(ftype, type)					\
ftype									\
dnvlist_get_##type(const nvlist_t *nvl, const char *name, ftype defval)	\
{									\
									\
	if (nvlist_exists_##type(nvl, name))				\
		return (nvlist_get_##type(nvl, name));			\
	else								\
		return (defval);					\
}

DNVLIST_GET(bool, bool)
DNVLIST_GET(uint64_t, number)
DNVLIST_GET(const char *, string)
DNVLIST_GET(const nvlist_t *, nvlist)
DNVLIST_GET(int, descriptor)

#undef	DNVLIST_GET

const void *
dnvlist_get_binary(const nvlist_t *nvl, const char *name, size_t *sizep,
    const void *defval, size_t defsize)
{
	const void *value;

	if (nvlist_exists_binary(nvl, name))
		value = nvlist_get_binary(nvl, name, sizep);
	else {
		if (sizep != NULL)
			*sizep = defsize;
		value = defval;
	}
	return (value);
}

#define	DNVLIST_GETF(ftype, type)					\
ftype									\
dnvlist_getf_##type(const nvlist_t *nvl, ftype defval,			\
    const char *namefmt, ...)						\
{									\
	va_list nameap;							\
	ftype value;							\
									\
	va_start(nameap, namefmt);					\
	value = dnvlist_getv_##type(nvl, defval, namefmt, nameap);	\
	va_end(nameap);							\
									\
	return (value);							\
}

DNVLIST_GETF(bool, bool)
DNVLIST_GETF(uint64_t, number)
DNVLIST_GETF(const char *, string)
DNVLIST_GETF(const nvlist_t *, nvlist)
DNVLIST_GETF(int, descriptor)

#undef	DNVLIST_GETF

const void *
dnvlist_getf_binary(const nvlist_t *nvl, size_t *sizep, const void *defval,
    size_t defsize, const char *namefmt, ...)
{
	va_list nameap;
	const void *value;

	va_start(nameap, namefmt);
	value = dnvlist_getv_binary(nvl, sizep, defval, defsize, namefmt,
	    nameap);
	va_end(nameap);

	return (value);
}

#define	DNVLIST_GETV(ftype, type)					\
ftype									\
dnvlist_getv_##type(const nvlist_t *nvl, ftype defval,			\
    const char *namefmt, va_list nameap)				\
{									\
	char *name;							\
	ftype value;							\
									\
	vasprintf(&name, namefmt, nameap);				\
	if (name == NULL)						\
		return (defval);					\
	value = dnvlist_get_##type(nvl, name, defval);			\
	free(name);							\
	return (value);							\
}

DNVLIST_GETV(bool, bool)
DNVLIST_GETV(uint64_t, number)
DNVLIST_GETV(const char *, string)
DNVLIST_GETV(const nvlist_t *, nvlist)
DNVLIST_GETV(int, descriptor)

#undef	DNVLIST_GETV

const void *
dnvlist_getv_binary(const nvlist_t *nvl, size_t *sizep, const void *defval,
    size_t defsize, const char *namefmt, va_list nameap)
{
	char *name;
	const void *value;

	vasprintf(&name, namefmt, nameap);
	if (name != NULL) {
		value = dnvlist_get_binary(nvl, name, sizep, defval, defsize);
		free(name);
	} else {
		if (sizep != NULL)
			*sizep = defsize;
		value = defval;
	}
	return (value);
}

#define	DNVLIST_TAKE(ftype, type)					\
ftype									\
dnvlist_take_##type(nvlist_t *nvl, const char *name, ftype defval)	\
{									\
									\
	if (nvlist_exists_##type(nvl, name))				\
		return (nvlist_take_##type(nvl, name));			\
	else								\
		return (defval);					\
}

DNVLIST_TAKE(bool, bool)
DNVLIST_TAKE(uint64_t, number)
DNVLIST_TAKE(char *, string)
DNVLIST_TAKE(nvlist_t *, nvlist)
DNVLIST_TAKE(int, descriptor)

#undef	DNVLIST_TAKE

void *
dnvlist_take_binary(nvlist_t *nvl, const char *name, size_t *sizep,
    void *defval, size_t defsize)
{
	void *value;

	if (nvlist_exists_binary(nvl, name))
		value = nvlist_take_binary(nvl, name, sizep);
	else {
		if (sizep != NULL)
			*sizep = defsize;
		value = defval;
	}
	return (value);
}

#define	DNVLIST_TAKEF(ftype, type)					\
ftype									\
dnvlist_takef_##type(nvlist_t *nvl, ftype defval,			\
    const char *namefmt, ...)						\
{									\
	va_list nameap;							\
	ftype value;							\
									\
	va_start(nameap, namefmt);					\
	value = dnvlist_takev_##type(nvl, defval, namefmt, nameap);	\
	va_end(nameap);							\
									\
	return (value);							\
}

DNVLIST_TAKEF(bool, bool)
DNVLIST_TAKEF(uint64_t, number)
DNVLIST_TAKEF(char *, string)
DNVLIST_TAKEF(nvlist_t *, nvlist)
DNVLIST_TAKEF(int, descriptor)

#undef	DNVLIST_TAKEF

void *
dnvlist_takef_binary(nvlist_t *nvl, size_t *sizep, void *defval,
    size_t defsize, const char *namefmt, ...)
{
	va_list nameap;
	void *value;

	va_start(nameap, namefmt);
	value = dnvlist_takev_binary(nvl, sizep, defval, defsize, namefmt,
	    nameap);
	va_end(nameap);

	return (value);
}

#define	DNVLIST_TAKEV(ftype, type)					\
ftype									\
dnvlist_takev_##type(nvlist_t *nvl, ftype defval, const char *namefmt,	\
    va_list nameap)							\
{									\
	char *name;							\
	ftype value;							\
									\
	vasprintf(&name, namefmt, nameap);				\
	if (name == NULL)						\
		return (defval);					\
	value = dnvlist_take_##type(nvl, name, defval);			\
	free(name);							\
	return (value);							\
}

DNVLIST_TAKEV(bool, bool)
DNVLIST_TAKEV(uint64_t, number)
DNVLIST_TAKEV(char *, string)
DNVLIST_TAKEV(nvlist_t *, nvlist)
DNVLIST_TAKEV(int, descriptor)

#undef	DNVLIST_TAKEV

void *
dnvlist_takev_binary(nvlist_t *nvl, size_t *sizep, void *defval,
    size_t defsize, const char *namefmt, va_list nameap)
{
	char *name;
	void *value;

	vasprintf(&name, namefmt, nameap);
	if (name != NULL) {
		value = dnvlist_take_binary(nvl, name, sizep, defval, defsize);
		free(name);
	} else {
		if (sizep != NULL)
			*sizep = defsize;
		value = defval;
	}

	return (value);
}
