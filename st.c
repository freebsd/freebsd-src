/*	$Id: st.c,v 1.16 2018/12/14 01:18:26 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>

#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "roff.h"
#include "libmdoc.h"

#define LINE(x, y) \
	if (0 == strcmp(p, x)) return(y);

const char *
mdoc_a2st(const char *p)
{
LINE("-p1003.1-88",	"IEEE Std 1003.1-1988 (\\(lqPOSIX.1\\(rq)")
LINE("-p1003.1-90",	"IEEE Std 1003.1-1990 (\\(lqPOSIX.1\\(rq)")
LINE("-p1003.1-96",	"ISO/IEC 9945-1:1996 (\\(lqPOSIX.1\\(rq)")
LINE("-p1003.1-2001",	"IEEE Std 1003.1-2001 (\\(lqPOSIX.1\\(rq)")
LINE("-p1003.1-2004",	"IEEE Std 1003.1-2004 (\\(lqPOSIX.1\\(rq)")
LINE("-p1003.1-2008",	"IEEE Std 1003.1-2008 (\\(lqPOSIX.1\\(rq)")
LINE("-p1003.1",	"IEEE Std 1003.1 (\\(lqPOSIX.1\\(rq)")
LINE("-p1003.1b",	"IEEE Std 1003.1b (\\(lqPOSIX.1b\\(rq)")
LINE("-p1003.1b-93",	"IEEE Std 1003.1b-1993 (\\(lqPOSIX.1b\\(rq)")
LINE("-p1003.1c-95",	"IEEE Std 1003.1c-1995 (\\(lqPOSIX.1c\\(rq)")
LINE("-p1003.1g-2000",	"IEEE Std 1003.1g-2000 (\\(lqPOSIX.1g\\(rq)")
LINE("-p1003.1i-95",	"IEEE Std 1003.1i-1995 (\\(lqPOSIX.1i\\(rq)")
LINE("-p1003.2",	"IEEE Std 1003.2 (\\(lqPOSIX.2\\(rq)")
LINE("-p1003.2-92",	"IEEE Std 1003.2-1992 (\\(lqPOSIX.2\\(rq)")
LINE("-p1003.2a-92",	"IEEE Std 1003.2a-1992 (\\(lqPOSIX.2\\(rq)")
LINE("-isoC",		"ISO/IEC 9899:1990 (\\(lqISO\\~C90\\(rq)")
LINE("-isoC-90",	"ISO/IEC 9899:1990 (\\(lqISO\\~C90\\(rq)")
LINE("-isoC-amd1",	"ISO/IEC 9899/AMD1:1995 (\\(lqISO\\~C90, Amendment 1\\(rq)")
LINE("-isoC-tcor1",	"ISO/IEC 9899/TCOR1:1994 (\\(lqISO\\~C90, Technical Corrigendum 1\\(rq)")
LINE("-isoC-tcor2",	"ISO/IEC 9899/TCOR2:1995 (\\(lqISO\\~C90, Technical Corrigendum 2\\(rq)")
LINE("-isoC-99",	"ISO/IEC 9899:1999 (\\(lqISO\\~C99\\(rq)")
LINE("-isoC-2011",	"ISO/IEC 9899:2011 (\\(lqISO\\~C11\\(rq)")
LINE("-iso9945-1-90",	"ISO/IEC 9945-1:1990 (\\(lqPOSIX.1\\(rq)")
LINE("-iso9945-1-96",	"ISO/IEC 9945-1:1996 (\\(lqPOSIX.1\\(rq)")
LINE("-iso9945-2-93",	"ISO/IEC 9945-2:1993 (\\(lqPOSIX.2\\(rq)")
LINE("-ansiC",		"ANSI X3.159-1989 (\\(lqANSI\\~C89\\(rq)")
LINE("-ansiC-89",	"ANSI X3.159-1989 (\\(lqANSI\\~C89\\(rq)")
LINE("-ieee754",	"IEEE Std 754-1985")
LINE("-iso8802-3",	"ISO 8802-3: 1989")
LINE("-iso8601",	"ISO 8601")
LINE("-ieee1275-94",	"IEEE Std 1275-1994 (\\(lqOpen Firmware\\(rq)")
LINE("-xpg3",		"X/Open Portability Guide Issue\\~3 (\\(lqXPG3\\(rq)")
LINE("-xpg4",		"X/Open Portability Guide Issue\\~4 (\\(lqXPG4\\(rq)")
LINE("-xpg4.2",		"X/Open Portability Guide Issue\\~4, Version\\~2 (\\(lqXPG4.2\\(rq)")
LINE("-xbd5",		"X/Open Base Definitions Issue\\~5 (\\(lqXBD5\\(rq)")
LINE("-xcu5",		"X/Open Commands and Utilities Issue\\~5 (\\(lqXCU5\\(rq)")
LINE("-xsh4.2",		"X/Open System Interfaces and Headers Issue\\~4, Version\\~2 (\\(lqXSH4.2\\(rq)")
LINE("-xsh5",		"X/Open System Interfaces and Headers Issue\\~5 (\\(lqXSH5\\(rq)")
LINE("-xns5",		"X/Open Networking Services Issue\\~5 (\\(lqXNS5\\(rq)")
LINE("-xns5.2",		"X/Open Networking Services Issue\\~5.2 (\\(lqXNS5.2\\(rq)")
LINE("-xcurses4.2",	"X/Open Curses Issue\\~4, Version\\~2 (\\(lqXCURSES4.2\\(rq)")
LINE("-susv1",		"Version\\~1 of the Single UNIX Specification (\\(lqSUSv1\\(rq)")
LINE("-susv2",		"Version\\~2 of the Single UNIX Specification (\\(lqSUSv2\\(rq)")
LINE("-susv3",		"Version\\~3 of the Single UNIX Specification (\\(lqSUSv3\\(rq)")
LINE("-susv4",		"Version\\~4 of the Single UNIX Specification (\\(lqSUSv4\\(rq)")
LINE("-svid4",		"System\\~V Interface Definition, Fourth Edition (\\(lqSVID4\\(rq)")

	return NULL;
}
