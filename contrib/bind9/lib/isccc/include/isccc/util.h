/*
 * Portions Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001  Internet Software Consortium.
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NOMINUM DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: util.h,v 1.4.18.2 2005-04-29 00:17:14 marka Exp $ */

#ifndef ISCCC_UTIL_H
#define ISCCC_UTIL_H 1

#include <isc/util.h>

/*! \file
 * \brief
 * Macros for dealing with unaligned numbers.
 *
 * \note no side effects are allowed when invoking these macros!
 */

#define GET8(v, w) \
	do { \
		v = *w; \
		w++; \
	} while (0)

#define GET16(v, w) \
	do { \
		v = (unsigned int)w[0] << 8; \
 		v |= (unsigned int)w[1]; \
		w += 2; \
	} while (0)

#define GET24(v, w) \
	do { \
 		v = (unsigned int)w[0] << 16; \
 		v |= (unsigned int)w[1] << 8; \
 		v |= (unsigned int)w[2]; \
		w += 3; \
	} while (0)

#define GET32(v, w) \
	do { \
		v = (unsigned int)w[0] << 24; \
 		v |= (unsigned int)w[1] << 16; \
 		v |= (unsigned int)w[2] << 8; \
 		v |= (unsigned int)w[3]; \
		w += 4; \
	} while (0)

#define GET64(v, w) \
	do { \
		v = (isc_uint64_t)w[0] << 56; \
 		v |= (isc_uint64_t)w[1] << 48; \
 		v |= (isc_uint64_t)w[2] << 40; \
 		v |= (isc_uint64_t)w[3] << 32; \
 		v |= (isc_uint64_t)w[4] << 24; \
 		v |= (isc_uint64_t)w[5] << 16; \
 		v |= (isc_uint64_t)w[6] << 8; \
 		v |= (isc_uint64_t)w[7]; \
		w += 8; \
	} while (0)

#define GETC16(v, w, d) \
	do { \
		GET8(v, w); \
		if (v == 0) \
			d = ISCCC_TRUE; \
 		else { \
			d = ISCCC_FALSE; \
			if (v == 255) \
				GET16(v, w); \
		} \
	} while (0)

#define GETC32(v, w) \
	do { \
		GET24(v, w); \
 		if (v == 0xffffffu) \
			GET32(v, w); \
	} while (0)

#define GET_OFFSET(v, w)		GET32(v, w)

#define GET_MEM(v, c, w) \
	do { \
		memcpy(v, w, c); \
		w += c; \
	} while (0)

#define GET_TYPE(v, w) \
	do { \
		GET8(v, w); \
		if (v > 127) { \
			if (v < 255) \
				v = ((v & 0x7f) << 16) | ISCCC_RDATATYPE_SIG; \
			else \
				GET32(v, w); \
		} \
	} while (0)

#define PUT8(v, w) \
	do { \
		*w = (v & 0x000000ffU); \
		w++; \
	} while (0)

#define PUT16(v, w) \
	do { \
		w[0] = (v & 0x0000ff00U) >> 8; \
		w[1] = (v & 0x000000ffU); \
		w += 2; \
	} while (0)

#define PUT24(v, w) \
	do { \
		w[0] = (v & 0x00ff0000U) >> 16; \
		w[1] = (v & 0x0000ff00U) >> 8; \
		w[2] = (v & 0x000000ffU); \
		w += 3; \
	} while (0)

#define PUT32(v, w) \
	do { \
		w[0] = (v & 0xff000000U) >> 24; \
		w[1] = (v & 0x00ff0000U) >> 16; \
		w[2] = (v & 0x0000ff00U) >> 8; \
		w[3] = (v & 0x000000ffU); \
		w += 4; \
	} while (0)

#define PUT64(v, w) \
	do { \
		w[0] = (v & 0xff00000000000000ULL) >> 56; \
		w[1] = (v & 0x00ff000000000000ULL) >> 48; \
		w[2] = (v & 0x0000ff0000000000ULL) >> 40; \
		w[3] = (v & 0x000000ff00000000ULL) >> 32; \
		w[4] = (v & 0x00000000ff000000ULL) >> 24; \
		w[5] = (v & 0x0000000000ff0000ULL) >> 16; \
		w[6] = (v & 0x000000000000ff00ULL) >> 8; \
		w[7] = (v & 0x00000000000000ffULL); \
		w += 8; \
	} while (0)

#define PUTC16(v, w) \
	do { \
		if (v > 0 && v < 255) \
			PUT8(v, w); \
		else { \
			PUT8(255, w); \
			PUT16(v, w); \
		} \
	} while (0)

#define PUTC32(v, w) \
	do { \
		if (v < 0xffffffU) \
			PUT24(v, w); \
		else { \
			PUT24(0xffffffU, w); \
			PUT32(v, w); \
		} \
	} while (0)

#define PUT_OFFSET(v, w)		PUT32(v, w)

#include <string.h>

#define PUT_MEM(s, c, w) \
	do { \
		memcpy(w, s, c); \
		w += c; \
	} while (0)

/*
 * Regions.
 */
#define REGION_SIZE(r)		((unsigned int)((r).rend - (r).rstart))
#define REGION_EMPTY(r)		((r).rstart == (r).rend)
#define REGION_FROMSTRING(r, s) do { \
	(r).rstart = (unsigned char *)s; \
	(r).rend = (r).rstart + strlen(s); \
} while (0)

/*%
 * Use this to remove the const qualifier of a variable to assign it to
 * a non-const variable or pass it as a non-const function argument ...
 * but only when you are sure it won't then be changed!
 * This is necessary to sometimes shut up some compilers
 * (as with gcc -Wcast-qual) when there is just no other good way to avoid the
 * situation.
 */
#define DE_CONST(konst, var) \
	do { \
		union { const void *k; void *v; } _u; \
		_u.k = konst; \
		var = _u.v; \
	} while (0)

#endif /* ISCCC_UTIL_H */
