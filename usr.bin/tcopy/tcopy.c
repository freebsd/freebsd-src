/*
 * Copyright (c) 1985, 1987, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1985, 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tcopy.c	8.2 (Berkeley) 4/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

#define	MAXREC	(64 * 1024)
#define	NOCOUNT	(-2)

int	filen, guesslen, maxblk = MAXREC;
u_long	lastrec, record, size, tsize;
FILE	*msg = stdout;

void	*getspace __P((int));
void	 intr __P((int));
void	 usage __P((void));
void	 verify __P((int, int, char *));
void	 writeop __P((int, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int lastnread, nread, nw, inp, outp;
	enum {READ, VERIFY, COPY, COPYVERIFY} op = READ;
	sig_t oldsig;
	int ch, needeof;
	char *buff, *inf;

	guesslen = 1;
	while ((ch = getopt(argc, argv, "cs:vx")) != EOF)
		switch((char)ch) {
		case 'c':
			op = COPYVERIFY;
			break;
		case 's':
			maxblk = atoi(optarg);
			if (maxblk <= 0) {
				fprintf(stderr, "tcopy: illegal block size\n");
				usage();
			}
			guesslen = 0;
			break;
		case 'v':
			op = VERIFY;
			break;
		case 'x':
			msg = stderr;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch(argc) {
	case 0:
		if (op != READ)
			usage();
		inf = _PATH_DEFTAPE;
		break;
	case 1:
		if (op != READ)
			usage();
		inf = argv[0];
		break;
	case 2:
		if (op == READ)
			op = COPY;
		inf = argv[0];
		if ((outp = open(argv[1], op == VERIFY ? O_RDONLY :
		    op == COPY ? O_WRONLY : O_RDWR, DEFFILEMODE)) < 0) {
			perror(argv[1]);
			exit(3);
		}
		break;
	default:
		usage();
	}

	if ((inp = open(inf, O_RDONLY, 0)) < 0) {
		perror(inf);
		exit(1);
	}

	buff = getspace(maxblk);

	if (op == VERIFY) {
		verify(inp, outp, buff);
		exit(0);
	}

	if ((oldsig = signal(SIGINT, SIG_IGN)) != SIG_IGN)
		(void) signal(SIGINT, intr);

	needeof = 0;
	for (lastnread = NOCOUNT;;) {
		if ((nread = read(inp, buff, maxblk)) == -1) {
			while (errno == EINVAL && (maxblk -= 1024)) {
				nread = read(inp, buff, maxblk);
				if (nread >= 0)
					goto r1;
			}
			fprintf(stderr, "read error, file %d, record %ld: ",
			    filen, record);
			perror("");
			exit(1);
		} else if (nread != lastnread) {
			if (lastnread != 0 && lastnread != NOCOUNT) {
				if (lastrec == 0 && nread == 0)
					fprintf(msg, "%ld records\n", record);
				else if (record - lastrec > 1)
					fprintf(msg, "records %ld to %ld\n",
					    lastrec, record);
				else
					fprintf(msg, "record %ld\n", lastrec);
			}
			if (nread != 0)
				fprintf(msg, "file %d: block size %d: ",
				    filen, nread);
			(void) fflush(stdout);
			lastrec = record;
		}
r1:		guesslen = 0;
		if (nread > 0) {
			if (op == COPY || op == COPYVERIFY) {
				if (needeof) {
					writeop(outp, MTWEOF);
					needeof = 0;
				}
				nw = write(outp, buff, nread);
				if (nw != nread) {
				    fprintf(stderr,
					"write error, file %d, record %ld: ",
					filen, record);
				    if (nw == -1)
					perror("");
				    else
					fprintf(stderr,
					    "write (%d) != read (%d)\n",
					    nw, nread);
				    fprintf(stderr, "copy aborted\n");
				    exit(5);
				}
			}
			size += nread;
			record++;
		} else {
			if (lastnread <= 0 && lastnread != NOCOUNT) {
				fprintf(msg, "eot\n");
				break;
			}
			fprintf(msg,
			    "file %d: eof after %lu records: %lu bytes\n",
			    filen, record, size);
			needeof = 1;
			filen++;
			tsize += size;
			size = record = lastrec = 0;
			lastnread = 0;
		}
		lastnread = nread;
	}
	fprintf(msg, "total length: %lu bytes\n", tsize);
	(void)signal(SIGINT, oldsig);
	if (op == COPY || op == COPYVERIFY) {
		writeop(outp, MTWEOF);
		writeop(outp, MTWEOF);
		if (op == COPYVERIFY) {
			writeop(outp, MTREW);
			writeop(inp, MTREW);
			verify(inp, outp, buff);
		}
	}
	exit(0);
}

void
verify(inp, outp, outb)
	register int inp, outp;
	register char *outb;
{
	register int eot, inmaxblk, inn, outmaxblk, outn;
	register char *inb;

	inb = getspace(maxblk);
	inmaxblk = outmaxblk = maxblk;
	for (eot = 0;; guesslen = 0) {
		if ((inn = read(inp, inb, inmaxblk)) == -1) {
			if (guesslen)
				while (errno == EINVAL && (inmaxblk -= 1024)) {
					inn = read(inp, inb, inmaxblk);
					if (inn >= 0)
						goto r1;
				}
			perror("tcopy: read error");
			break;
		}
r1:		if ((outn = read(outp, outb, outmaxblk)) == -1) {
			if (guesslen)
				while (errno == EINVAL && (outmaxblk -= 1024)) {
					outn = read(outp, outb, outmaxblk);
					if (outn >= 0)
						goto r2;
				}
			perror("tcopy: read error");
			break;
		}
r2:		if (inn != outn) {
			fprintf(msg,
			    "%s: tapes have different block sizes; %d != %d.\n",
			    "tcopy", inn, outn);
			break;
		}
		if (!inn) {
			if (eot++) {
				fprintf(msg, "tcopy: tapes are identical.\n");
				return;
			}
		} else {
			if (bcmp(inb, outb, inn)) {
				fprintf(msg,
				    "tcopy: tapes have different data.\n");
				break;
			}
			eot = 0;
		}
	}
	exit(1);
}

void
intr(signo)
	int signo;
{
	if (record)
		if (record - lastrec > 1)
			fprintf(msg, "records %ld to %ld\n", lastrec, record);
		else
			fprintf(msg, "record %ld\n", lastrec);
	fprintf(msg, "interrupt at file %d: record %ld\n", filen, record);
	fprintf(msg, "total length: %ld bytes\n", tsize + size);
	exit(1);
}

void *
getspace(blk)
	int blk;
{
	void *bp;

	if ((bp = malloc((size_t)blk)) == NULL) {
		fprintf(stderr, "tcopy: no memory\n");
		exit(11);
	}
	return (bp);
}

void
writeop(fd, type)
	int fd, type;
{
	struct mtop op;

	op.mt_op = type;
	op.mt_count = (daddr_t)1;
	if (ioctl(fd, MTIOCTOP, (char *)&op) < 0) {
		perror("tcopy: tape op");
		exit(6);
	}
}

void
usage()
{
	fprintf(stderr, "usage: tcopy [-cvx] [-s maxblk] src [dest]\n");
	exit(1);
}
