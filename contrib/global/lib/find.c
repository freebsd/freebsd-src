/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	find.c					1-May-98
 *
 */
/*
 * USEFIND	use find(1) to traverse directory tree.
 *		Otherwise, use dirent(3) library.
 */
#define USEFIND

#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#ifndef USEFIND
#include <dirent.h>
#ifndef BSD4_4
#include <sys/stat.h>
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <regex.h>

#include "conf.h"
#include "die.h"
#include "find.h"
#include "gparam.h"
#include "locatestring.h"
#include "makepath.h"
#include "strbuf.h"

/*
 * usage of findxxx()
 *
 *	findopen(db);
 *	while (path = findread(&length)) {
 *		...
 *	}
 *	findclose();
 *
 */
static regex_t	skip_area;
static regex_t	*skip = &skip_area;
static int	opened;

static void	trim __P((char *));

/*
 * trim: remove blanks and '\'.
 */
static void
trim(s)
char	*s;
{
	char	*p;

	for (p = s; *s; s++) {
		if (isspace(*s))
			continue;	
		if (*s == '\\' && *(s + 1))
			s++;
		*p++ = *s;
	}
	*p = 0;
}
#ifdef USEFIND
/*----------------------------------------------------------------------*/
/* find command version							*/
/*----------------------------------------------------------------------*/
static FILE	*ip;

void
findopen()
{
	char	*findcom, *p, *q;
	STRBUF	*sb;
	char	*sufflist = NULL;
	char	*skiplist = NULL;

	assert(opened == 0);
	opened = 1;

	sb = stropen();
	if (!getconfs("suffixes", sb))
		die("cannot get suffixes data.");
	sufflist = strdup(strvalue(sb));
	if (!sufflist)
		die("short of memory.");
	trim(sufflist);
	strstart(sb);
	if (getconfs("skip", sb)) {
		skiplist = strdup(strvalue(sb));
		if (!skiplist)
			die("short of memory.");
		trim(skiplist);
	}

	strstart(sb);
	strputs(sb, "find . \\( -type f -o -type l \\) \\(");
	for (p = sufflist; p; ) {
		char	*suff = p;
		if ((p = locatestring(p, ",", MATCH_FIRST)) != NULL)
			*p++ = 0;
		strputs(sb, " -name '*.");
		strputs(sb, suff);
		strputs(sb, "'");
		if (p)
			strputs(sb, " -o");
	}
	strputs(sb, " \\) -print");
	findcom = strvalue(sb);

	if (skiplist) {
		char	*reg;
		STRBUF	*sbb = stropen();
		/*
		 * construct regular expression.
		 */
		strputc(sbb, '(');	/* ) */
		for (p = skiplist; p; ) {
			char    *skipf = p;
			if ((p = locatestring(p, ",", MATCH_FIRST)) != NULL)
				*p++ = 0;
			strputc(sbb, '/');
			for (q = skipf; *q; q++) {
				if (*q == '.')
					strputc(sbb, '\\');
				strputc(sbb, *q);
			}
			if (*(q - 1) != '/')
				strputc(sbb, '$');
			if (p)
				strputc(sbb, '|');
		}
		strputc(sbb, ')');
		reg = strvalue(sbb);
		/*
		 * compile regular expression.
		 */
		if (regcomp(skip, reg, REG_EXTENDED|REG_NEWLINE) != 0)
			die("cannot compile regular expression.");
		strclose(sbb);
	} else {
		skip = (regex_t *)0;
	}
	if (!(ip = popen(findcom, "r")))
		die("cannot execute find.");
	strclose(sb);
	if (sufflist)
		free(sufflist);
	if (skiplist)
		free(skiplist);
}
char	*
findread(length)
int	*length;
{
	static char	path[MAXPATHLEN+1];
	char	*p;

	assert(opened == 1);
	while (fgets(path, MAXPATHLEN, ip)) {
		if (!skip || regexec(skip, path, 0, 0, 0) != 0) {
			/*
			 * chop(path)
			 */
			p = path + strlen(path) - 1;
			if (*p != '\n')
				die("output of find(1) is wrong (findread).");
			*p = 0;
			if (length)
				*length = p - path;
			return path;
		}
	}
	return NULL;
}
void
findclose(void)
{
	assert(opened == 1);
	pclose(ip);
	opened = 0;
}
#else /* USEFIND */
/*----------------------------------------------------------------------*/
/* dirent version findxxx()						*/
/*----------------------------------------------------------------------*/
#define STACKSIZE 50
static  char    dir[MAXPATHLEN+1];		/* directory path */
static  struct {
	STRBUF  *sb;
	char    *dirp, *start, *end, *p;
} stack[STACKSIZE], *topp, *curp;		/* stack */

static regex_t	suff_area;
static regex_t	*suff = &suff_area;

static int
getdirs(dir, sb)
char    *dir;
STRBUF  *sb;
{
	DIR     *dirp;
	struct dirent *dp;
#ifndef BSD4_4
	struct stat st;
#endif

	if ((dirp = opendir(dir)) == NULL)
		return -1;
	while ((dp = readdir(dirp)) != NULL) {
#ifdef BSD4_4
		if (dp->d_namlen == 1 && dp->d_name[0] == '.')
			continue;
		if (dp->d_namlen == 2 && dp->d_name[0] == '.' && dp->d_name[1] == '.')
			continue;
		if (dp->d_type == DT_DIR)
			strputc(sb, 'd');
		else if (dp->d_type == DT_REG)
			strputc(sb, 'f');
		else if (dp->d_type == DT_LNK)
			strputc(sb, 'l');
		else
			strputc(sb, ' ');
		strnputs(sb, dp->d_name, (int)dp->d_namlen);
#else
		if (stat(path, &st) < 0) {
			fprintf(stderr, "cannot stat '%s'. (Ignored)\n", path);
			continue;
		}
		if (S_ISDIR(st.st_mode))
			strputc(sb, 'd');
		else if (S_ISREG(st.st_mode))
			strputc(sb, 'f');
		else if (S_ISLNK(st.st_mode))
			strputc(sb, 'l');
		else
			strputc(sb, ' ');
		strputs(sb, dp->d_name);
#endif /* BSD4_4 */
		strputc(sb, '\0');
	}
	(void)closedir(dirp);
	return 0;
}
void
findopen()
{
	STRBUF	*sb = stropen();
	char	*sufflist = NULL;
	char	*skiplist = NULL;

	assert(opened == 0);
	opened = 1;

	/*
	 * setup stack.
	 */
	curp = &stack[0];
	topp = curp + STACKSIZE; 
	strcpy(dir, ".");

	curp->dirp = dir + strlen(dir);
	curp->sb = stropen();
	if (getdirs(dir, curp->sb) < 0)
		die("cannot open '.' directory.");
	curp->start = curp->p = strvalue(curp->sb);
	curp->end   = curp->start + strbuflen(curp->sb);

	/*
	 * preparing regular expression.
	 */
	strstart(sb);
	if (!getconfs("suffixes", sb))
		die("cannot get suffixes data.");
	sufflist = strdup(strvalue(sb));
	if (!sufflist)
		die("short of memory.");
	trim(sufflist);
	strstart(sb);
	if (getconfs("skip", sb)) {
		skiplist = strdup(strvalue(sb));
		if (!skiplist)
			die("short of memory.");
		trim(skiplist);
	}
	{
		char    *p;

		strstart(sb);
		strputc(sb, '(');       /* ) */
		for (p = sufflist; p; ) {
			char    *suffp = p;
			if ((p = locatestring(p, ",", MATCH_FIRST)) != NULL)
				*p++ = 0;
			strputs(sb, "\\.");
			strputs(sb, suffp);
			strputc(sb, '$');
			if (p)
				strputc(sb, '|');
		}
		strputc(sb, ')');
		/*
		 * compile regular expression.
		 */
		if (regcomp(suff, strvalue(sb), REG_EXTENDED) != 0)
			die("cannot compile regular expression.");
	}
	if (skiplist) {
		char    *p, *q;
		/*
		 * construct regular expression.
		 */
		strstart(sb);
		strputc(sb, '(');	/* ) */
		for (p = skiplist; p; ) {
			char    *skipf = p;
			if ((p = locatestring(p, ",", MATCH_FIRST)) != NULL)
				*p++ = 0;
			strputc(sb, '/');
			for (q = skipf; *q; q++) {
				if (*q == '.')
					strputc(sb, '\\');
				strputc(sb, *q);
			}
			if (*(q - 1) != '/')
				strputc(sb, '$');
			if (p)
				strputc(sb, '|');
		}
		strputc(sb, ')');
		/*
		 * compile regular expression.
		 */
		if (regcomp(skip, strvalue(sb), REG_EXTENDED) != 0)
			die("cannot compile regular expression.");
	} else {
		skip = (regex_t *)0;
	}
	strclose(sb);
	if (sufflist)
		free(sufflist);
	if (skiplist)
		free(skiplist);
}
char    *
findread(length)
int	*length;
{
	static	char val[MAXPATHLEN+1];

	for (;;) {
		while (curp->p < curp->end) {
			char	type = *(curp->p);
			char    *unit = curp->p + 1;

			curp->p += strlen(curp->p) + 1;
			if (type == 'f' || type == 'l') {
				char	*path = makepath(dir, unit);
				if (regexec(suff, path, 0, 0, 0) != 0)
					continue;
				if (skip && regexec(skip, path, 0, 0, 0) == 0)
					continue;
				strcpy(val, path);
				return val;
			}
			if (type == 'd') {
				STRBUF  *sb = stropen();
				char    *dirp = curp->dirp;

				strcat(dirp, "/");
				strcat(dirp, unit);
				if (getdirs(dir, sb) < 0) {
					fprintf(stderr, "cannot open directory '%s'. (Ignored)\n", dir);
					strclose(sb);
					*(curp->dirp) = 0;
					continue;
				}
				/*
				 * Push stack.
				 */
				if (++curp >= topp)
					die("directory stack over flow.");
				curp->dirp = dirp + strlen(dirp);
				curp->sb = sb;
				curp->start = curp->p = strvalue(sb);
				curp->end   = curp->start + strbuflen(sb);
			}
		}
		strclose(curp->sb);
		curp->sb = NULL;
		if (curp == &stack[0])
			break;
		/*
		 * Pop stack.
		 */
		curp--;
		*(curp->dirp) = 0;
	}
	return NULL;
}
void
findclose(void)
{
	assert(opened == 1);
	for (curp = &stack[0]; curp < topp; curp++)
		if (curp->sb != NULL)
			strclose(curp->sb);
		else
			break;
	opened = 0;
}
#endif /* !USEFIND */
