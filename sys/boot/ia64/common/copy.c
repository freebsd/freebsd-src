/*-
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/ia64/common/copy.c,v 1.9.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <stand.h>
#include <ia64/include/vmparam.h>

#include "libia64.h"

static void *
va2pa(vm_offset_t va, size_t *len)
{
	uint64_t pa;

	if (va >= IA64_RR_BASE(7)) {
		pa = IA64_RR_MASK(va);
		return ((void *)pa);
	}

	printf("\n%s: va=%lx, *len=%lx\n", __func__, va, *len);
	*len = 0;
	return (NULL);
}

ssize_t
ia64_copyin(const void *src, vm_offset_t va, size_t len)
{
	void *pa;
	ssize_t res;
	size_t sz;

	res = 0;
	while (len > 0) {
		sz = len;
		pa = va2pa(va, &sz);
		if (sz == 0)
			break;
		bcopy(src, pa, sz);
		len -= sz;
		res += sz;
		va += sz;
	}
	return (res);
}

ssize_t
ia64_copyout(vm_offset_t va, void *dst, size_t len)
{
	void *pa;
	ssize_t res;
	size_t sz;

	res = 0;
	while (len > 0) {
		sz = len;
		pa = va2pa(va, &sz);
		if (sz == 0)
			break;
		bcopy(pa, dst, sz);
		len -= sz;
		res += sz;
		va += sz;
	}
	return (res);
}

ssize_t
ia64_readin(int fd, vm_offset_t va, size_t len)
{
	void *pa;
	ssize_t res, s;
	size_t sz;

	res = 0;
	while (len > 0) {
		sz = len;
		pa = va2pa(va, &sz);
		if (sz == 0)
			break;
		s = read(fd, pa, sz);
		if (s <= 0)
			break;
		len -= s;
		res += s;
		va += s;
	}
	return (res);
}
