/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: string.h,v 1.9 2001/01/09 21:57:37 bwelling Exp $ */

#ifndef ISC_STRING_H
#define ISC_STRING_H 1

#include <string.h>

#include <isc/int.h>
#include <isc/lang.h>
#include <isc/platform.h>

ISC_LANG_BEGINDECLS

isc_uint64_t
isc_string_touint64(char *source, char **endp, int base);
/*
 * Convert the string pointed to by 'source' to isc_uint64_t.
 *
 * On successful conversion 'endp' points to the first character
 * after conversion is complete.
 *
 * 'base': 0 or 2..36
 *
 * If base is 0 the base is computed from the string type.
 *
 * On error 'endp' points to 'source'.
 */


char *
isc_string_separate(char **stringp, const char *delim);

#ifdef ISC_PLATFORM_NEEDSTRSEP
#define strsep isc_string_separate
#endif

ISC_LANG_ENDDECLS

#endif /* ISC_STRING_H */
