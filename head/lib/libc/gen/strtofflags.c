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
static char sccsid[] = "@(#)stat_flags.c	8.1 (Berkeley) 5/31/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define longestflaglen	12
static struct {
	char name[longestflaglen + 1];
	char invert;
	u_long flag;
} const mapping[] = {
	/* shorter names per flag first, all prefixed by "no" */
	{ "nosappnd",		0, SF_APPEND	},
	{ "nosappend",		0, SF_APPEND	},
	{ "noarch",		0, SF_ARCHIVED	},
	{ "noarchived",		0, SF_ARCHIVED	},
	{ "noschg",		0, SF_IMMUTABLE	},
	{ "noschange",		0, SF_IMMUTABLE	},
	{ "nosimmutable",	0, SF_IMMUTABLE	},
	{ "nosunlnk",		0, SF_NOUNLINK	},
	{ "nosunlink",		0, SF_NOUNLINK	},
#ifdef SF_SNAPSHOT
	{ "nosnapshot",		0, SF_SNAPSHOT	},
#endif
	{ "nouappnd",		0, UF_APPEND	},
	{ "nouappend",		0, UF_APPEND	},
	{ "nouchg",		0, UF_IMMUTABLE	},
	{ "nouchange",		0, UF_IMMUTABLE	},
	{ "nouimmutable",	0, UF_IMMUTABLE	},
	{ "nodump",		1, UF_NODUMP	},
	{ "noopaque",		0, UF_OPAQUE	},
	{ "nouunlnk",		0, UF_NOUNLINK	},
	{ "nouunlink",		0, UF_NOUNLINK	}
};
#define nmappings	(sizeof(mapping) / sizeof(mapping[0]))

/*
 * fflagstostr --
 *	Convert file flags to a comma-separated string.  If no flags
 *	are set, return the empty string.
 */
char *
fflagstostr(flags)
	u_long flags;
{
	char *string;
	const char *sp;
	char *dp;
	u_long setflags;
	int i;

	if ((string = (char *)malloc(nmappings * (longestflaglen + 1))) == NULL)
		return (NULL);

	setflags = flags;
	dp = string;
	for (i = 0; i < nmappings; i++) {
		if (setflags & mapping[i].flag) {
			if (dp > string)
				*dp++ = ',';
			for (sp = mapping[i].invert ? mapping[i].name :
			    mapping[i].name + 2; *sp; *dp++ = *sp++) ;
			setflags &= ~mapping[i].flag;
		}
	}
	*dp = '\0';
	return (string);
}

/*
 * strtofflags --
 *	Take string of arguments and return file flags.  Return 0 on
 *	success, 1 on failure.  On failure, stringp is set to point
 *	to the offending token.
 */
int
strtofflags(stringp, setp, clrp)
	char **stringp;
	u_long *setp, *clrp;
{
	char *string, *p;
	int i;

	if (setp)
		*setp = 0;
	if (clrp)
		*clrp = 0;
	string = *stringp;
	while ((p = strsep(&string, "\t ,")) != NULL) {
		*stringp = p;
		if (*p == '\0')
			continue;
		for (i = 0; i < nmappings; i++) {
			if (strcmp(p, mapping[i].name + 2) == 0) {
				if (mapping[i].invert) {
					if (clrp)
						*clrp |= mapping[i].flag;
				} else {
					if (setp)
						*setp |= mapping[i].flag;
				}
				break;
			} else if (strcmp(p, mapping[i].name) == 0) {
				if (mapping[i].invert) {
					if (setp)
						*setp |= mapping[i].flag;
				} else {
					if (clrp)
						*clrp |= mapping[i].flag;
				}
				break;
			}
		}
		if (i == nmappings)
			return 1;
	}
	return 0;
}
