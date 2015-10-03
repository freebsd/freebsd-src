/*
 * Copyright (C) 2004, 2005, 2007, 2009, 2013, 2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef NAMED_MAIN_H
#define NAMED_MAIN_H 1

/*! \file */

#ifdef ISC_MAIN_HOOK
#define main(argc, argv) bindmain(argc, argv)
#endif

/*
 * Commandline arguments for named; also referenced in win32/ntservice.c
 */
#define NS_MAIN_ARGS "46c:C:d:E:fFgi:lM:m:n:N:p:P:sS:t:T:U:u:vVx:"

ISC_PLATFORM_NORETURN_PRE void
ns_main_earlyfatal(const char *format, ...)
ISC_FORMAT_PRINTF(1, 2) ISC_PLATFORM_NORETURN_POST;

void
ns_main_earlywarning(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

void
ns_main_setmemstats(const char *);

#endif /* NAMED_MAIN_H */
