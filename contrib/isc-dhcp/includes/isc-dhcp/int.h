/*
 * Copyright (C) 1999  Internet Software Consortium.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef ISC_INT_H
#define ISC_INT_H 1

#include <isc-dhcp/lang.h>

ISC_LANG_BEGINDECLS

typedef char				isc_int8_t;
typedef unsigned char			isc_uint8_t;
typedef short				isc_int16_t;
typedef unsigned short			isc_uint16_t;
typedef int				isc_int32_t;
typedef unsigned int			isc_uint32_t;
typedef long long			isc_int64_t;
typedef unsigned long long		isc_uint64_t;

ISC_LANG_ENDDECLS

#endif /* ISC_INT_H */
