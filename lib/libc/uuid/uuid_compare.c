/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * Copyright (c) 2002 Hiten Mahesh Pandya
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <string.h>
#include <uuid.h>

/*
 * uuid_compare() - compare two UUIDs.
 * See also:
 *	http://www.opengroup.org/onlinepubs/009629399/uuid_compare.htm
 *
 * NOTE: Either UUID can be NULL, meaning a nil UUID. nil UUIDs are smaller
 *	 than any non-nil UUID.
 */
int32_t
uuid_compare(uuid_t *a, uuid_t *b, uint32_t *status)
{
	int res;

	if (status != NULL)
		*status = uuid_s_ok;

	/* Deal with NULL or equal pointers. */
	if (a == b)
		return (0);
	if (a == NULL)
		return ((uuid_is_nil(b, NULL)) ? 0 : -1);
	if (b == NULL)
		return ((uuid_is_nil(a, NULL)) ? 0 : 1);

	/* We have to compare the hard way. */
	res = (int)((int64_t)a->time_low - (int64_t)b->time_low);
	if (res)
		return ((res < 0) ? -1 : 1);
	res = (int)a->time_mid - (int)b->time_mid;
	if (res)
		return ((res < 0) ? -1 : 1);
	res = (int)a->time_hi_and_version - (int)b->time_hi_and_version;
	if (res)
		return ((res < 0) ? -1 : 1);
	res = (int)a->clock_seq_hi_and_reserved -
	    (int)b->clock_seq_hi_and_reserved;
	if (res)
		return ((res < 0) ? -1 : 1);
	res = (int)a->clock_seq_low - (int)b->clock_seq_low;
	if (res)
		return ((res < 0) ? -1 : 1);
	res = memcmp(a->node, b->node, sizeof(a->node));
	if (res)
		return ((res < 0) ? -1 : 1);
	return (0);
}
