/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID YOUNG ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DAVID
 * YOUNG BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

#include "cpack.h"

const uint8_t *
nd_cpack_next_boundary(const uint8_t *buf, const uint8_t *p, size_t alignment)
{
	size_t misalignment = (size_t)(p - buf) % alignment;

	if (misalignment == 0)
		return p;

	return p + (alignment - misalignment);
}

/* Advance to the next wordsize boundary. Return NULL if fewer than
 * wordsize bytes remain in the buffer after the boundary.  Otherwise,
 * return a pointer to the boundary.
 */
const uint8_t *
nd_cpack_align_and_reserve(struct cpack_state *cs, size_t wordsize)
{
	const uint8_t *next;

	/* Ensure alignment. */
	next = nd_cpack_next_boundary(cs->c_buf, cs->c_next, wordsize);

	/* Too little space for wordsize bytes? */
	if (next - cs->c_buf + wordsize > cs->c_len)
		return NULL;

	return next;
}

/* Advance by N bytes without returning them. */
int
nd_cpack_advance(struct cpack_state *cs, const size_t toskip)
{
	/* No space left? */
	if (cs->c_next - cs->c_buf + toskip > cs->c_len)
		return -1;
	cs->c_next += toskip;
	return 0;
}

int
nd_cpack_init(struct cpack_state *cs, const uint8_t *buf, size_t buflen)
{
	memset(cs, 0, sizeof(*cs));

	cs->c_buf = buf;
	cs->c_len = buflen;
	cs->c_next = cs->c_buf;

	return 0;
}

/* Unpack a 64-bit unsigned integer. */
int
nd_cpack_uint64(netdissect_options *ndo, struct cpack_state *cs, uint64_t *u)
{
	const uint8_t *next;

	if ((next = nd_cpack_align_and_reserve(cs, sizeof(*u))) == NULL)
		return -1;

	*u = GET_LE_U_8(next);

	/* Move pointer past the uint64_t. */
	cs->c_next = next + sizeof(*u);
	return 0;
}

/* Unpack a 64-bit signed integer. */
int
nd_cpack_int64(netdissect_options *ndo, struct cpack_state *cs, int64_t *u)
{
	const uint8_t *next;

	if ((next = nd_cpack_align_and_reserve(cs, sizeof(*u))) == NULL)
		return -1;

	*u = GET_LE_S_8(next);

	/* Move pointer past the int64_t. */
	cs->c_next = next + sizeof(*u);
	return 0;
}

/* Unpack a 32-bit unsigned integer. */
int
nd_cpack_uint32(netdissect_options *ndo, struct cpack_state *cs, uint32_t *u)
{
	const uint8_t *next;

	if ((next = nd_cpack_align_and_reserve(cs, sizeof(*u))) == NULL)
		return -1;

	*u = GET_LE_U_4(next);

	/* Move pointer past the uint32_t. */
	cs->c_next = next + sizeof(*u);
	return 0;
}

/* Unpack a 32-bit signed integer. */
int
nd_cpack_int32(netdissect_options *ndo, struct cpack_state *cs, int32_t *u)
{
	const uint8_t *next;

	if ((next = nd_cpack_align_and_reserve(cs, sizeof(*u))) == NULL)
		return -1;

	*u = GET_LE_S_4(next);

	/* Move pointer past the int32_t. */
	cs->c_next = next + sizeof(*u);
	return 0;
}

/* Unpack a 16-bit unsigned integer. */
int
nd_cpack_uint16(netdissect_options *ndo, struct cpack_state *cs, uint16_t *u)
{
	const uint8_t *next;

	if ((next = nd_cpack_align_and_reserve(cs, sizeof(*u))) == NULL)
		return -1;

	*u = GET_LE_U_2(next);

	/* Move pointer past the uint16_t. */
	cs->c_next = next + sizeof(*u);
	return 0;
}

/* Unpack a 16-bit signed integer. */
int
nd_cpack_int16(netdissect_options *ndo, struct cpack_state *cs, int16_t *u)
{
	const uint8_t *next;

	if ((next = nd_cpack_align_and_reserve(cs, sizeof(*u))) == NULL)
		return -1;

	*u = GET_LE_S_2(next);

	/* Move pointer past the int16_t. */
	cs->c_next = next + sizeof(*u);
	return 0;
}

/* Unpack an 8-bit unsigned integer. */
int
nd_cpack_uint8(netdissect_options *ndo, struct cpack_state *cs, uint8_t *u)
{
	/* No space left? */
	if ((size_t)(cs->c_next - cs->c_buf) >= cs->c_len)
		return -1;

	*u = GET_U_1(cs->c_next);

	/* Move pointer past the uint8_t. */
	cs->c_next++;
	return 0;
}

/* Unpack an 8-bit signed integer. */
int
nd_cpack_int8(netdissect_options *ndo, struct cpack_state *cs, int8_t *u)
{
	/* No space left? */
	if ((size_t)(cs->c_next - cs->c_buf) >= cs->c_len)
		return -1;

	*u = GET_S_1(cs->c_next);

	/* Move pointer past the int8_t. */
	cs->c_next++;
	return 0;
}
