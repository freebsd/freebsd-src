/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)extract.c	5.5 (Berkeley) 3/12/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "archive.h"
#include "extern.h"

extern CHDR chdr;			/* converted header */
extern char *archive;			/* archive name */

/*
 * extract --
 *	Extract files from the named archive - if member names given only
 *	extract those members otherwise extract all members.  If 'o' option
 *	selected modify date of newly created file to be same as archive
 *	members date otherwise date is time of extraction.  Does not modify
 *	archive.
 */
extract(argv)
	char **argv;
{
	register int afd, all, tfd;
	struct timeval tv[2];
	struct stat sb;
	CF cf;
	int eval;
	char *file;

	eval = 0;
	tv[0].tv_usec = tv[1].tv_usec = 0;

	afd = open_archive(O_RDONLY);

	/* Read from an archive, write to disk; pad on read. */
	SETCF(afd, archive, 0, 0, RPAD);
	for (all = !*argv; get_arobj(afd);) {
		if (all)
			file = chdr.name;
		else if (!(file = files(argv))) {
			skip_arobj(afd);
			continue;
		}

		if (options & AR_U && !stat(file, &sb) &&
		    sb.st_mtime > chdr.date)
			continue;

		if ((tfd = open(file, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR)) < 0) {
			(void)fprintf(stderr, "ar: %s: %s.\n",
			    file, strerror(errno));
			skip_arobj(afd);
			eval = 1;
			continue;
		}

		if (options & AR_V)
			(void)printf("x - %s\n", file);

		cf.wfd = tfd;
		cf.wname = file;
		copy_ar(&cf, chdr.size);

		if (fchmod(tfd, (short)chdr.mode)) {
			(void)fprintf(stderr, "ar: %s: chmod: %s\n",
			    file, strerror(errno));
			eval = 1;
		}
		if (options & AR_O) {
			tv[0].tv_sec = tv[1].tv_sec = chdr.date;
			if (utimes(file, tv)) {
				(void)fprintf(stderr, "ar: %s: utimes: %s\n",
				    file, strerror(errno));
				eval = 1;
			}
		}
		(void)close(tfd);
		if (!all && !*argv)
			break;
	}
	close_archive(afd);

	if (*argv) {
		orphans(argv);
		return(1);
	}
	return(0);
}	
