/*
 * Ascii magic -- file types that we know based on keywords
 * that can appear anywhere in the file.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "file.h"
#include "names.h"

#ifndef	lint
static char *moduleid = 
	"@(#)ascmagic.c,v 1.2 1993/06/10 00:38:04 jtc Exp";
#endif	/* lint */

			/* an optimisation over plain strcmp() */
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

int
ascmagic(buf, nbytes)
unsigned char *buf;
int nbytes;	/* size actually read */
{
	int i, isblock, has_escapes = 0;
	unsigned char *s;
	char nbuf[HOWMANY+1];	/* one extra for terminating '\0' */
	char *token;
	register struct names *p;

	/* these are easy, do them first */

	/*
	 * for troff, look for . + letter + letter or .\";
	 * this must be done to disambiguate tar archives' ./file
	 * and other trash from real troff input.
	 */
	if (*buf == '.') {
		unsigned char *tp = buf + 1;

		while (isascii(*tp) && isspace(*tp))
			++tp;	/* skip leading whitespace */
		if ((isascii(*tp) && (isalnum(*tp) || *tp=='\\') &&
		    isascii(*(tp+1)) && (isalnum(*(tp+1)) || *tp=='"'))) {
			ckfputs("troff or preprocessor input text", stdout);
			return 1;
		}
	}
	if ((*buf == 'c' || *buf == 'C') && 
	    isascii(*(buf + 1)) && isspace(*(buf + 1))) {
		ckfputs("fortran program text", stdout);
		return 1;
	}

	/* look for tokens from names.h - this is expensive! */
	/* make a copy of the buffer here because strtok() will destroy it */
	s = (unsigned char*) memcpy(nbuf, buf, HOWMANY);
	has_escapes = (memchr(s, '\033', HOWMANY) != NULL);
	while ((token = strtok((char*)s, " \t\n\r\f")) != NULL) {
		s = NULL;	/* make strtok() keep on tokin' */
		for (p = names; p < names + NNAMES; p++) {
			if (STREQ(p->name, token)) {
				ckfputs(types[p->type], stdout);
				if (has_escapes)
					ckfputs(" (with escape sequences)", 
						stdout);
				return 1;
			}
		}
	}

	switch (is_tar(buf)) {
	case 1:
		ckfputs("tar archive", stdout);
		return 1;
	case 2:
		ckfputs("POSIX tar archive", stdout);
		return 1;
	}

	if (i = is_compress(buf, &isblock)) {
		if (zflag) {
			unsigned char *newbuf;
			int newsize;

			if (newsize = uncompress(buf, &newbuf, nbytes)) {
			    tryit(newbuf, newsize);
			    free(newbuf);
			}
			printf(" (%scompressed data - %d bits)",
				isblock ? "block " : "", i);
		}
	 	else printf("%scompressed data - %d bits",
			isblock ? "block " : "", i);
		return 1;
	}

	for (i = 0; i < nbytes; i++) {
		if (!isascii(*(buf+i)))
			return 0;	/* not all ascii */
	}

	/* all else fails, but it is ascii... */
	ckfputs("ascii text", stdout);
	if (has_escapes) {
		ckfputs(" (with escape sequences)", stdout);
	}
	return 1;
}


