/*
 * Copyright (c) 1987 Regents of the University of California.
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
"@(#) Copyright (c) 1987, 1990 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)cmp.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#define	EXITNODIFF	0
#define	EXITDIFF	1
#define	EXITERR		2

int	all, fd1, fd2, silent;
u_char	buf1[MAXBSIZE], buf2[MAXBSIZE];
char	*file1, *file2;

main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch;
	u_long otoi();

	while ((ch = getopt(argc, argv, "-ls")) != EOF)
		switch (ch) {
		case 'l':		/* print all differences */
			all = 1;
			break;
		case 's':		/* silent run */
			silent = 1;
			break;
		case '-':		/* must be after any flags */
			--optind;
			goto endargs;
		case '?':
		default:
			usage();
		}
endargs:
	argv += optind;
	argc -= optind;

	if (argc < 2 || argc > 4)
		usage();

	if (all && silent) {
		fprintf(stderr,
		    "cmp: only one of -l and -s may be specified.\n");
		exit(EXITERR);
	}
	if (strcmp(file1 = argv[0], "-") == 0)
		fd1 = 0;
	else if ((fd1 = open(file1, O_RDONLY, 0)) < 0)
		error(file1);
	if (strcmp(file2 = argv[1], "-") == 0)
		fd2 = 0;
	else if ((fd2 = open(file2, O_RDONLY, 0)) < 0)
		error(file2);
	if (fd1 == fd2) {
		fprintf(stderr,
		    "cmp: standard input may only be specified once.\n");
		exit(EXITERR);
	}

	/* handle skip arguments */
	if (argc > 2) {
		skip(otoi(argv[2]), fd1, file1);
		if (argc == 4)
			skip(otoi(argv[3]), fd2, file2);
	}
	cmp();
	/*NOTREACHED*/
}

/*
 * skip --
 *	skip first part of file
 */
skip(dist, fd, fname)
	register u_long dist;
	register int fd;
	char *fname;
{
	register int rlen, nread;

	for (; dist; dist -= rlen) {
		rlen = MIN(dist, sizeof(buf1));
		if ((nread = read(fd, buf1, rlen)) != rlen) {
			if (nread < 0)
				error(fname);
			else
				endoffile(fname);
		}
	}
}

cmp()
{
	register u_char	*p1, *p2;
	register int cnt, len1, len2;
	register long byte, line;
	int dfound = 0;

	for (byte = 0, line = 1; ; ) {
		switch (len1 = read(fd1, buf1, MAXBSIZE)) {
		case -1:
			error(file1);
		case 0:
			/*
			 * read of file 1 just failed, find out
			 * if there's anything left in file 2
			 */
			switch (read(fd2, buf2, 1)) {
				case -1:
					error(file2);
					/* NOTREACHED */
				case 0:
					exit(dfound ? EXITDIFF : EXITNODIFF);
					/* NOTREACHED */
				default:
					endoffile(file1);
					break;
			}
		}
		/*
		 * file1 might be stdio, which means that a read of less than
		 * MAXBSIZE might not mean an EOF.  So, read whatever we read
		 * from file1 from file2.
		 */
		if ((len2 = read(fd2, buf2, len1)) == -1)
			error(file2);
		if (bcmp(buf1, buf2, len2)) {
			if (silent)
				exit(EXITDIFF);
			if (all) {
				dfound = 1;
				for (p1 = buf1, p2 = buf2, cnt = len2; cnt--;
				    ++p1, ++p2) {
					++byte;
					if (*p1 != *p2)
						printf("%6ld %3o %3o\n",
						    byte, *p1, *p2);
				}
			} else for (p1 = buf1, p2 = buf2; ; ++p1, ++p2) {
				++byte;
				if (*p1 != *p2) {
					printf("%s %s differ: char %ld, line %ld\n", file1, file2, byte, line);
					exit(EXITDIFF);
				}
				if (*p1 == '\n')
					++line;
			}
		} else {
			byte += len2;
			/*
			 * here's the real performance problem, we've got to
			 * count the stupid lines, which means that -l is a
			 * *much* faster version, i.e., unless you really
			 * *want* to know the line number, run -s or -l.
			 */
			if (!silent && !all)
				for (p1 = buf1, cnt = len2; cnt--; )
					if (*p1++ == '\n')
						++line;
		}
		/*
		 * couldn't read as much from file2 as from file1; checked
		 * here because there might be a difference before we got
		 * to this point, which would have precedence.
		 */
		if (len2 < len1)
			endoffile(file2);
	}
}

/*
 * otoi --
 *	octal/decimal string to u_long
 */
u_long
otoi(s)
	register char *s;
{
	register u_long val;
	register int base;

	base = (*s == '0') ? 8 : 10;
	for (val = 0; isdigit(*s); ++s)
		val = val * base + *s - '0';
	return (val);
}

/*
 * error --
 *	print I/O error message and die
 */
error(filename)
	char *filename;
{
	extern int errno;
	char *strerror();

	if (!silent)
		(void) fprintf(stderr, "cmp: %s: %s\n",
		    filename, strerror(errno));
	exit(EXITERR);
}

/*
 * endoffile --
 *	print end-of-file message and exit indicating the files were different
 */
endoffile(filename)
	char *filename;
{
	/* 32V put this message on stdout, S5 does it on stderr. */
	/* POSIX.2 currently does it on stdout-- Hooray! */
	if (!silent)
		(void) printf("cmp: EOF on %s\n", filename);
	exit(EXITDIFF);
}

/*
 * usage --
 *	print usage and die
 */
usage()
{
	fputs("usage: cmp [-ls] file1 file2 [skip1] [skip2]\n", stderr);
	exit(EXITERR);
}
