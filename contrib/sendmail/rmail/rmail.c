/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.\n\
	All rights reserved.\n\
     Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* ! lint */

#ifndef lint
static char id[] = "@(#)$Id: rmail.c,v 8.39.4.12 2001/05/07 22:06:39 gshapiro Exp $";
#endif /* ! lint */

/* $FreeBSD$ */

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <fcntl.h>
#ifdef BSD4_4
# define FORK vfork
# include <paths.h>
#else /* BSD4_4 */
# define FORK fork
# ifndef _PATH_SENDMAIL
#  define _PATH_SENDMAIL "/usr/lib/sendmail"
# endif /* ! _PATH_SENDMAIL */
#endif /* BSD4_4 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef EX_OK
# undef EX_OK		/* unistd.h may have another use for this */
#endif /* EX_OK */
#include <sysexits.h>

#ifndef MAX
# define MAX(a, b)	((a) < (b) ? (b) : (a))
#endif /* ! MAX */

#ifndef __P
# ifdef __STDC__
#  define __P(protos)	protos
# else /* __STDC__ */
#  define __P(protos)	()
#  define const
# endif /* __STDC__ */
#endif /* ! __P */

#ifndef STDIN_FILENO
# define STDIN_FILENO	0
#endif /* ! STDIN_FILENO */

#if defined(BSD4_4) || defined(linux) || SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206) || _AIX4 >= 40300 || defined(HPUX11)
# define HASSNPRINTF	1
#endif /* defined(BSD4_4) || defined(linux) || SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206) || _AIX4 >= 40300 || defined(HPUX11) */

#if defined(sun) && !defined(BSD) && !defined(SOLARIS) && !defined(__svr4__) && !defined(__SVR4)
# define memmove(d, s, l)	(bcopy((s), (d), (l)))
#endif /* defined(sun) && !defined(BSD) && !defined(SOLARIS) && !defined(__svr4__) && !defined(__SVR4) */

#if !HASSNPRINTF && !SFIO
extern int	snprintf __P((char *, size_t, const char *, ...));
#endif /* !HASSNPRINTF && !SFIO */

#if defined(BSD4_4) || defined(__osf__) || defined(__GNU_LIBRARY__) || defined(IRIX64) || defined(IRIX5) || defined(IRIX6)
# ifndef HASSTRERROR
#  define HASSTRERROR	1
# endif /* ! HASSTRERROR */
#endif /* defined(BSD4_4) || defined(__osf__) || defined(__GNU_LIBRARY__) ||
	  defined(IRIX64) || defined(IRIX5) || defined(IRIX6) */

#if defined(SUNOS403) || defined(NeXT) || (defined(MACH) && defined(i386) && !defined(__GNU__)) || defined(oldBSD43) || defined(MORE_BSD) || defined(umipsbsd) || defined(ALTOS_SYSTEM_V) || defined(RISCOS) || defined(_AUX_SOURCE) || defined(UMAXV) || defined(titan) || defined(UNIXWARE) || defined(sony_news) || defined(luna) || defined(nec_ews_svr4) || defined(_nec_ews_svr4) || defined(__MAXION__)
# undef WIFEXITED
# undef WEXITSTATUS
# define WIFEXITED(st)		(((st) & 0377) == 0)
# define WEXITSTATUS(st)	(((st) >> 8) & 0377)
#endif /* defined(SUNOS403) || defined(NeXT) || (defined(MACH) && defined(i386) && !defined(__GNU__)) || defined(oldBSD43) || defined(MORE_BSD) || defined(umipsbsd) || defined(ALTOS_SYSTEM_V) || defined(RISCOS) || defined(_AUX_SOURCE) || defined(UMAXV) || defined(titan) || defined(UNIXWARE) || defined(sony_news) || defined(luna) || defined(nec_ews_svr4) || defined(_nec_ews_svr4) || defined(__MAXION__) */


#include "sendmail/errstring.h"

static void err __P((int, const char *, ...));
static void usage __P((void));
static char *xalloc __P((int));

#define newstr(s)	strcpy(xalloc(strlen(s) + 1), s)

static char *
xalloc(sz)
	register int sz;
{
	register char *p;

	/* some systems can't handle size zero mallocs */
	if (sz <= 0)
		sz = 1;

	p = malloc((unsigned) sz);
	if (p == NULL)
		err(EX_TEMPFAIL, "out of memory");
	return (p);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, debug, i, pdes[2], pid, status;
	size_t fplen = 0, fptlen = 0, len;
	off_t offset;
	FILE *fp;
	char *addrp = NULL, *domain, *p, *t;
	char *from_path, *from_sys, *from_user;
	char **args, buf[2048], lbuf[2048];
	struct stat sb;
	extern char *optarg;
	extern int optind;

	debug = 0;
	domain = "UUCP";		/* Default "domain". */
	while ((ch = getopt(argc, argv, "D:T")) != -1)
	{
		switch (ch)
		{
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
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	from_path = from_sys = from_user = NULL;
	for (offset = 0; ; )
	{
		/* Get and nul-terminate the line. */
		if (fgets(lbuf, sizeof(lbuf), stdin) == NULL)
			exit(EX_DATAERR);
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
		else
		{
			*p = '\n';
			break;
		}

		if (addrp == NULL || *addrp == '\0')
			err(EX_DATAERR, "corrupted From line: %s", lbuf);

		/* Use the "remote from" if it exists. */
		for (p = addrp; (p = strchr(p + 1, 'r')) != NULL; )
		{
			if (!strncmp(p, "remote from ", 12))
			{
				for (t = p += 12; *t != '\0'; ++t)
				{
					if (isascii(*t) && isspace(*t))
						break;
				}
				*t = '\0';
				if (debug)
					fprintf(stderr, "remote from: %s\n", p);
				break;
			}
		}

		/* Else use the string up to the last bang. */
		if (p == NULL)
		{
			if (*addrp == '!')
				err(EX_DATAERR, "bang starts address: %s",
				    addrp);
			else if ((t = strrchr(addrp, '!')) != NULL)
			{
				*t = '\0';
				p = addrp;
				addrp = t + 1;
				if (*addrp == '\0')
					err(EX_DATAERR,
					    "corrupted From line: %s", lbuf);
				if (debug)
					fprintf(stderr, "bang: %s\n", p);
			}
		}

		/* 'p' now points to any system string from this line. */
		if (p != NULL)
		{
			/* Nul terminate it as necessary. */
			for (t = p; *t != '\0'; ++t)
			{
				if (isascii(*t) && isspace(*t))
					break;
			}
			*t = '\0';

			/* If the first system, copy to the from_sys string. */
			if (from_sys == NULL)
			{
				from_sys = newstr(p);
				if (debug)
					fprintf(stderr, "from_sys: %s\n",
						from_sys);
			}

			/* Concatenate to the path string. */
			len = t - p;
			if (from_path == NULL)
			{
				fplen = 0;
				if ((from_path = malloc(fptlen = 256)) == NULL)
					err(EX_TEMPFAIL, NULL);
			}
			if (fplen + len + 2 > fptlen)
			{
				fptlen += MAX(fplen + len + 2, 256);
				if ((from_path = realloc(from_path,
							 fptlen)) == NULL)
					err(EX_TEMPFAIL, NULL);
			}
			memmove(from_path + fplen, p, len);
			fplen += len;
			from_path[fplen++] = '!';
			from_path[fplen] = '\0';
		}

		/* Save off from user's address; the last one wins. */
		for (p = addrp; *p != '\0'; ++p)
		{
			if (isascii(*p) && isspace(*p))
				break;
		}
		*p = '\0';
		if (*addrp == '\0')
			addrp = "<>";
		if (from_user != NULL)
			free(from_user);
		from_user = newstr(addrp);

		if (debug)
		{
			if (from_path != NULL)
				fprintf(stderr, "from_path: %s\n", from_path);
			fprintf(stderr, "from_user: %s\n", from_user);
		}

		if (offset != -1)
			offset = (off_t)ftell(stdin);
	}


	/* Allocate args (with room for sendmail args as well as recipients) */
	args = (char **)xalloc(sizeof(*args) * (10 + argc));

	i = 0;
	args[i++] = _PATH_SENDMAIL;	/* Build sendmail's argument list. */
	args[i++] = "-oee";		/* No errors, just status. */
#ifdef QUEUE_ONLY
	args[i++] = "-odq";		/* Queue it, don't try to deliver. */
#else
	args[i++] = "-odi";		/* Deliver in foreground. */
#endif
	args[i++] = "-oi";		/* Ignore '.' on a line by itself. */

	/* set from system and protocol used */
	if (from_sys == NULL)
		snprintf(buf, sizeof(buf), "-p%s", domain);
	else if (strchr(from_sys, '.') == NULL)
		snprintf(buf, sizeof(buf), "-p%s:%s.%s",
			domain, from_sys, domain);
	else
		snprintf(buf, sizeof(buf), "-p%s:%s", domain, from_sys);
	args[i++] = newstr(buf);

	/* Set name of ``from'' person. */
	snprintf(buf, sizeof(buf), "-f%s%s",
		 from_path ? from_path : "", from_user);
	args[i++] = newstr(buf);

	/*
	**  Don't copy arguments beginning with - as they will be
	**  passed to sendmail and could be interpreted as flags.
	**  To prevent confusion of sendmail wrap < and > around
	**  the address (helps to pass addrs like @gw1,@gw2:aa@bb)
	*/

	while (*argv != NULL)
	{
		if (**argv == '-')
			err(EX_USAGE, "dash precedes argument: %s", *argv);

		if (strchr(*argv, ',') == NULL || strchr(*argv, '<') != NULL)
			args[i++] = *argv;
		else
		{
			len = strlen(*argv) + 3;
			if ((args[i] = malloc(len)) == NULL)
				err(EX_TEMPFAIL, "Cannot malloc");
			snprintf(args[i++], len, "<%s>", *argv);
		}
		argv++;
		argc--;

		/* Paranoia check, argc used for args[] bound */
		if (argc < 0)
			err(EX_SOFTWARE, "Argument count mismatch");
	}
	args[i] = NULL;

	if (debug)
	{
		fprintf(stderr, "Sendmail arguments:\n");
		for (i = 0; args[i] != NULL; i++)
			fprintf(stderr, "\t%s\n", args[i]);
	}

	/*
	**  If called with a regular file as standard input, seek to the right
	**  position in the file and just exec sendmail.  Could probably skip
	**  skip the stat, but it's not unreasonable to believe that a failed
	**  seek will cause future reads to fail.
	*/

	if (!fstat(STDIN_FILENO, &sb) && S_ISREG(sb.st_mode))
	{
		if (lseek(STDIN_FILENO, offset, SEEK_SET) != offset)
			err(EX_TEMPFAIL, "stdin seek");
		(void) execv(_PATH_SENDMAIL, args);
		err(EX_OSERR, "%s", _PATH_SENDMAIL);
	}

	if (pipe(pdes) < 0)
		err(EX_OSERR, NULL);

	switch (pid = FORK())
	{
	  case -1:				/* Err. */
		err(EX_OSERR, NULL);
		/* NOTREACHED */

	  case 0:				/* Child. */
		if (pdes[0] != STDIN_FILENO)
		{
			(void) dup2(pdes[0], STDIN_FILENO);
			(void) close(pdes[0]);
		}
		(void) close(pdes[1]);
		(void) execv(_PATH_SENDMAIL, args);
		_exit(127);
		/* NOTREACHED */
	}

	if ((fp = fdopen(pdes[1], "w")) == NULL)
		err(EX_OSERR, NULL);
	(void) close(pdes[0]);

	/* Copy the file down the pipe. */
	do
	{
		(void) fprintf(fp, "%s", lbuf);
	} while (fgets(lbuf, sizeof(lbuf), stdin) != NULL);

	if (ferror(stdin))
		err(EX_TEMPFAIL, "stdin: %s", errstring(errno));

	if (fclose(fp))
		err(EX_OSERR, NULL);

	if ((waitpid(pid, &status, 0)) == -1)
		err(EX_OSERR, "%s", _PATH_SENDMAIL);

	if (!WIFEXITED(status))
		err(EX_OSERR, "%s: did not terminate normally", _PATH_SENDMAIL);

	if (WEXITSTATUS(status))
		err(status, "%s: terminated with %d (non-zero) status",
		    _PATH_SENDMAIL, WEXITSTATUS(status));
	exit(EX_OK);
	/* NOTREACHED */
	return EX_OK;
}

static void
usage()
{
	(void) fprintf(stderr, "usage: rmail [-T] [-D domain] user ...\n");
	exit(EX_USAGE);
}

#ifdef __STDC__
# include <stdarg.h>
#else /* __STDC__ */
# include <varargs.h>
#endif /* __STDC__ */

static void
#ifdef __STDC__
err(int eval, const char *fmt, ...)
#else /* __STDC__ */
err(eval, fmt, va_alist)
	int eval;
	const char *fmt;
	va_dcl
#endif /* __STDC__ */
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else /* __STDC__ */
	va_start(ap);
#endif /* __STDC__ */
	(void) fprintf(stderr, "rmail: ");
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fprintf(stderr, "\n");
	exit(eval);
}
