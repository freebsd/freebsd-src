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

#ifndef ND_CPACK_H
#define ND_CPACK_H

#include "netdissect.h"

struct cpack_state {
	const uint8_t					*c_buf;
	const uint8_t					*c_next;
	size_t						 c_len;
};

int nd_cpack_init(struct cpack_state *, const uint8_t *, size_t);

int nd_cpack_uint8(netdissect_options *, struct cpack_state *, uint8_t *);
int nd_cpack_int8(netdissect_options *, struct cpack_state *, int8_t *);
int nd_cpack_uint16(netdissect_options *, struct cpack_state *, uint16_t *);
int nd_cpack_int16(netdissect_options *, struct cpack_state *, int16_t *);
int nd_cpack_uint32(netdissect_options *, struct cpack_state *, uint32_t *);
int nd_cpack_int32(netdissect_options *, struct cpack_state *, int32_t *);
int nd_cpack_uint64(netdissect_options *, struct cpack_state *, uint64_t *);
int nd_cpack_int64(netdissect_options *, struct cpack_state *, int64_t *);

const uint8_t *nd_cpack_next_boundary(const uint8_t *buf, const uint8_t *p, size_t alignment);
const uint8_t *nd_cpack_align_and_reserve(struct cpack_state *cs, size_t wordsize);

extern int nd_cpack_advance(struct cpack_state *, const size_t);

#endif /* ND_CPACK_H */
