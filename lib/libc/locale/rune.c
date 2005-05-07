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
__FBSDID("$FreeBSD: src/lib/libc/locale/rune.c,v 1.12 2004/07/29 06:16:19 tjr Exp $");

#include "namespace.h"
#include <arpa/inet.h>
#include <errno.h>
#include <runetype.h>
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

	if ((data = malloc(sb.st_size)) == NULL)
		return (NULL);

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

	rl->__variable = rl + 1;

	if (memcmp(rl->__magic, _RUNE_MAGIC_1, sizeof(rl->__magic))) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	rl->__invalid_rune = ntohl(rl->__invalid_rune);
	rl->__variable_len = ntohl(rl->__variable_len);
	rl->__runetype_ext.__nranges = ntohl(rl->__runetype_ext.__nranges);
	rl->__maplower_ext.__nranges = ntohl(rl->__maplower_ext.__nranges);
	rl->__mapupper_ext.__nranges = ntohl(rl->__mapupper_ext.__nranges);

	for (x = 0; x < _CACHED_RUNES; ++x) {
		rl->__runetype[x] = ntohl(rl->__runetype[x]);
		rl->__maplower[x] = ntohl(rl->__maplower[x]);
		rl->__mapupper[x] = ntohl(rl->__mapupper[x]);
	}

	rl->__runetype_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__runetype_ext.__ranges +
	    rl->__runetype_ext.__nranges;
	if (rl->__variable > lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	rl->__maplower_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__maplower_ext.__ranges +
	    rl->__maplower_ext.__nranges;
	if (rl->__variable > lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	rl->__mapupper_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__mapupper_ext.__ranges +
	    rl->__mapupper_ext.__nranges;
	if (rl->__variable > lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	for (x = 0; x < rl->__runetype_ext.__nranges; ++x) {
		rr = rl->__runetype_ext.__ranges;

		rr[x].__min = ntohl(rr[x].__min);
		rr[x].__max = ntohl(rr[x].__max);
		if ((rr[x].__map = ntohl(rr[x].__map)) == 0) {
			int len = rr[x].__max - rr[x].__min + 1;
			rr[x].__types = rl->__variable;
			rl->__variable = rr[x].__types + len;
			if (rl->__variable > lastp) {
				free(data);
				errno = EFTYPE;
				return (NULL);
			}
			while (len-- > 0)
				rr[x].__types[len] = ntohl(rr[x].__types[len]);
		} else
			rr[x].__types = 0;
	}

	for (x = 0; x < rl->__maplower_ext.__nranges; ++x) {
		rr = rl->__maplower_ext.__ranges;

		rr[x].__min = ntohl(rr[x].__min);
		rr[x].__max = ntohl(rr[x].__max);
		rr[x].__map = ntohl(rr[x].__map);
	}

	for (x = 0; x < rl->__mapupper_ext.__nranges; ++x) {
		rr = rl->__mapupper_ext.__ranges;

		rr[x].__min = ntohl(rr[x].__min);
		rr[x].__max = ntohl(rr[x].__max);
		rr[x].__map = ntohl(rr[x].__map);
	}
	if (((char *)rl->__variable) + rl->__variable_len > (char *)lastp) {
		free(data);
		errno = EFTYPE;
		return (NULL);
	}

	/*
	 * Go out and zero pointers that should be zero.
	 */
	if (!rl->__variable_len)
		rl->__variable = 0;

	if (!rl->__runetype_ext.__nranges)
		rl->__runetype_ext.__ranges = 0;

	if (!rl->__maplower_ext.__nranges)
		rl->__maplower_ext.__ranges = 0;

	if (!rl->__mapupper_ext.__nranges)
		rl->__mapupper_ext.__ranges = 0;

	return (rl);
}
