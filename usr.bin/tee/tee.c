/*
 * Copyright (c) 1988 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tee.c	5.11 (Berkeley) 5/6/91";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _list {
	struct _list *next;
	int fd;
	char *name;
} LIST;
LIST *head;

main(argc, argv)
	int argc;
	char **argv;
{
	extern int errno, optind;
	register LIST *p;
	register int n, fd, rval, wval;
	register char *bp;
	int append, ch, exitval;
	char *buf;
	off_t lseek();

	append = 0;
	while ((ch = getopt(argc, argv, "ai")) != EOF)
		switch((char)ch) {
		case 'a':
			append = 1;
			break;
		case 'i':
			(void)signal(SIGINT, SIG_IGN);
			break;
		case '?':
		default:
			(void)fprintf(stderr, "usage: tee [-ai] [file ...]\n");
			exit(1);
		}
	argv += optind;
	argc -= optind;

	if (!(buf = malloc((u_int)8 * 1024))) {
		(void)fprintf(stderr, "tee: out of space.\n");
		exit(1);
	}
	add(STDOUT_FILENO, "stdout");
	for (; *argv; ++argv)
		if ((fd = open(*argv, append ? O_WRONLY|O_CREAT|O_APPEND :
		    O_WRONLY|O_CREAT|O_TRUNC, DEFFILEMODE)) < 0)
			(void)fprintf(stderr, "tee: %s: %s.\n",
			    *argv, strerror(errno));
		else
			add(fd, *argv);
	exitval = 0;
	while ((rval = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
		for (p = head; p; p = p->next) {
			n = rval;
			bp = buf;
			do {
				if ((wval = write(p->fd, bp, n)) == -1) {
					(void)fprintf(stderr, "tee: %s: %s.\n",
					    p->name, strerror(errno));
					exitval = 1;
					break;
				}
				bp += wval;
			} while (n -= wval);
		}
	if (rval < 0) {
		(void)fprintf(stderr, "tee: read: %s\n", strerror(errno));
		exit(1);
	}
	exit(exitval);
}

add(fd, name)
	int fd;
	char *name;
{
	LIST *p;

	if (!(p = malloc((u_int)sizeof(LIST)))) {
		(void)fprintf(stderr, "tee: out of space.\n");
		exit(1);
	}
	p->fd = fd;
	p->name = name;
	p->next = head;
	head = p;
}
