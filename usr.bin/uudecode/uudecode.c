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

#if 0
#ifndef lint
static char sccsid[] = "@(#)uudecode.c	8.2 (Berkeley) 4/2/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * uudecode [file ...]
 *
 * create the specified file, decoding as you go.
 * used with uuencode.
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <err.h>
#include <pwd.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *filename;
char *outfile;
int cflag, iflag, oflag, pflag, sflag;

static void usage(void);
int	decode(void);
int	decode2(int);
void	base64_decode(const char *);

int
main(int argc, char *argv[])
{
	int rval, ch;

	while ((ch = getopt(argc, argv, "cio:ps")) != -1) {
		switch(ch) {
		case 'c':
			if (oflag)
				usage();
			cflag = 1; /* multiple uudecode'd files */
			break;
		case 'i':
			iflag = 1; /* ask before override files */
			break;
		case 'o':
			if (cflag || pflag || sflag)
				usage();
			oflag = 1; /* output to the specified file */
			sflag = 1; /* do not strip pathnames for output */
			outfile = optarg; /* set the output filename */
			if (strcmp(outfile, "/dev/stdout") == 0)
				pflag = 1;
			break;
		case 'p':
			if (oflag)
				usage();
			pflag = 1; /* print output to stdout */
			break;
		case 's':
			if (oflag)
				usage();
			sflag = 1; /* do not strip pathnames for output */
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
decode(void)
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
decode2(int flag)
{
	struct passwd *pw;
	register int n;
	register char ch, *p;
	int base64, ignore, n1;
	char buf[MAXPATHLEN+1];
	char buffn[MAXPATHLEN+1]; /* file name buffer */
	char *mode, *s;
	void *mode_handle;

	base64 = ignore = 0;
	/* search for header line */
	do {
		if (!fgets(buf, sizeof(buf), stdin)) {
			if (flag) /* no error */
				return(0);

			warnx("%s: no \"begin\" line", filename);
			return(1);
		}
	} while (strncmp(buf, "begin", 5) != 0);

	if (strncmp(buf, "begin-base64", 12) == 0)
		base64 = 1;

	/* Parse the header: begin{,-base64} mode outfile. */
	s = strtok(buf, " ");
	if (s == NULL)
		errx(1, "no mode or filename in input file");
	s = strtok(NULL, " ");
	if (s == NULL)
		errx(1, "no mode in input file");
	else {
		mode = strdup(s);
		if (mode == NULL)
			err(1, "strdup()");
	}
	if (!oflag) {
		outfile = strtok(NULL, "\r\n");
		if (outfile == NULL)
			errx(1, "no filename in input file");
	}

	if (strlcpy(buf, outfile, sizeof(buf)) >= sizeof(buf))
		errx(1, "%s: filename too long", outfile);
	if (!sflag && !pflag) {
		strlcpy(buffn, buf, sizeof(buffn));
		if (strrchr(buffn, '/') != NULL)
			strncpy(buf, strrchr(buffn, '/') + 1, sizeof(buf));
		if (buf[0] == '\0') {
			warnx("%s: illegal filename", buffn);
			return(1);
		}

		/* handle ~user/file format */
		if (buf[0] == '~') {
			if (!(p = index(buf, '/'))) {
				warnx("%s: illegal ~user", filename);
				return(1);
			}
			*p++ = '\0';
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
	}

	/* create output file, set mode */
	if (pflag)
		; /* print to stdout */

	else {
		mode_handle = setmode(mode);
		if (mode_handle == NULL)
			err(1, "setmode()");
		if (iflag && !access(buf, F_OK)) {
			(void)fprintf(stderr, "not overwritten: %s\n", buf);
			ignore++;
		} else if (!freopen(buf, "w", stdout) ||
		    fchmod(fileno(stdout), getmode(mode_handle, 0) & 0666)) {
			warn("%s: %s", buf, filename);
			return(1);
		}
		free(mode_handle);
		free(mode);
	}
	strcpy(buffn, buf); /* store file name from header line */

	/* for each input line */
next:
	for (;;) {
		if (!fgets(p = buf, sizeof(buf), stdin)) {
			warnx("%s: short file", filename);
			return(1);
		}
		if (base64) {
			if (strncmp(buf, "====", 4) == 0)
				return (0);
			base64_decode(buf);
			goto next;
		}
#define	DEC(c)	(((c) - ' ') & 077)		/* single character decode */
#define IS_DEC(c) ( (((c) - ' ') >= 0) &&  (((c) - ' ') <= 077 + 1) )
/* #define IS_DEC(c) (1) */

#define OUT_OF_RANGE \
{	\
    warnx( \
"\n\tinput file: %s\n\tencoded file: %s\n\tcharacter out of range: [%d-%d]", \
 	filename, buffn, 1 + ' ', 077 + ' ' + 1); \
        return(1); \
}
#define PUTCHAR(c) \
if (!ignore) \
	putchar(c)


		/*
		 * `n' is used to avoid writing out all the characters
		 * at the end of the file.
		 */
		if ((n = DEC(*p)) <= 0)
			break;
		for (++p; n > 0; p += 4, n -= 3)
			if (n >= 3) {
				if (!(IS_DEC(*p) && IS_DEC(*(p + 1)) &&
				     IS_DEC(*(p + 2)) && IS_DEC(*(p + 3))))
                                	OUT_OF_RANGE

				ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
				PUTCHAR(ch);
				ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
				PUTCHAR(ch);
				ch = DEC(p[2]) << 6 | DEC(p[3]);
				PUTCHAR(ch);
			}
			else {
				if (n >= 1) {
					if (!(IS_DEC(*p) && IS_DEC(*(p + 1))))
	                                	OUT_OF_RANGE
					ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
					PUTCHAR(ch);
				}
				if (n >= 2) {
					if (!(IS_DEC(*(p + 1)) &&
						IS_DEC(*(p + 2))))
		                                OUT_OF_RANGE

					ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
					PUTCHAR(ch);
				}
				if (n >= 3) {
					if (!(IS_DEC(*(p + 2)) &&
						IS_DEC(*(p + 3))))
		                                OUT_OF_RANGE
					ch = DEC(p[2]) << 6 | DEC(p[3]);
					PUTCHAR(ch);
				}
			}
	}
	if (fgets(buf, sizeof(buf), stdin) == NULL ||
	    (strcmp(buf, "end") && strcmp(buf, "end\n") &&
	     strcmp(buf, "end\r\n"))) {
		warnx("%s: no \"end\" line", filename);
		return(1);
	}
	return(0);
}

void
base64_decode(const char *stream)
{
	unsigned char out[MAXPATHLEN * 4];
	int rv;

	if (index(stream, '\r') != NULL)
		*index(stream, '\r') = '\0';
	if (index(stream, '\n') != NULL)
		*index(stream, '\n') = '\0';
	rv = b64_pton(stream, out, (sizeof(out) / sizeof(out[0])));
	if (rv == -1)
		errx(1, "b64_pton: error decoding base64 input stream");
	fwrite(out, 1, rv, stdout);
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: uudecode [-cips] [file ...]\n"
"       uudecode [-i] -o output_file [file]\n"
"       b64decode [-cips] [file ...]\n"
"       b64decode [-i] -o output_file [file]\n");
	exit(1);
}
