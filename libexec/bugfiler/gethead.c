/*
 * Copyright (c) 1986, 1987 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)gethead.c	5.10 (Berkeley) 3/7/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pathnames.h"
#include "bug.h"

static int	chk1(), pbuf();

#define ENT(X)	sizeof(X) - 1, X
HEADER	mailhead[] = {				/* mail headers */
	{ NO, YES,  NULL, ENT("Date:"), },
	{ NO,  NO,  NULL, ENT("From "), },
	{ NO, YES,  NULL, ENT("From:"), },
	{ NO,  NO,  chk1, ENT("Index:"), },
	{ NO, YES,  NULL, ENT("Message-Id:"), },
	{ NO, YES,  NULL, ENT("Reply-To:"), },
	{ NO, YES,  NULL, ENT("Return-Path:"), },
	{ NO,  NO,  pbuf, ENT("Subject:"), },
	{ NO, YES,  NULL, ENT("To:"), },
	{ NO,  NO,  NULL, ENT("Apparently-To:"), },
	{ ERR, }
};

FILE	*dfp;				/* distf file pointer */
char	dir[MAXNAMLEN],			/* subject and folder */
	folder[MAXNAMLEN];

/*
 * gethead --
 *	read mail and bug headers from bug report, construct redist headers
 */
gethead(redist)
	int	redist;
{
	register HEADER	*hp;		/* mail header pointer */

	if (redist) {
		int	fd;
		char	*distf;

		distf = strdup(_PATH_TMP);
		if (!(fd = mkstemp(distf)) || !(dfp = fdopen(fd, "w+")))
			error("can't create redistribution file %s.", distf);
		/* disappear after last reference is closed */
		(void)unlink(distf);
		free(distf);
	}
	if (!freopen(tmpname, "r", stdin))
		error("can't read temporary bug file %s.", tmpname);

	while (fgets(bfr, sizeof(bfr), stdin)) {
		for (hp = mailhead; hp->found != ERR; ++hp)
			if (!hp->found)
				if (!strncmp(hp->tag, bfr, hp->len)) {
					if (hp->valid && !((*(hp->valid))(bfr)))
						break;
					if (!(hp->line = malloc((u_int)(strlen(bfr) + 1))))
						error("malloc failed.", CHN);
					(void)strcpy(hp->line, bfr);
					hp->found = YES;
					break;
				}
		if ((hp->found == ERR || hp->redist) && redist)
			fputs(bfr, dfp);
	}

	if (!mailhead[INDX_TAG].found)
		error("no readable \"Index:\" header in bug report.", CHN);
}

/*
 * chk1 --
 *	parse the "Index:" line into folder and directory
 */
static
chk1(line)
	char	*line;
{
	register char	*C;		/* tmp pointer */
	struct stat	sbuf;		/* existence check */
	char	*index();

	if (sscanf(line, " Index: %s %s ", folder, dir) != 2)
		return(NO);
	if (C = index(folder, '/')) {	/* deal with "bin/from.c" */
		if (C == folder)
			return(NO);
		*C = EOS;
	}
	if (stat(dir, &sbuf) || (sbuf.st_mode & S_IFMT) != S_IFDIR)
		return(NO);
	(void)pbuf(line);
	return(YES);
}

/*
 * pbuf --
 *	kludge so that summary file looks pretty
 */
static
pbuf(line)
	char	*line;
{
	register char	*rp,			/* tmp pointers */
			*wp;

	for (rp = line; *rp == ' ' || *rp == '\t'; ++rp);
	for (wp = line; *rp; ++wp) {
		if ((*wp = *rp++) != ' ' && *wp != '\t')
			continue;
		*wp = ' ';
		while (*rp == ' ' || *rp == '\t')
			++rp;
	}
	if (wp[-1] == ' ')			/* wp can't == line */
		--wp;
	*wp = EOS;
	return(YES);
}
