/* result.c

   Cheap knock-off of libisc result table code.   This is just a place-holder
   until the actual libisc merge. */

/*
 * Copyright (c) 1999-2001 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
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
