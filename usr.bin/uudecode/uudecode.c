/*-
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
static char sccsid[] = "@(#)uudecode.c	8.2 (Berkeley) 4/2/94";
#endif
static const char rcsid[] =
	"$Id: uudecode.c,v 1.4.2.1 1997/08/25 06:27:29 charnier Exp $";
#endif /* not lint */

/*
 * uudecode [file ...]
 *
 * create the specified file, decoding as you go.
 * used with uuencode.
 */
#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *filename;
int cflag, pflag;

static void    usage __P((void));
int	decode __P((void));
int	decode2 __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int rval, ch;

	while ((ch = getopt(argc, argv, "cp")) !=  -1) {
		switch(ch) {
		case 'c':
			cflag = 1; /* multiple uudecode'd files */
			break;
		case 'p':
			pflag = 1; /* print output to stdout */
			break;
		default:
			usage();
		}
	}
        argc -= optind;
        argv += optind;

			
	if (*argv) {
		rval = 0;
		do {
			if (!freopen(filename = *argv, "r", stdin)) {
				warn("%s", *argv);
				rval = 1;
				continue;
			}
			rval |= decode();
		} while (*++argv);
	} else {
		filename = "stdin";
		rval = decode();
	}
	exit(rval);
}

int
decode ()
{
	int flag;

	/* decode only one file per input stream */
	if (!cflag) 
		return(decode2(0));

	/* multiple uudecode'd files */
	for (flag = 0; ; flag++)
		if (decode2(flag))
			return(1);
		else if (feof(stdin))
			break;

	return(0);
}

int
decode2(flag)
	int flag;
{
	struct passwd *pw;
	register int n;
	register char ch, *p;
	int mode, n1;
	char buf[MAXPATHLEN];

	
	/* search for header line */
	do {
		if (!fgets(buf, sizeof(buf), stdin)) {
			if (flag) /* no error */
				return(0);

			warnx("%s: no \"begin\" line", filename);
			return(1);
		}
	} while (strncmp(buf, "begin ", 6));
	(void)sscanf(buf, "begin %o %s", &mode, buf);

	/* handle ~user/file format */
	if (buf[0] == '~') {
		if (!(p = index(buf, '/'))) {
			warnx("%s: illegal ~user", filename);
			return(1);
		}
		*p++ = NULL;
		if (!(pw = getpwnam(buf + 1))) {
			warnx("%s: no user %s", filename, buf);
			return(1);
		}
		n = strlen(pw->pw_dir);
		n1 = strlen(p);
		if (n + n1 + 2 > MAXPATHLEN) {
			warnx("%s: path too long", filename);
			return(1);
		}
		bcopy(p, buf + n + 1, n1 + 1);
		bcopy(pw->pw_dir, buf, n);
		buf[n] = '/';
	}

	/* create output file, set mode */
	if (pflag)
		; /* print to stdout */

	else if (!freopen(buf, "w", stdout) ||
	    fchmod(fileno(stdout), mode&0666)) {
		warn("%s: %s", buf, filename);
		return(1);
	}

	/* for each input line */
	for (;;) {
		if (!fgets(p = buf, sizeof(buf), stdin)) {
			warnx("%s: short file", filename);
			return(1);
		}
#define	DEC(c)	(((c) - ' ') & 077)		/* single character decode */
		/*
		 * `n' is used to avoid writing out all the characters
		 * at the end of the file.
		 */
		if ((n = DEC(*p)) <= 0)
			break;
		for (++p; n > 0; p += 4, n -= 3)
			if (n >= 3) {
				ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
				putchar(ch);
				ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
				putchar(ch);
				ch = DEC(p[2]) << 6 | DEC(p[3]);
				putchar(ch);
			}
			else {
				if (n >= 1) {
					ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
					putchar(ch);
				}
				if (n >= 2) {
					ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
					putchar(ch);
				}
				if (n >= 3) {
					ch = DEC(p[2]) << 6 | DEC(p[3]);
					putchar(ch);
				}
			}
	}
	if (!fgets(buf, sizeof(buf), stdin) || strncmp(buf, "end", 3) || (buf[3] && buf[3] != '\n')) {
		warnx("%s: no \"end\" line", filename);
		return(1);
	}
	return(0);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: uudecode [-cp] [file ...]\n");
	exit(1);
}
