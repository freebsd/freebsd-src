/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *      This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	find.c					20-Oct-97
 *
 */
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <sys/param.h>
#include "gparam.h"
#include "find.h"
#include "die.h"
#include "locatestring.h"

/*
 * usage of findxxx()
 *
 *	findopen();
 *	while (path = findread(&length)) {
 *		...
 *	}
 *	findclose();
 *
 */
static char	*skippath[] = {
	"y.tab.c",
	"y.tab.h",
	"SCCS/",
	"RCS/",
};
static char	*ext[] = {
	"c",
	"h",
	"y",
	"s",
	"S",
};
static char	findcom[MAXCOMLINE+1];
static regex_t	skip_area;
static regex_t	*skip;
static FILE	*ip;
static int	opened;

int
issource(path)
char	*path;
{
	char	c, *p, *q;

	if (!(p = locatestring(path, ".", 2)))
		return 0;
	++p;
	if (sizeof(ext) != 0) {
		int	i, lim = sizeof(ext)/sizeof(char *);
		for (i = 0; i < lim; i++)
			if (*ext[i] == *p && !strcmp(ext[i], p))
				return 1;
	}
	return 0;
}

void
findopen()
{
	char	edit[512], *p, *q;
	int	i, lim;

	if (opened)
		die("nested call to findopen.");
	opened = 1;
	p = findcom;
	strcpy(p, "find . \\( -type f -o -type l \\) \\(");
	p += strlen(p);
	lim = sizeof(ext)/sizeof(char *);
	for (i = 0; i < lim; i++) {
		sprintf(p, " -name '*.%s'%s", ext[i], (i + 1 < lim) ? " -o" : "");
		p += strlen(p);
	}
	sprintf(p, " \\) -print");
	if (sizeof(skippath) != 0) {
		int	i, lim = sizeof(skippath)/sizeof(char *);
		/*
		 * construct regular expression.
		 */
		p = edit;
		*p++ = '(';
		for (i = 0; i < lim; i++) {
			*p++ = '/';
			for (q = skippath[i]; *q; q++) {
				if (*q == '.')
					*p++ = '\\';
				*p++ = *q;
			}
			if (*(q - 1) != '/')
				*p++ = '$';
			*p++ = '|';
		}
		*(p - 1) = ')';
		*p = 0;
		/*
		 * compile regular expression.
		 */
		skip = &skip_area;
		if (regcomp(skip, edit, REG_EXTENDED|REG_NEWLINE) != 0)
			die("cannot compile regular expression.");
	} else {
		skip = (regex_t *)0;
	}
	if (!(ip = popen(findcom, "r")))
		die("cannot execute find.");
}
char	*
findread(length)
int	*length;
{
	static char	path[MAXPATHLEN+1];
	char	*p;

	while (fgets(path, MAXPATHLEN, ip)) {
		if (!skip || regexec(skip, path, 0, 0, 0) != 0) {
			p = path + strlen(path) - 1;
			if (*p != '\n')
				die("output of find(1) is wrong (findread).");
			*p = 0;
			if (length)
				*length = p - path;
			return path;
		}
	}
	return (char *)0;
}
void
findclose()
{
	pclose(ip);
	opened = 0;
}
