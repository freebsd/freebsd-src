/* result.c

   Cheap knock-off of libisc result table code.   This is just a place-holder
   until the actual libisc merge. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
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
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include <omapip/omapip_p.h>

static const char *text[ISC_R_NRESULTS] = {
	"success",				/*  0 */
	"out of memory",			/*  1 */
	"timed out",				/*  2 */
	"no available threads",			/*  3 */
	"address not available",		/*  4 */
	"address in use",			/*  5 */
	"permission denied",			/*  6 */
	"no pending connections",		/*  7 */
	"network unreachable",			/*  8 */
	"host unreachable",			/*  9 */
	"network down",				/* 10 */
	"host down",				/* 11 */
	"connection refused",			/* 12 */
	"not enough free resources",		/* 13 */
	"end of file",				/* 14 */
	"socket already bound",			/* 15 */
	"task is done",				/* 16 */
	"lock busy",				/* 17 */
	"already exists",			/* 18 */
	"ran out of space",			/* 19 */
	"operation canceled",			/* 20 */
	"sending events is not allowed",	/* 21 */
	"shutting down",			/* 22 */
	"not found",				/* 23 */
	"unexpected end of input",		/* 24 */
	"failure",				/* 25 */
	"I/O error",				/* 26 */
	"not implemented",			/* 27 */
	"unbalanced parentheses",		/* 28 */
	"no more",				/* 29 */
	"invalid file",				/* 30 */
	"bad base64 encoding",			/* 31 */
	"unexpected token",			/* 32 */
	"quota reached",			/* 33 */
	"unexpected error",			/* 34 */
	"already running",			/* 35 */
	"host unknown",				/* 36 */
	"protocol version mismatch",		/* 37 */
	"protocol error",			/* 38 */
	"invalid argument",			/* 39 */
	"not connected",			/* 40 */
	"data not yet available",		/* 41 */
	"object unchanged",			/* 42 */
	"more than one object matches key",	/* 43 */
	"key conflict",				/* 44 */
	"parse error(s) occurred",		/* 45 */
	"no key specified",			/* 46 */
	"zone TSIG key not known",		/* 47 */
	"invalid TSIG key",			/* 48 */
	"operation in progress",		/* 49 */
	"DNS format error",			/* 50 */
	"DNS server failed",			/* 51 */
	"no such domain",			/* 52 */
	"not implemented",			/* 53 */
	"refused",				/* 54 */
	"domain already exists",		/* 55 */
	"RRset already exists",			/* 56 */
	"no such RRset",			/* 57 */
	"not authorized",			/* 58 */
	"not a zone",				/* 59 */
	"bad DNS signature",			/* 60 */
	"bad DNS key",				/* 61 */
	"clock skew too great",			/* 62 */
	"no root zone",				/* 63 */
	"destination address required",		/* 64 */
	"cross-zone update",			/* 65 */
	"no TSIG signature",			/* 66 */
	"not equal",				/* 67 */
	"connection reset by peer",		/* 68 */
	"unknown attribute"			/* 69 */
};

const char *isc_result_totext (isc_result_t result)
{
	static char ebuf[40];

	if (result >= ISC_R_SUCCESS && result < ISC_R_NRESULTS)
		return text [result];
	sprintf(ebuf, "unknown error: %d", result);
	return ebuf;
}
