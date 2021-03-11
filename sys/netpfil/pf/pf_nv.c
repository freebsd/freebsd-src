/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/limits.h>
#include <sys/systm.h>

#include <netpfil/pf/pf_nv.h>

#define	PV_NV_IMPL_UINT(fnname, type, max)					\
	int									\
	fnname(const nvlist_t *nvl, const char *name, type *val)		\
	{									\
		uint64_t raw;							\
		if (! nvlist_exists_number(nvl, name))				\
			return (EINVAL);					\
		raw = nvlist_get_number(nvl, name);				\
		if (raw > max)							\
			return (ERANGE);					\
		*val = (type)raw;						\
		return (0);							\
	}									\
	int									\
	fnname ## _array(const nvlist_t *nvl, const char *name, type *array, 	\
	    size_t maxelems, size_t *nelems)					\
	{									\
		const uint64_t *n;						\
		size_t nitems;							\
		bzero(array, sizeof(type) * maxelems);				\
		if (! nvlist_exists_number_array(nvl, name))			\
			return (EINVAL);					\
		n = nvlist_get_number_array(nvl, name, &nitems);		\
		if (nitems != maxelems)						\
			return (E2BIG);						\
		if (nelems != NULL)						\
			*nelems = nitems;					\
		for (size_t i = 0; i < nitems; i++) {				\
			if (n[i] > max)						\
				return (ERANGE);				\
			array[i] = (type)n[i];					\
		}								\
		return (0);							\
	}
int
pf_nvbinary(const nvlist_t *nvl, const char *name, void *data,
    size_t expected_size)
{
	const uint8_t *nvdata;
	size_t len;

	bzero(data, expected_size);

	if (! nvlist_exists_binary(nvl, name))
		return (EINVAL);

	nvdata = (const uint8_t *)nvlist_get_binary(nvl, name, &len);
	if (len > expected_size)
		return (EINVAL);

	memcpy(data, nvdata, len);

	return (0);
}

PV_NV_IMPL_UINT(pf_nvuint8, uint8_t, UINT8_MAX)
PV_NV_IMPL_UINT(pf_nvuint16, uint16_t, UINT16_MAX);
PV_NV_IMPL_UINT(pf_nvuint32, uint32_t, UINT32_MAX)

int
pf_nvint(const nvlist_t *nvl, const char *name, int *val)
{
	int64_t raw;

	if (! nvlist_exists_number(nvl, name))
		return (EINVAL);

	raw = nvlist_get_number(nvl, name);
	if (raw > INT_MAX || raw < INT_MIN)
		return (ERANGE);

	*val = (int)raw;

	return (0);
}

int
pf_nvstring(const nvlist_t *nvl, const char *name, char *str, size_t maxlen)
{
	int ret;

	if (! nvlist_exists_string(nvl, name))
		return (EINVAL);

	ret = strlcpy(str, nvlist_get_string(nvl, name), maxlen);
	if (ret >= maxlen)
		return (EINVAL);

	return (0);
}
