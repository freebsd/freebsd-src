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
static char sccsid[] = "@(#)append.c	5.6 (Berkeley) 3/12/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "archive.h"
#include "extern.h"

extern char *archive;			/* archive name */

/*
 * append --
 *	Append files to the archive - modifies original archive or creates
 *	a new archive if named archive does not exist.
 */
append(argv)
	char **argv;
{
	register int fd, afd;
	register char *file;
	struct stat sb;
	CF cf;
	int eval;

	afd = open_archive(O_CREAT|O_RDWR);
	if (lseek(afd, (off_t)0, SEEK_END) == (off_t)-1)
		error(archive);

	/* Read from disk, write to an archive; pad on write. */
	SETCF(0, 0, afd, archive, WPAD);
	for (eval = 0; file = *argv++;) {
		if ((fd = open(file, O_RDONLY)) < 0) {
			(void)fprintf(stderr,
			    "ar: %s: %s.\n", file, strerror(errno));
			eval = 1;
			continue;
		}
		if (options & AR_V)
			(void)printf("q - %s\n", file);
		cf.rfd = fd;
		cf.rname = file;
		put_arobj(&cf, &sb);
		(void)close(fd);
	}
	close_archive(afd);
	return(eval);	
}
