/*
 * Copyright (c) 1988, 1993
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
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)size.c	8.2 (Berkeley) 12/9/93";
#endif
static const char rcsid[] =
	"$Id: size.c,v 1.1.1.1.8.1 1997/08/12 06:39:16 charnier Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <err.h>
#include <a.out.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int	show __P((int, char *));
static void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, eval;

	while ((ch = getopt(argc, argv, "")) !=  -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	eval = 0;
	if (*argv)
		do {
			eval |= show(argc, *argv);
		} while (*++argv);
	else
		eval |= show(1, "a.out");
	exit(eval);
}

int
show(count, name)
	int count;
	char *name;
{
	static int first = 1;
	struct exec head;
	u_long total;
	int fd;

	if ((fd = open(name, O_RDONLY, 0)) < 0) {
		warn("%s", name);
		return (1);
	}
	if (read(fd, &head, sizeof(head)) != sizeof(head) || N_BADMAG(head)) {
		(void)close(fd);
		warnx("%s: not in a.out format", name);
		return (1);
	}
	(void)close(fd);

	if (first) {
		first = 0;
		(void)printf("text\tdata\tbss\tdec\thex\n");
	}
	total = head.a_text + head.a_data + head.a_bss;
	(void)printf("%lu\t%lu\t%lu\t%lu\t%lx", head.a_text, head.a_data,
	    head.a_bss, total, total);
	if (count > 1)
		(void)printf("\t%s", name);
	(void)printf("\n");
	return (0);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: size [file ...]\n");
	exit(1);
}
