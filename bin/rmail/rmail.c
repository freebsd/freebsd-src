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
 *
 *	$Id: rmail.c,v 1.11 1997/06/06 06:46:27 charnier Exp $
 */

#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char const sccsid[] = "@(#)rmail.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * RMAIL -- UUCP mail server.
 *
 * This program reads the >From ... remote from ... lines that UUCP is so
 * fond of and turns them into something reasonable.  It then execs sendmail
 * with various options built from these lines.
 *
 * The expected syntax is:
 *
 *	 <user> := [-a-z0-9]+
 *	 <date> := ctime format
 *	 <site> := [-a-z0-9!]+
 * <blank line> := "^\n$"
 *	 <from> := "From" <space> <user> <space> <date>
 *		  [<space> "remote from" <space> <site>]
 *    <forward> := ">" <from>
 *	    msg := <from> <forward>* <blank-line> <body>
 *
 * The output of rmail(8) compresses the <forward> lines into a single
 * from path.
 *
 * The err(3) routine is included here deliberately to make this code
 * a bit more portable.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int errno, optind;
	FILE *fp;
	struct stat sb;
	size_t fplen, fptlen, len;
	off_t offset;
	int ch, debug, i, pdes[2], pid, status;
	char *addrp, *domain, *p, *t;
	char *from_path, *from_sys, *from_user;
	char *args[100], buf[2048], lbuf[2048];

	debug = 0;
	domain = "UUCP";		/* Default "domain". */
	while ((ch = getopt(argc, argv, "D:T")) != -1)
		switch (ch) {
		case 'T':
			debug = 1;
			break;
		case 'D':
			domain = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	fplen = fptlen = 0;
	addrp = "";
	from_path = from_sys = from_user = NULL;
	for (offset = 0;;) {

		/* Get and nul-terminate the line. */
		if (fgets(lbuf, sizeof(lbuf), stdin) == NULL)
			exit (EX_DATAERR);
		if ((p = strchr(lbuf, '\n')) == NULL)
			err(EX_DATAERR, "line too long");
		*p = '\0';

		/* Parse lines until reach a non-"From" line. */
		if (!strncmp(lbuf, "From ", 5))
			addrp = lbuf + 5;
		else if (!strncmp(lbuf, ">From ", 6))
			addrp = lbuf + 6;
		else if (offset == 0)
			err(EX_DATAERR,
			    "missing or empty From line: %s", lbuf);
		else {
			*p = '\n';
			break;
		}

		if (*addrp == '\0')
			err(EX_DATAERR, "corrupted From line: %s", lbuf);

		/* Use the "remote from" if it exists. */
		for (p = addrp; (p = strchr(p + 1, 'r')) != NULL;)
			if (!strncmp(p, "remote from ", 12)) {
				for (t = p += 12; *t && !isspace(*t); ++t);
				*t = '\0';
				if (debug)
					(void)fprintf(stderr,
					    "remote from: %s\n", p);
				break;
			}

		/* Else use the string up to the last bang. */
		if (p == NULL)
			if (*addrp == '!')
				err(EX_DATAERR,
				    "bang starts address: %s", addrp);
			else if ((t = strrchr(addrp, '!')) != NULL) {
				*t = '\0';
				p = addrp;
				addrp = t + 1;
				if (*addrp == '\0')
					err(EX_DATAERR,
					    "corrupted From line: %s", lbuf);
				if (debug)
					(void)fprintf(stderr, "bang: %s\n", p);
			}

		/* 'p' now points to any system string from this line. */
		if (p != NULL) {
			/* Nul terminate it as necessary. */
			for (t = p; *t && !isspace(*t); ++t);
			*t = '\0';

			/* If the first system, copy to the from_sys string. */
			if (from_sys == NULL) {
				if ((from_sys = strdup(p)) == NULL)
					err(EX_TEMPFAIL, NULL);
				if (debug)
					(void)fprintf(stderr,
					    "from_sys: %s\n", from_sys);
			}

			/* Concatenate to the path string. */
			len = t - p;
			if (from_path == NULL) {
				fplen = 0;
				if ((from_path = malloc(fptlen = 256)) == NULL)
					err(EX_TEMPFAIL, NULL);
			}
			if (fplen + len + 2 > fptlen) {
				fptlen += MAX(fplen + len + 2, 256);
				if ((from_path =
				    realloc(from_path, fptlen)) == NULL)
					err(EX_TEMPFAIL, NULL);
			}
			memmove(from_path + fplen, p, len);
			fplen += len;
			from_path[fplen++] = '!';
			from_path[fplen] = '\0';
		}

		/* Save off from user's address; the last one wins. */
		for (p = addrp; *p && !isspace(*p); ++p);
		*p = '\0';
		if (*addrp == '\0')
			addrp = "<>";
		if (from_user != NULL)
			free(from_user);
		if ((from_user = strdup(addrp)) == NULL)
			err(EX_TEMPFAIL, NULL);

		if (debug) {
			if (from_path != NULL)
				(void)fprintf(stderr,
				    "from_path: %s\n", from_path);
			(void)fprintf(stderr, "from_user: %s\n", from_user);
		}

		if (offset != -1)
			offset = (off_t)ftell(stdin);
	}

	i = 0;
	args[i++] = _PATH_SENDMAIL;	/* Build sendmail's argument list. */
	args[i++] = "-oee";		/* No errors, just status. */
#ifdef QUEUE_ONLY
	args[i++] = "-odq";		/* Queue it, don't try to deliver. */
#else
	args[i++] = "-odi";		/* Deliver in foreground. */
#endif
	args[i++] = "-oi";		/* Ignore '.' on a line by itself. */

	if (from_sys != NULL) {		/* Set sender's host name. */
		if (strchr(from_sys, '.') == NULL)
			(void)snprintf(buf, sizeof(buf),
			    "-oMs%s.%s", from_sys, domain);
		else
			(void)snprintf(buf, sizeof(buf), "-oMs%s", from_sys);
		if ((args[i++] = strdup(buf)) == NULL)
			 err(EX_TEMPFAIL, NULL);
	}
					/* Set protocol used. */
	(void)snprintf(buf, sizeof(buf), "-oMr%s", domain);
	if ((args[i++] = strdup(buf)) == NULL)
		err(EX_TEMPFAIL, NULL);

					/* Set name of ``from'' person. */
	(void)snprintf(buf, sizeof(buf), "-f%s%s",
	    from_path ? from_path : "", from_user);
	if ((args[i++] = strdup(buf)) == NULL)
		err(EX_TEMPFAIL, NULL);

	/*
	 * Don't copy arguments beginning with - as they will be
	 * passed to sendmail and could be interpreted as flags.
	 * To prevent confusion of sendmail wrap < and > around
	 * the address (helps to pass addrs like @gw1,@gw2:aa@bb)
	 */
	while (*argv) {
		if (**argv == '-')
			err(EX_USAGE, "dash precedes argument: %s", *argv);
		if (strchr(*argv, ',') == NULL || strchr(*argv, '<') != NULL)
			args[i++] = *argv;
		else {
			if ((args[i] = malloc(strlen(*argv) + 3)) == NULL)
				err(EX_TEMPFAIL, "Cannot malloc");
			sprintf (args [i++], "<%s>", *argv);
		}
		argv++;
	} 
	args[i] = 0;

	if (debug) {
		(void)fprintf(stderr, "Sendmail arguments:\n");
		for (i = 0; args[i]; i++)
			(void)fprintf(stderr, "\t%s\n", args[i]);
	}

	/*
	 * If called with a regular file as standard input, seek to the right
	 * position in the file and just exec sendmail.  Could probably skip
	 * skip the stat, but it's not unreasonable to believe that a failed
	 * seek will cause future reads to fail.
	 */
	if (!fstat(STDIN_FILENO, &sb) && S_ISREG(sb.st_mode)) {
		if (lseek(STDIN_FILENO, offset, SEEK_SET) != offset)
			err(EX_TEMPFAIL, "stdin seek");
		execv(_PATH_SENDMAIL, args);
		err(EX_OSERR, "%s", _PATH_SENDMAIL);
	}

	if (pipe(pdes) < 0)
		err(EX_OSERR, NULL);

	switch (pid = vfork()) {
	case -1:				/* Err. */
		err(EX_OSERR, NULL);
	case 0:					/* Child. */
		if (pdes[0] != STDIN_FILENO) {
			(void)dup2(pdes[0], STDIN_FILENO);
			(void)close(pdes[0]);
		}
		(void)close(pdes[1]);
		execv(_PATH_SENDMAIL, args);
		_exit(127);
		/* NOTREACHED */
	}

	if ((fp = fdopen(pdes[1], "w")) == NULL)
		err(EX_OSERR, NULL);
	(void)close(pdes[0]);

	/* Copy the file down the pipe. */
	do {
		(void)fprintf(fp, "%s", lbuf);
	} while (fgets(lbuf, sizeof(lbuf), stdin) != NULL);

	if (ferror(stdin))
		err(EX_TEMPFAIL, "stdin: %s", strerror(errno));

	if (fclose(fp))
		err(EX_OSERR, NULL);

	if ((waitpid(pid, &status, 0)) == -1)
		err(EX_OSERR, "%s", _PATH_SENDMAIL);

	if (!WIFEXITED(status))
		err(EX_OSERR,
		    "%s: did not terminate normally", _PATH_SENDMAIL);

	if (WEXITSTATUS(status))
		err(status, "%s: terminated with %d (non-zero) status",
		    _PATH_SENDMAIL, WEXITSTATUS(status));
	exit(EX_OK);
}

void
usage()
{
	(void)fprintf(stderr, "usage: rmail [-T] [-D domain] user ...\n");
	exit(EX_USAGE);
}

