/*
 * Copyright (c) 1989, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nohup.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void dofile __P((void));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	if (argc < 2)
		usage();

	if (isatty(STDOUT_FILENO))
		dofile();
	if (isatty(STDERR_FILENO) && dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
		/* may have just closed stderr */
		(void)fprintf(stdin, "nohup: %s\n", strerror(errno));
		exit(1);
	}

	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);

	execvp(argv[1], &argv[1]);
	err(1, "%s", argv[1]);
}

void
dofile()
{
	int fd;
	char *p, path[MAXPATHLEN];

#define	FILENAME	"nohup.out"
	p = FILENAME;
	if ((fd = open(p, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR)) >= 0)
		goto dupit;
	if ((p = getenv("HOME"))) {
		(void)strcpy(path, p);
		(void)strcat(path, "/");
		(void)strcat(path, FILENAME);
		if ((fd = open(p = path,
		    O_RDWR|O_CREAT, S_IRUSR | S_IWUSR)) >= 0)
			goto dupit;
	}
	errx(1, "can't open a nohup.out file");

dupit:	(void)lseek(fd, (off_t)0, SEEK_END);
	if (dup2(fd, STDOUT_FILENO) == -1)
		err(1, NULL);
	(void)fprintf(stderr, "sending output to %s\n", p);
}

void
usage()
{
	(void)fprintf(stderr, "usage: nohup command\n");
	exit(1);
}
