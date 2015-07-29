/*-
 * Copyright (C) Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

#include "pw.h"

#define INVALID		"invalid"
#define TOOSMALL	"too small"
#define	TOOLARGE	"too large"

uintmax_t
strtounum(const char *numstr, uintmax_t minval, uintmax_t maxval,
    const char **errstrp)
{
	uintmax_t ret = 0;
	char *ep;

	if (minval > maxval) {
		errno = EINVAL;
		if (errstrp != NULL)
			*errstrp = INVALID;
		return (0);
	}

	ret = strtoumax(numstr, &ep, 10);
	if (errno == EINVAL || numstr == ep || *ep != '\0') {
		errno = EINVAL;
		if (errstrp != NULL)
			*errstrp = INVALID;
		return (0);
	} else if ((ret == 0 && errno == ERANGE) || ret < minval) {
		errno = ERANGE;
		if (errstrp != NULL)
			*errstrp = TOOSMALL;
		return (0);
	} else if ((ret == UINTMAX_MAX && errno == ERANGE) || ret > maxval) {
		errno = ERANGE;
		if (errstrp != NULL)
			*errstrp = TOOLARGE;
		return (0);
	}

	return (ret);
}
