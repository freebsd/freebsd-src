/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *
 *	@(#)cd9660_util.c	8.3 (Berkeley) 12/5/94
 * $Id: cd9660_util.c,v 1.9 1997/02/22 09:38:50 peter Exp $
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include <isofs/cd9660/iso.h>

/*
 * translate and compare a filename
 * Note: Version number plus ';' may be omitted.
 */
int
isofncmp(fn, fnlen, isofn, isolen)
	u_char *fn;
	int fnlen;
	u_char *isofn;
	int isolen;
{
	int i, j;
	unsigned char c;

	while (--fnlen >= 0) {
		if (--isolen < 0)
			return *fn;
		if ((c = *isofn++) == ';') {
			switch (*fn++) {
			default:
				return *--fn;
			case 0:
				return 0;
			case ';':
				break;
			}
			for (i = 0; --fnlen >= 0; i = i * 10 + *fn++ - '0') {
				if (*fn < '0' || *fn > '9') {
					return -1;
				}
			}
			for (j = 0; --isolen >= 0; j = j * 10 + *isofn++ - '0');
			return i - j;
		}
		if (c != *fn) {
			if (c >= 'A' && c <= 'Z') {
				if (c + ('a' - 'A') != *fn) {
					if (*fn >= 'a' && *fn <= 'z')
						return *fn - ('a' - 'A') - c;
					else
						return *fn - c;
				}
			} else
				return *fn - c;
		}
		fn++;
	}
	if (isolen > 0) {
		switch (*isofn) {
		default:
			return -1;
		case '.':
			if (isofn[1] != ';')
				return -1;
		case ';':
			return 0;
		}
	}
	return 0;
}

/*
 * translate a filename
 */
void
isofntrans(infn, infnlen, outfn, outfnlen, original, assoc)
	u_char *infn;
	int infnlen;
	u_char *outfn;
	u_short *outfnlen;
	int original;
	int assoc;
{
	int fnidx = 0;

	if (assoc) {
		*outfn++ = ASSOCCHAR;
		fnidx++;
		infnlen++;
	}
	for (; fnidx < infnlen; fnidx++) {
		char c = *infn++;

		if (!original && c >= 'A' && c <= 'Z')
			*outfn++ = c + ('a' - 'A');
		else if (!original && c == '.' && *infn == ';')
			break;
		else if (!original && c == ';')
			break;
		else
			*outfn++ = c;
	}
	*outfnlen = fnidx;
}
