/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001  Internet Software Consortium.
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

/* $Id: os.h,v 1.4.206.1 2004/03/06 10:21:33 marka Exp $ */

#ifndef RNDC_OS_H
#define RNDC_OS_H 1

#include <isc/lang.h>
#include <stdio.h>

ISC_LANG_BEGINDECLS

FILE *safe_create(const char *filename);
/*
 * Open 'filename' for writing, truncate if necessary.  If the file was
 * created ensure that only the owner can read/write it.
 */

int set_user(FILE *fd, const char *user);
/*
 * Set the owner of the file refernced by 'fd' to 'user'.
 * Returns:
 *   0 		success
 *   -1 	insufficient permissions, or 'user' does not exist.
 */

ISC_LANG_ENDDECLS

#endif
