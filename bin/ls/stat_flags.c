/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char sccsid[] = "@(#)stat_flags.c	8.2 (Berkeley) 7/28/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <stddef.h>
#include <string.h>

#define	SAPPEND(s) {							\
	if (prefix != NULL)						\
		(void)strcat(string, prefix);				\
	(void)strcat(string, s);					\
	prefix = ",";							\
}

/*
 * flags_to_string --
 *	Convert stat flags to a comma-separated string.  If no flags
 *	are set, return the default string.
 */
char *
flags_to_string(flags, def)
	u_long flags;
	char *def;
{
	static char string[128];
	char *prefix;

	string[0] = '\0';
	prefix = NULL;
	if (flags & UF_APPEND)
		SAPPEND("uappnd");
	if (flags & UF_IMMUTABLE)
		SAPPEND("uchg");
	if (flags & UF_NODUMP)
		SAPPEND("nodump");
	if (flags & UF_OPAQUE)
		SAPPEND("opaque");
	if (flags & SF_APPEND)
		SAPPEND("sappnd");
	if (flags & SF_ARCHIVED)
		SAPPEND("arch");
	if (flags & SF_IMMUTABLE)
		SAPPEND("schg");
	return (prefix == NULL && def != NULL ? def : string);
}

#define	TEST(a, b, f) {							\
	if (!memcmp(a, b, sizeof(b))) {					\
		if (clear) {						\
			if (clrp)					\
				*clrp |= (f);				\
		} else if (setp)					\
			*setp |= (f);					\
		break;							\
	}								\
}

/*
 * string_to_flags --
 *	Take string of arguments and return stat flags.  Return 0 on
 *	success, 1 on failure.  On failure, stringp is set to point
 *	to the offending token.
 */
int
string_to_flags(stringp, setp, clrp)
	char **stringp;
	u_long *setp, *clrp;
{
	int clear;
	char *string, *p;

	clear = 0;
	if (setp)
		*setp = 0;
	if (clrp)
		*clrp = 0;
	string = *stringp;
	while ((p = strsep(&string, "\t ,")) != NULL) {
		*stringp = p;
		if (*p == '\0')
			continue;
		if (p[0] == 'n' && p[1] == 'o') {
			clear = 1;
			p += 2;
		}
		switch (p[0]) {
		case 'a':
			TEST(p, "arch", SF_ARCHIVED);
			TEST(p, "archived", SF_ARCHIVED);
			return (1);
		case 'd':
			clear = !clear;
			TEST(p, "dump", UF_NODUMP);
			return (1);
		case 'o':
			TEST(p, "opaque", UF_OPAQUE);
			return (1);
		case 's':
			TEST(p, "sappnd", SF_APPEND);
			TEST(p, "sappend", SF_APPEND);
			TEST(p, "schg", SF_IMMUTABLE);
			TEST(p, "schange", SF_IMMUTABLE);
			TEST(p, "simmutable", SF_IMMUTABLE);
			return (1);
		case 'u':
			TEST(p, "uappnd", UF_APPEND);
			TEST(p, "uappend", UF_APPEND);
			TEST(p, "uchg", UF_IMMUTABLE);
			TEST(p, "uchange", UF_IMMUTABLE);
			TEST(p, "uimmutable", UF_IMMUTABLE);
			/* FALLTHROUGH */
		default:
			return (1);
		}
	}
	return (0);
}
