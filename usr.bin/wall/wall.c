/*
 * Copyright (c) 1988, 1990, 1993
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
"@(#) Copyright (c) 1988, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)wall.c	8.2 (Berkeley) 11/16/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * This program is not related to David Wall, whose Stanford Ph.D. thesis
 * is entitled "Mechanisms for Broadcast and Selective Broadcast".
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

void	makemsg __P((char *));
static void usage __P((void));
char   *ttymsg __P((struct iovec *, int, char *, int));

#define	IGNOREUSER	"sleeper"

int nobanner;
int mbufsize;
char *mbuf;

/* ARGSUSED */
int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	struct iovec iov;
	struct utmp utmp;
	FILE *fp;
	char *p;
	char line[sizeof(utmp.ut_line) + 1];

	(void)setlocale(LC_CTYPE, "");

	while ((ch = getopt(argc, argv, "n")) != -1)
		switch (ch) {
		case 'n':
			/* undoc option for shutdown: suppress banner */
			if (geteuid() == 0)
				nobanner = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc > 1)
		usage();

	makemsg(*argv);

	if (!(fp = fopen(_PATH_UTMP, "r")))
		err(1, "cannot read %s", _PATH_UTMP);
	iov.iov_base = mbuf;
	iov.iov_len = mbufsize;
	/* NOSTRICT */
	while (fread((char *)&utmp, sizeof(utmp), 1, fp) == 1) {
		if (!utmp.ut_name[0] ||
		    !strncmp(utmp.ut_name, IGNOREUSER, sizeof(utmp.ut_name)))
			continue;
		strncpy(line, utmp.ut_line, sizeof(utmp.ut_line));
		line[sizeof(utmp.ut_line)] = '\0';
		if ((p = ttymsg(&iov, 1, line, 60*5)) != NULL)
			warnx("%s", p);
	}
	exit(0);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: wall [file]\n");
	exit(1);
}

void
makemsg(fname)
	char *fname;
{
	register int cnt;
	register unsigned char ch;
	struct tm *lt;
	struct passwd *pw;
	struct stat sbuf;
	time_t now;
	FILE *fp;
	int fd;
	char *p, *tty, hostname[MAXHOSTNAMELEN], lbuf[256], tmpname[64];
	const char *whom;

	(void)snprintf(tmpname, sizeof(tmpname), "%s/wall.XXXXXX", _PATH_TMP);
	if ((fd = mkstemp(tmpname)) == -1 || !(fp = fdopen(fd, "r+")))
		err(1, "can't open temporary file");
	(void)unlink(tmpname);

	if (!nobanner) {
		tty = ttyname(STDERR_FILENO);
		if (tty == NULL)
			tty = "no tty";

		if (!(whom = getlogin()))
			whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";
		(void)gethostname(hostname, sizeof(hostname));
		(void)time(&now);
		lt = localtime(&now);

		/*
		 * all this stuff is to blank out a square for the message;
		 * we wrap message lines at column 79, not 80, because some
		 * terminals wrap after 79, some do not, and we can't tell.
		 * Which means that we may leave a non-blank character
		 * in column 80, but that can't be helped.
		 */
		(void)fprintf(fp, "\r%79s\r\n", " ");
		(void)snprintf(lbuf, sizeof(lbuf), 
		    "Broadcast Message from %s@%s",
		    whom, hostname);
		(void)fprintf(fp, "%-79.79s\007\007\r\n", lbuf);
		(void)snprintf(lbuf, sizeof(lbuf),
		    "        (%s) at %d:%02d %s...", tty,
		    lt->tm_hour, lt->tm_min, lt->tm_zone);
		(void)fprintf(fp, "%-79.79s\r\n", lbuf);
	}
	(void)fprintf(fp, "%79s\r\n", " ");

	if (fname && !(freopen(fname, "r", stdin)))
		err(1, "can't read %s", fname);
	while (fgets(lbuf, sizeof(lbuf), stdin))
		for (cnt = 0, p = lbuf; (ch = *p) != '\0'; ++p, ++cnt) {
			if (ch == '\r') {
				cnt = 0;
			} else if (cnt == 79 || ch == '\n') {
				for (; cnt < 79; ++cnt)
					putc(' ', fp);
				putc('\r', fp);
				putc('\n', fp);
				cnt = 0;
			} else if (((ch & 0x80) && ch < 0xA0) ||
				   /* disable upper controls */
				   (!isprint(ch) && !isspace(ch) &&
				    ch != '\a' && ch != '\b')
				  ) {
				if (ch & 0x80) {
					ch &= 0x7F;
					putc('M', fp);
					if (++cnt == 79) {
						putc('\r', fp);
						putc('\n', fp);
						cnt = 0;
					}
					putc('-', fp);
					if (++cnt == 79) {
						putc('\r', fp);
						putc('\n', fp);
						cnt = 0;
					}
				}
				if (iscntrl(ch)) {
					ch ^= 040;
					putc('^', fp);
					if (++cnt == 79) {
						putc('\r', fp);
						putc('\n', fp);
						cnt = 0;
					}
				}
				putc(ch, fp);
			} else {
				putc(ch, fp);
			}
		}
	(void)fprintf(fp, "%79s\r\n", " ");
	rewind(fp);

	if (fstat(fd, &sbuf))
		err(1, "can't stat temporary file");
	mbufsize = sbuf.st_size;
	if (!(mbuf = malloc((u_int)mbufsize)))
		err(1, "out of memory");
	if (fread(mbuf, sizeof(*mbuf), mbufsize, fp) != mbufsize)
		err(1, "can't read temporary file");
	(void)close(fd);
}
