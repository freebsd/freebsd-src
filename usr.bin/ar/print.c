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
static char sccsid[] = "@(#)print.c	5.6 (Berkeley) 3/12/91";
#endif /* not lint */

#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include "archive.h"
#include "extern.h"

extern CHDR chdr;			/* converted header */
extern char *archive;			/* archive name */

/*
 * print --
 *	Prints archive members on stdout - if member names given only
 *	print those members, otherwise print all members.
 */
print(argv)
	char **argv;
{
	CF cf;
	register int afd, all;
	char *file;

	afd = open_archive(O_RDONLY);

	/* Read from an archive, write to stdout; pad on read. */
	SETCF(afd, archive, STDOUT_FILENO, "stdout", RPAD);
	for (all = !*argv; get_arobj(afd);) {
		if (all)
			file = chdr.name;
		else if (!(file = files(argv))) {
			skip_arobj(afd);
			continue;
		}
		if (options & AR_V) {
			(void)printf("\n<%s>\n\n", file);
			(void)fflush(stdout);
		}
		copy_ar(&cf, chdr.size);
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
