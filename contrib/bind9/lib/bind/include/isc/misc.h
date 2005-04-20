/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-1999 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * $Id: misc.h,v 1.2.2.1.4.1 2004/03/09 08:33:31 marka Exp $
 */

#ifndef _ISC_MISC_H
#define _ISC_MISC_H

#include <stdio.h>

#define	bitncmp		__bitncmp
/*#define isc_movefile	__isc_movefile */

extern int		bitncmp(const void *, const void *, int);
extern int		isc_movefile(const char *, const char *);

extern int		isc_gethexstring(unsigned char *, size_t, int, FILE *,
					 int *);
extern void		isc_puthexstring(FILE *, const unsigned char *, size_t,
					 size_t, size_t, const char *);
extern void		isc_tohex(const unsigned char *, size_t, char *);

#endif /*_ISC_MISC_H*/
