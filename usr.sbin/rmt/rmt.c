/*
 * Copyright (c) 1983, 1993
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rmt.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

/*
 * rmt
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mtio.h>
#include <errno.h>
#include <fcntl.h>
#include <sgtty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	tape = -1;

char	*record;
int	maxrecsize = -1;

#define	SSIZE	64
char	device[SSIZE];
char	count[SSIZE], mode[SSIZE], pos[SSIZE], op[SSIZE];

char	resp[BUFSIZ];

FILE	*debug;
#define	DEBUG(f)	if (debug) fprintf(debug, f)
#define	DEBUG1(f,a)	if (debug) fprintf(debug, f, a)
#define	DEBUG2(f,a1,a2)	if (debug) fprintf(debug, f, a1, a2)

char	*checkbuf __P((char *, int));
void	 error __P((int));
void	 getstring __P((char *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int rval;
	char c;
	int n, i, cc;

	argc--, argv++;
	if (argc > 0) {
		debug = fopen(*argv, "w");
		if (debug == 0)
			exit(1);
		(void)setbuf(debug, (char *)0);
	}
top:
	errno = 0;
	rval = 0;
	if (read(0, &c, 1) != 1)
		exit(0);
	switch (c) {

	case 'O':
		if (tape >= 0)
			(void) close(tape);
		getstring(device);
		getstring(mode);
		DEBUG2("rmtd: O %s %s\n", device, mode);
		/*
		 * XXX the rmt protocol does not provide a means to
		 * specify the permission bits; allow rw for everyone,
		 * as modified by the users umask
		 */
		tape = open(device, atoi(mode), 0666);
		if (tape < 0)
			goto ioerror;
		goto respond;

	case 'C':
		DEBUG("rmtd: C\n");
		getstring(device);		/* discard */
		if (close(tape) < 0)
			goto ioerror;
		tape = -1;
		goto respond;

	case 'L':
		getstring(count);
		getstring(pos);
		DEBUG2("rmtd: L %s %s\n", count, pos);
		rval = lseek(tape, (off_t)atol(count), atoi(pos));
		if (rval < 0)
			goto ioerror;
		goto respond;

	case 'W':
		getstring(count);
		n = atoi(count);
		DEBUG1("rmtd: W %s\n", count);
		record = checkbuf(record, n);
		for (i = 0; i < n; i += cc) {
			cc = read(0, &record[i], n - i);
			if (cc <= 0) {
				DEBUG("rmtd: premature eof\n");
				exit(2);
			}
		}
		rval = write(tape, record, n);
		if (rval < 0)
			goto ioerror;
		goto respond;

	case 'R':
		getstring(count);
		DEBUG1("rmtd: R %s\n", count);
		n = atoi(count);
		record = checkbuf(record, n);
		rval = read(tape, record, n);
		if (rval < 0)
			goto ioerror;
		(void)sprintf(resp, "A%d\n", rval);
		(void)write(1, resp, strlen(resp));
		(void)write(1, record, rval);
		goto top;

	case 'I':
		getstring(op);
		getstring(count);
		DEBUG2("rmtd: I %s %s\n", op, count);
		{ struct mtop mtop;
		  mtop.mt_op = atoi(op);
		  mtop.mt_count = atoi(count);
		  if (ioctl(tape, MTIOCTOP, (char *)&mtop) < 0)
			goto ioerror;
		  rval = mtop.mt_count;
		}
		goto respond;

	case 'S':		/* status */
		DEBUG("rmtd: S\n");
		{ struct mtget mtget;
		  if (ioctl(tape, MTIOCGET, (char *)&mtget) < 0)
			goto ioerror;
		  rval = sizeof (mtget);
		  (void)sprintf(resp, "A%d\n", rval);
		  (void)write(1, resp, strlen(resp));
		  (void)write(1, (char *)&mtget, sizeof (mtget));
		  goto top;
		}

	default:
		DEBUG1("rmtd: garbage command %c\n", c);
		exit(3);
	}
respond:
	DEBUG1("rmtd: A %d\n", rval);
	(void)sprintf(resp, "A%d\n", rval);
	(void)write(1, resp, strlen(resp));
	goto top;
ioerror:
	error(errno);
	goto top;
}

void
getstring(bp)
	char *bp;
{
	int i;
	char *cp = bp;

	for (i = 0; i < SSIZE; i++) {
		if (read(0, cp+i, 1) != 1)
			exit(0);
		if (cp[i] == '\n')
			break;
	}
	cp[i] = '\0';
}

char *
checkbuf(record, size)
	char *record;
	int size;
{

	if (size <= maxrecsize)
		return (record);
	if (record != 0)
		free(record);
	record = malloc(size);
	if (record == 0) {
		DEBUG("rmtd: cannot allocate buffer space\n");
		exit(4);
	}
	maxrecsize = size;
	while (size > 1024 &&
	       setsockopt(0, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size)) < 0)
		size -= 1024;
	return (record);
}

void
error(num)
	int num;
{

	DEBUG2("rmtd: E %d (%s)\n", num, strerror(num));
	(void)snprintf(resp, sizeof(resp), "E%d\n%s\n", num, strerror(num));
	(void)write(1, resp, strlen(resp));
}
