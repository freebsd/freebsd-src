/*-
 * Copyright (c) 2013 FreeBSD Foundation
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*
 * Note that this file is compiled into the kernel and into libc.
 */

#ifdef _KERNEL
#include <sys/types.h>
#include <sys/capability.h>
#include <sys/systm.h>

#include <machine/stdarg.h>
#else	/* !_KERNEL */
#include <sys/types.h>
#include <sys/capability.h>

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#endif

#ifdef _KERNEL
#define	assert(exp)	KASSERT((exp), ("%s:%u", __func__, __LINE__))
#endif

#define	CAPARSIZE_MIN	(CAP_RIGHTS_VERSION_00 + 2)
#define	CAPARSIZE_MAX	(CAP_RIGHTS_VERSION + 2)

static __inline int
right_to_index(uint64_t right)
{
	static const int bit2idx[] = {
		-1, 0, 1, -1, 2, -1, -1, -1, 3, -1, -1, -1, -1, -1, -1, -1,
		4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};
	int idx;

	idx = CAPIDXBIT(right);
	assert(idx >= 0 && idx < sizeof(bit2idx) / sizeof(bit2idx[0]));
	return (bit2idx[idx]);
}

static void
cap_rights_vset(cap_rights_t *rights, va_list ap)
{
	uint64_t right;
	int i, n;

	assert(CAPVER(rights) == CAP_RIGHTS_VERSION_00);

	n = CAPARSIZE(rights);
	assert(n >= CAPARSIZE_MIN && n <= CAPARSIZE_MAX);

	for (;;) {
		right = (uint64_t)va_arg(ap, unsigned long long);
		if (right == 0)
			break;
		assert(CAPRVER(right) == 0);
		i = right_to_index(right);
		assert(i >= 0);
		assert(i < n);
		assert(CAPIDXBIT(rights->cr_rights[i]) == CAPIDXBIT(right));
		rights->cr_rights[i] |= right;
		assert(CAPIDXBIT(rights->cr_rights[i]) == CAPIDXBIT(right));
	}
}

static void
cap_rights_vclear(cap_rights_t *rights, va_list ap)
{
	uint64_t right;
	int i, n;

	assert(CAPVER(rights) == CAP_RIGHTS_VERSION_00);

	n = CAPARSIZE(rights);
	assert(n >= CAPARSIZE_MIN && n <= CAPARSIZE_MAX);

	for (;;) {
		right = (uint64_t)va_arg(ap, unsigned long long);
		if (right == 0)
			break;
		assert(CAPRVER(right) == 0);
		i = right_to_index(right);
		assert(i >= 0);
		assert(i < n);
		assert(CAPIDXBIT(rights->cr_rights[i]) == CAPIDXBIT(right));
		rights->cr_rights[i] &= ~(right & 0x01FFFFFFFFFFFFFFULL);
		assert(CAPIDXBIT(rights->cr_rights[i]) == CAPIDXBIT(right));
	}
}

static bool
cap_rights_is_vset(const cap_rights_t *rights, va_list ap)
{
	uint64_t right;
	int i, n;

	assert(CAPVER(rights) == CAP_RIGHTS_VERSION_00);

	n = CAPARSIZE(rights);
	assert(n >= CAPARSIZE_MIN && n <= CAPARSIZE_MAX);

	for (;;) {
		right = (uint64_t)va_arg(ap, unsigned long long);
		if (right == 0)
			break;
		assert(CAPRVER(right) == 0);
		i = right_to_index(right);
		assert(i >= 0);
		assert(i < n);
		assert(CAPIDXBIT(rights->cr_rights[i]) == CAPIDXBIT(right));
		if ((rights->cr_rights[i] & right) != right)
			return (false);
	}

	return (true);
}

cap_rights_t *
__cap_rights_init(int version, cap_rights_t *rights, ...)
{
	unsigned int n;
	va_list ap;

	assert(version == CAP_RIGHTS_VERSION_00);

	n = version + 2;
	assert(n >= CAPARSIZE_MIN && n <= CAPARSIZE_MAX);
	memset(rights->cr_rights, 0, sizeof(rights->cr_rights[0]) * n);
	CAP_NONE(rights);
	va_start(ap, rights);
	cap_rights_vset(rights, ap);
	va_end(ap);

	return (rights);
}

cap_rights_t *
__cap_rights_set(cap_rights_t *rights, ...)
{
	va_list ap;

	assert(CAPVER(rights) == CAP_RIGHTS_VERSION_00);

	va_start(ap, rights);
	cap_rights_vset(rights, ap);
	va_end(ap);

	return (rights);
}

cap_rights_t *
__cap_rights_clear(cap_rights_t *rights, ...)
{
	va_list ap;

	assert(CAPVER(rights) == CAP_RIGHTS_VERSION_00);

	va_start(ap, rights);
	cap_rights_vclear(rights, ap);
	va_end(ap);

	return (rights);
}

bool
__cap_rights_is_set(const cap_rights_t *rights, ...)
{
	va_list ap;
	bool ret;

	assert(CAPVER(rights) == CAP_RIGHTS_VERSION_00);

	va_start(ap, rights);
	ret = cap_rights_is_vset(rights, ap);
	va_end(ap);

	return (ret);
}

bool
cap_rights_is_valid(const cap_rights_t *rights)
{
	cap_rights_t allrights;
	int i, j;

	if (CAPVER(rights) != CAP_RIGHTS_VERSION_00)
		return (false);
	if (CAPARSIZE(rights) < CAPARSIZE_MIN ||
	    CAPARSIZE(rights) > CAPARSIZE_MAX) {
		return (false);
	}
	CAP_ALL(&allrights);
	if (!cap_rights_contains(&allrights, rights))
		return (false);
	for (i = 0; i < CAPARSIZE(rights); i++) {
		j = right_to_index(rights->cr_rights[i]);
		if (i != j)
			return (false);
		if (i > 0) {
			if (CAPRVER(rights->cr_rights[i]) != 0)
				return (false);
		}
	}

	return (true);
}

cap_rights_t *
cap_rights_merge(cap_rights_t *dst, const cap_rights_t *src)
{
	unsigned int i, n;

	assert(CAPVER(dst) == CAP_RIGHTS_VERSION_00);
	assert(CAPVER(src) == CAP_RIGHTS_VERSION_00);
	assert(CAPVER(dst) == CAPVER(src));
	assert(cap_rights_is_valid(src));
	assert(cap_rights_is_valid(dst));

	n = CAPARSIZE(dst);
	assert(n >= CAPARSIZE_MIN && n <= CAPARSIZE_MAX);

	for (i = 0; i < n; i++)
		dst->cr_rights[i] |= src->cr_rights[i];

	assert(cap_rights_is_valid(src));
	assert(cap_rights_is_valid(dst));

	return (dst);
}

cap_rights_t *
cap_rights_remove(cap_rights_t *dst, const cap_rights_t *src)
{
	unsigned int i, n;

	assert(CAPVER(dst) == CAP_RIGHTS_VERSION_00);
	assert(CAPVER(src) == CAP_RIGHTS_VERSION_00);
	assert(CAPVER(dst) == CAPVER(src));
	assert(cap_rights_is_valid(src));
	assert(cap_rights_is_valid(dst));

	n = CAPARSIZE(dst);
	assert(n >= CAPARSIZE_MIN && n <= CAPARSIZE_MAX);

	for (i = 0; i < n; i++) {
		dst->cr_rights[i] &=
		    ~(src->cr_rights[i] & 0x01FFFFFFFFFFFFFFULL);
	}

	assert(cap_rights_is_valid(src));
	assert(cap_rights_is_valid(dst));

	return (dst);
}

bool
cap_rights_contains(const cap_rights_t *big, const cap_rights_t *little)
{
	unsigned int i, n;

	assert(CAPVER(big) == CAP_RIGHTS_VERSION_00);
	assert(CAPVER(little) == CAP_RIGHTS_VERSION_00);
	assert(CAPVER(big) == CAPVER(little));

	n = CAPARSIZE(big);
	assert(n >= CAPARSIZE_MIN && n <= CAPARSIZE_MAX);

	for (i = 0; i < n; i++) {
		if ((big->cr_rights[i] & little->cr_rights[i]) !=
		    little->cr_rights[i]) {
			return (false);
		}
	}

	return (true);
}
