/*
 * Copyright (c) 1985, 1993
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
 *
 * $FreeBSD$
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getusershell.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>

/*
 * Local shells should NOT be added here.  They should be added in
 * /etc/shells.
 */

static char *okshells[] = { _PATH_BSHELL, _PATH_CSHELL, NULL };
static char **curshell, **shells, *strings;
static char **initshells __P((void));

/*
 * Get a list of shells from _PATH_SHELLS, if it exists.
 */
char *
getusershell(void)
{
	char *ret;

	if (curshell == NULL)
		curshell = initshells();
	ret = *curshell;
	if (ret != NULL)
		curshell++;
	return (ret);
}

void
endusershell(void)
{

	if (shells != NULL)
		free(shells);
	shells = NULL;
	if (strings != NULL)
		free(strings);
	strings = NULL;
	curshell = NULL;
}

void
setusershell(void)
{

	curshell = initshells();
}

static char **
initshells()
{
	char **sp, *cp;
	FILE *fp;
	struct stat statb;

	if (shells != NULL)
		free(shells);
	shells = NULL;
	if (strings != NULL)
		free(strings);
	strings = NULL;
	if ((fp = fopen(_PATH_SHELLS, "r")) == NULL)
		return (okshells);
	if (fstat(fileno(fp), &statb) == -1) {
		(void)fclose(fp);
		return (okshells);
	}
	if ((strings = malloc((u_int)statb.st_size)) == NULL) {
		(void)fclose(fp);
		return (okshells);
	}
	shells = calloc((unsigned)statb.st_size / 3, sizeof (char *));
	if (shells == NULL) {
		(void)fclose(fp);
		free(strings);
		strings = NULL;
		return (okshells);
	}
	sp = shells;
	cp = strings;
	while (fgets(cp, MAXPATHLEN + 1, fp) != NULL) {
		while (*cp != '#' && *cp != '/' && *cp != '\0')
			cp++;
		if (*cp == '#' || *cp == '\0')
			continue;
		*sp++ = cp;
		while (!isspace((unsigned char)*cp) && *cp != '#' && *cp != '\0')
			cp++;
		*cp++ = '\0';
	}
	*sp = NULL;
	(void)fclose(fp);
	return (shells);
}
