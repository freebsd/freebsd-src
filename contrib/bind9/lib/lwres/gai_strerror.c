/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: gai_strerror.c,v 1.14.2.1.10.1 2004/03/06 08:15:30 marka Exp $ */

#include <lwres/netdb.h>

static const char *gai_messages[] = {
	"no error",
	"address family for hostname not supported",
	"temporary failure in name resolution",
	"invalid value for ai_flags",
	"non-recoverable failure in name resolution",
	"ai_family not supported",
	"memory allocation failure",
	"no address associated with hostname",
	"hostname nor servname provided, or not known",
	"servname not supported for ai_socktype",
	"ai_socktype not supported",
	"system error returned in errno",
	"bad hints",
	"bad protocol"
};

char *
lwres_gai_strerror(int ecode) {
	union {
		const char *const_ptr;
		char *deconst_ptr;
	} ptr;

	if ((ecode < 0) ||
	    (ecode >= (int)(sizeof(gai_messages)/sizeof(*gai_messages))))
		ptr.const_ptr = "invalid error code";
	else
		ptr.const_ptr = gai_messages[ecode];
	return (ptr.deconst_ptr);
}
