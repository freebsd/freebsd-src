/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
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

#ifndef lint
static const char sccsid[] = "@(#)contents.c	8.3 (Berkeley) 4/2/94";
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ar.h>
#include <dirent.h>
#include <fcntl.h>
#include <langinfo.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "extern.h"

/*
 * contents --
 *	Handles t[v] option - opens the archive and then reads headers,
 *	skipping member contents.
 */
int
contents(argv)
	char **argv;
{
	int afd, all;
	struct tm *tp;
	char *file, buf[80];

	afd = open_archive(O_RDONLY);

	for (all = !*argv; get_arobj(afd);) {
		if (all)
			file = chdr.name;
		else if (!(file = files(argv)))
			goto next;
		if (options & AR_V) {
			static int d_first = -1;

			(void)strmode(chdr.mode, buf);
			(void)printf("%s %6d/%-6d %8qd ",
			    buf + 1, chdr.uid, chdr.gid, chdr.size);

			if (d_first < 0)
				d_first = (*nl_langinfo(D_MD_ORDER) == 'd');
			tp = localtime(&chdr.date);
			(void)strftime(buf, sizeof(buf),
				       d_first ? "%e %b %R %Y" :
						 "%b %e %R %Y",
				       tp);
			(void)printf("%s %s\n", buf, file);
		} else
			(void)printf("%s\n", file);
		if (!all && !*argv)
			break;
next:		skip_arobj(afd);
	}
	close_archive(afd);

	if (*argv) {
		orphans(argv);
		return (1);
	}
	return (0);
}
