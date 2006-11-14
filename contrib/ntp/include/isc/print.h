/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $Id: print.h,v 1.17 2001/02/27 02:19:33 gson Exp $ */

#ifndef ISC_PRINT_H
#define ISC_PRINT_H 1

/***
 *** Imports
 ***/

#include <isc/formatcheck.h>    /* Required for ISC_FORMAT_PRINTF() macro. */
#include <isc/lang.h>
#include <isc/platform.h>

/*
 * This block allows lib/isc/print.c to be cleanly compiled even if
 * the platform does not need it.  The standard Makefile will still
 * not compile print.c or archive print.o, so this is just to make test
 * compilation ("make print.o") easier.
 */
#if !defined(ISC_PLATFORM_NEEDVSNPRINTF) && defined(ISC__PRINT_SOURCE)
#define ISC_PLATFORM_NEEDVSNPRINTF
#endif

/***
 *** Macros
 ***/
#define ISC_PRINT_QUADFORMAT ISC_PLATFORM_QUADFORMAT

/***
 *** Functions
 ***/

#ifdef ISC_PLATFORM_NEEDVSNPRINTF
#include <stdarg.h>
#include <stddef.h>

ISC_LANG_BEGINDECLS

int
isc_print_vsnprintf(char *str, size_t size, const char *format, va_list ap)
     ISC_FORMAT_PRINTF(3, 0);
#define vsnprintf isc_print_vsnprintf

int
isc_print_snprintf(char *str, size_t size, const char *format, ...)
     ISC_FORMAT_PRINTF(3, 4);
#define snprintf isc_print_snprintf

ISC_LANG_ENDDECLS
#endif /* ISC_PLATFORM_NEEDVSNPRINTF */

#endif /* ISC_PRINT_H */
