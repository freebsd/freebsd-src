/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1993 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.\n\
	All rights reserved.\n\
     Copyright (c) 1993 Eric P. Allman.  All rights reserved.\n\
     Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* ! lint */

#ifndef lint
static char id[] = "@(#)$Id: smrsh.c,v 8.31.4.5 2000/09/17 17:04:27 gshapiro Exp $";
#endif /* ! lint */

/* $FreeBSD$ */

/*
**  SMRSH -- sendmail restricted shell
**
**	This is a patch to get around the prog mailer bugs in most
**	versions of sendmail.
**
**	Use this in place of /bin/sh in the "prog" mailer definition
**	in your sendmail.cf file.  You then create CMDDIR (owned by
**	root, mode 755) and put links to any programs you want
**	available to prog mailers in that directory.  This should
**	include things like "vacation" and "procmail", but not "sed"
**	or "sh".
**
**	Leading pathnames are stripped from program names so that
**	existing .forward files that reference things like
**	"/usr/bin/vacation" will continue to work.
**
**	The following characters are completely illegal:
**		<  >  ^  &  `  (  ) \n \r
**	The following characters are sometimes illegal:
**		|  &
**	This is more restrictive than strictly necessary.
**
**	To use this, add FEATURE(`smrsh') to your .mc file.
**
**	This can be used on any version of sendmail.
**
**	In loving memory of RTM.  11/02/93.
*/

#include <unistd.h>
#include <stdio.h>
#include <sys/file.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef EX_OK
# undef EX_OK
#endif /* EX_OK */
#include <sysexits.h>
#include <syslog.h>
#include <stdlib.h>

#ifndef TRUE
# define TRUE	1
# define FALSE	0
#endif /* ! TRUE */

/* directory in which all commands must reside */
#ifndef CMDDIR
# define CMDDIR		"/usr/libexec/sm.bin"
#endif /* ! CMDDIR */

/* characters disallowed in the shell "-c" argument */
#define SPECIALS	"<|>^();&`$\r\n"

/* default search path */
#ifndef PATH
# define PATH		"/bin:/usr/bin"
#endif /* ! PATH */

#ifndef __P
# include "sendmail/cdefs.h"
#endif /* ! __P */

extern size_t	strlcpy __P((char *, const char *, size_t));
extern size_t	strlcat __P((char *, const char *, size_t));

char newcmdbuf[1000];
char *prg, *par;

/*
**  ADDCMD -- add a string to newcmdbuf, check for overflow
**
**    Parameters:
**	s -- string to add
**	cmd -- it's a command: prepend CMDDIR/
**	len -- length of string to add
**
**    Side Effects:
**	changes newcmdbuf or exits with a failure.
**
*/

void
addcmd(s, cmd, len)
	char *s;
	int cmd;
	int len;
{
	if (s == NULL || *s == '\0')
		return;

	if (sizeof newcmdbuf - strlen(newcmdbuf) <=
	    len + (cmd ? (strlen(CMDDIR) + 1) : 0))
	{
		fprintf(stderr, "%s: command too long: %s\n", prg, par);
#ifndef DEBUG
		syslog(LOG_WARNING, "command too long: %.40s", par);
#endif /* ! DEBUG */
		exit(EX_UNAVAILABLE);
	}
	if (cmd)
	{
		(void) strlcat(newcmdbuf, CMDDIR, sizeof newcmdbuf);
		(void) strlcat(newcmdbuf, "/", sizeof newcmdbuf);
	}
	(void) strlcat(newcmdbuf, s, sizeof newcmdbuf);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	register char *p;
	register char *q;
	register char *r;
	register char *cmd;
	int i;
	int isexec;
	int save_errno;
	char *newenv[2];
	char cmdbuf[1000];
	char pathbuf[1000];
	char specialbuf[32];

#ifndef DEBUG
# ifndef LOG_MAIL
	openlog("smrsh", 0);
# else /* ! LOG_MAIL */
	openlog("smrsh", LOG_ODELAY|LOG_CONS, LOG_MAIL);
# endif /* ! LOG_MAIL */
#endif /* ! DEBUG */

	(void) strlcpy(pathbuf, "PATH=", sizeof pathbuf);
	(void) strlcat(pathbuf, PATH, sizeof pathbuf);
	newenv[0] = pathbuf;
	newenv[1] = NULL;

	/*
	**  Do basic argv usage checking
	*/

	prg = argv[0];
	par = argv[2];

	if (argc != 3 || strcmp(argv[1], "-c") != 0)
	{
		fprintf(stderr, "Usage: %s -c command\n", prg);
#ifndef DEBUG
		syslog(LOG_ERR, "usage");
#endif /* ! DEBUG */
		exit(EX_USAGE);
	}

	/*
	**  Disallow special shell syntax.  This is overly restrictive,
	**  but it should shut down all attacks.
	**  Be sure to include 8-bit versions, since many shells strip
	**  the address to 7 bits before checking.
	*/

	if (strlen(SPECIALS) * 2 >= sizeof specialbuf)
	{
#ifndef DEBUG
		syslog(LOG_ERR, "too many specials: %.40s", SPECIALS);
#endif /* ! DEBUG */
		exit(EX_UNAVAILABLE);
	}
	(void) strlcpy(specialbuf, SPECIALS, sizeof specialbuf);
	for (p = specialbuf; *p != '\0'; p++)
		*p |= '\200';
	(void) strlcat(specialbuf, SPECIALS, sizeof specialbuf);

	/*
	**  Do a quick sanity check on command line length.
	*/

	i = strlen(par);
	if (i > (sizeof newcmdbuf - sizeof CMDDIR - 2))
	{
		fprintf(stderr, "%s: command too long: %s\n", prg, par);
#ifndef DEBUG
		syslog(LOG_WARNING, "command too long: %.40s", par);
#endif /* ! DEBUG */
		exit(EX_UNAVAILABLE);
	}

	q = par;
	newcmdbuf[0] = '\0';
	isexec = FALSE;

	while (*q)
	{
		/*
		**  Strip off a leading pathname on the command name.  For
		**  example, change /usr/ucb/vacation to vacation.
		*/

		/* strip leading spaces */
		while (*q != '\0' && isascii(*q) && isspace(*q))
			q++;
		if (*q == '\0')
		{
			if (isexec)
			{
				fprintf(stderr, "%s: missing command to exec\n",
					prg);
#ifndef DEBUG
				syslog(LOG_CRIT, "uid %d: missing command to exec", getuid());
#endif /* ! DEBUG */
				exit(EX_UNAVAILABLE);
			}
			break;
		}

		/* find the end of the command name */
		p = strpbrk(q, " \t");
		if (p == NULL)
			cmd = &q[strlen(q)];
		else
		{
			*p = '\0';
			cmd = p;
		}
		/* search backwards for last / (allow for 0200 bit) */
		while (cmd > q)
		{
			if ((*--cmd & 0177) == '/')
			{
				cmd++;
				break;
			}
		}
		/* cmd now points at final component of path name */

		/* allow a few shell builtins */
		if (strcmp(q, "exec") == 0 && p != NULL)
		{
			addcmd("exec ", FALSE, strlen("exec "));
			/* test _next_ arg */
			q = ++p;
			isexec = TRUE;
			continue;
		}
		else if (strcmp(q, "exit") == 0 || strcmp(q, "echo") == 0)
		{
			addcmd(cmd, FALSE, strlen(cmd));
			/* test following chars */
		}
		else
		{
			/*
			**  Check to see if the command name is legal.
			*/
			(void) strlcpy(cmdbuf, CMDDIR, sizeof cmdbuf);
			(void) strlcat(cmdbuf, "/", sizeof cmdbuf);
			(void) strlcat(cmdbuf, cmd, sizeof cmdbuf);
#ifdef DEBUG
			printf("Trying %s\n", cmdbuf);
#endif /* DEBUG */
			if (access(cmdbuf, X_OK) < 0)
			{
				/* oops....  crack attack possiblity */
				fprintf(stderr,
					"%s: %s not available for sendmail programs\n",
					prg, cmd);
				if (p != NULL)
					*p = ' ';
#ifndef DEBUG
				syslog(LOG_CRIT, "uid %d: attempt to use %s",
				       getuid(), cmd);
#endif /* ! DEBUG */
				exit(EX_UNAVAILABLE);
			}

			/*
			**  Create the actual shell input.
			*/

			addcmd(cmd, TRUE, strlen(cmd));
		}
		isexec = FALSE;

		if (p != NULL)
			*p = ' ';
		else
			break;

		r = strpbrk(p, specialbuf);
		if (r == NULL) {
			addcmd(p, FALSE, strlen(p));
			break;
		}
#if ALLOWSEMI
		if (*r == ';') {
			addcmd(p, FALSE,  r - p + 1);
			q = r + 1;
			continue;
		}
#endif /* ALLOWSEMI */
		if ((*r == '&' && *(r + 1) == '&') ||
		    (*r == '|' && *(r + 1) == '|'))
		{
			addcmd(p, FALSE,  r - p + 2);
			q = r + 2;
			continue;
		}

		fprintf(stderr, "%s: cannot use %c in command\n", prg, *r);
#ifndef DEBUG
		syslog(LOG_CRIT, "uid %d: attempt to use %c in command: %s",
			getuid(), *r, par);
#endif /* ! DEBUG */
		exit(EX_UNAVAILABLE);
	}		/* end of while *q */
	if (isexec)
	{
		fprintf(stderr, "%s: missing command to exec\n", prg);
#ifndef DEBUG
		syslog(LOG_CRIT, "uid %d: missing command to exec", getuid());
#endif /* ! DEBUG */
		exit(EX_UNAVAILABLE);
	}
	/* make sure we created something */
	if (newcmdbuf[0] == '\0')
	{
		fprintf(stderr, "Usage: %s -c command\n", prg);
#ifndef DEBUG
		syslog(LOG_ERR, "usage");
#endif /* ! DEBUG */
		exit(EX_USAGE);
	}

	/*
	**  Now invoke the shell
	*/

#ifdef DEBUG
	printf("%s\n", newcmdbuf);
#endif /* DEBUG */
	(void) execle("/bin/sh", "/bin/sh", "-c", newcmdbuf, NULL, newenv);
	save_errno = errno;
#ifndef DEBUG
	syslog(LOG_CRIT, "Cannot exec /bin/sh: %m");
#endif /* ! DEBUG */
	errno = save_errno;
	perror("/bin/sh");
	exit(EX_OSFILE);
	/* NOTREACHED */
	return EX_OSFILE;
}
