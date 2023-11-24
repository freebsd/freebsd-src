/*
 * $Id: globals.h,v 1.1 1998/08/14 00:31:23 vixie Exp $
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium
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

#ifdef MAIN_PROGRAM
# define XTRN
# define INIT(x) = x
#else
# define XTRN extern
# define INIT(x)
#endif

XTRN const char *copyright[]
#ifdef MAIN_PROGRAM
	= {
		"@(#) Copyright 1988,1989,1990,1993,1994 by Paul Vixie",
		"@(#) Copyright 1997 by Internet Software Consortium",
		"@(#) All rights reserved",
		NULL
	}
#endif
	;

XTRN const char *MonthNames[]
#ifdef MAIN_PROGRAM
	= {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",\
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",\
		NULL
	}
#endif
	;

XTRN const char *DowNames[]
#ifdef MAIN_PROGRAM
	= {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",\
		NULL
	}
#endif
	;

XTRN const char *ProgramName INIT("amnesia");
XTRN const char *defmailto;
XTRN int	LineNumber INIT(0);
XTRN unsigned	Jitter;
XTRN unsigned	RootJitter;
XTRN time_t	TargetTime INIT(0);
XTRN struct pidfh *pfh;

#if DEBUGGING
XTRN int	DebugFlags INIT(0);
XTRN const char *DebugFlagNames[]
#ifdef MAIN_PROGRAM
	= {
		"ext", "sch", "proc", "pars", "load", "misc", "test", "bit",\
		NULL
	}
#endif
	;
#endif /* DEBUGGING */
