/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rune.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <arpa/inet.h>
#include <errno.h>
#include <rune.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "un-namespace.h"

_RuneLocale *
_Read_RuneMagi(fp)
	FILE *fp;
{
	char *data;
	void *lastp;
	_RuneLocale *rl;
	_RuneEntry *rr;
	struct stat sb;
	int x, saverr;

	if (_fstat(fileno(fp), &sb) < 0)
		return (NULL);

	if (sb.st_size < sizeof(_RuneLocale)) {
		errno = EFTYPE;
		return (NULL);
	}

	if ((data = malloc(sb.st_size)) == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	errno = 0;
	rewind(fp); /* Someone might have read the magic number once already */
	if (errno) {
		saverr = errno;
		free(data);
		errno = saverr;
		return (NULL);
	}

	if (fread(data, sb.st_size, 1, fp) != 1) {
		saverr = errno;
		free(data);
		errno = saverr;
		return (NULL);
	}

	rl = (_RuneLocale *)data;
	lastp = data + sb.st_size;

	rl->variable = rl + 1;

	if (memcmp(rl->magic, _RUNE_MAGIC_1, sizeof(rl->magic))) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	rl->invalid_rune = ntohl(rl->invalid_rune);
	rl->variable_len = ntohl(rl->variable_len);
	rl->runetype_ext.nranges = ntohl(rl->runetype_ext.nranges);
	rl->maplower_ext.nranges = ntohl(rl->maplower_ext.nranges);
	rl->mapupper_ext.nranges = ntohl(rl->mapupper_ext.nranges);

	for (x = 0; x < _CACHED_RUNES; ++x) {
		rl->runetype[x] = ntohl(rl->runetype[x]);
		rl->maplower[x] = ntohl(rl->maplower[x]);
		rl->mapupper[x] = ntohl(rl->mapupper[x]);
	}

	rl->runetype_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->runetype_ext.ranges + rl->runetype_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	rl->maplower_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->maplower_ext.ranges + rl->maplower_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	rl->mapupper_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->mapupper_ext.ranges + rl->mapupper_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	for (x = 0; x < rl->runetype_ext.nranges; ++x) {
		rr = rl->runetype_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		if ((rr[x].map = ntohl(rr[x].map)) == 0) {
			int len = rr[x].max - rr[x].min + 1;
			rr[x].types = rl->variable;
			rl->variable = rr[x].types + len;
			if (rl->variable > lastp) {
				free(data);
				errno = EFTYPE;
				return (NULL);
			}
			while (len-- > 0)
				rr[x].types[len] = ntohl(rr[x].types[len]);
		} else
			rr[x].types = 0;
	}

	for (x = 0; x < rl->maplower_ext.nranges; ++x) {
		rr = rl->maplower_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		rr[x].map = ntohl(rr[x].map);
	}

	for (x = 0; x < rl->mapupper_ext.nranges; ++x) {
		rr = rl->mapupper_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		rr[x].map = ntohl(rr[x].map);
	}
	if (((char *)rl->variable) + rl->variable_len > (char *)lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	/*
	 * Go out and zero pointers that should be zero.
	 */
	if (!rl->variable_len)
		rl->variable = 0;

	if (!rl->runetype_ext.nranges)
		rl->runetype_ext.ranges = 0;

	if (!rl->maplower_ext.nranges)
		rl->maplower_ext.ranges = 0;

	if (!rl->mapupper_ext.nranges)
		rl->mapupper_ext.ranges = 0;

	return (rl);
}
