/*
 * Copyright (c) 1993 Eric P. Allman
 * Copyright (c) 1993
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
static char sccsid[] = "@(#)smrsh.c	8.5 (Berkeley) 10/19/97";
#endif /* not lint */

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
**	"/usr/ucb/vacation" will continue to work.
**
**	The following characters are completely illegal:
**		<  >  |  ^  ;  &  $  `  (  ) \n \r
**	This is more restrictive than strictly necessary.
**
**	To use this, edit /etc/sendmail.cf, search for ^Mprog, and
**	change P=/bin/sh to P=/usr/local/etc/smrsh, where this compiled
**	binary is installed /usr/local/etc/smrsh.
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
#ifdef EX_OK
# undef EX_OK
#endif
#include <sysexits.h>
#include <syslog.h>
#include <stdlib.h>

/* directory in which all commands must reside */
#ifndef CMDDIR
# define CMDDIR		"/usr/adm/sm.bin"
#endif

/* characters disallowed in the shell "-c" argument */
#define SPECIALS	"<|>^();&`$\r\n"

/* default search path */
#ifndef PATH
# define PATH		"/bin:/usr/bin:/usr/ucb"
#endif

main(argc, argv)
	int argc;
	char **argv;
{
	register char *p;
	register char *q;
	register char *cmd;
	int i;
	char *newenv[2];
	char cmdbuf[1000];
	char pathbuf[1000];

#ifndef LOG_MAIL
	openlog("smrsh", 0);
#else
	openlog("smrsh", LOG_ODELAY|LOG_CONS, LOG_MAIL);
#endif

	strcpy(pathbuf, "PATH=");
	strcat(pathbuf, PATH);
	newenv[0] = pathbuf;
	newenv[1] = NULL;

	/*
	**  Do basic argv usage checking
	*/

	if (argc != 3 || strcmp(argv[1], "-c") != 0)
	{
		fprintf(stderr, "Usage: %s -c command\n", argv[0]);
		syslog(LOG_ERR, "usage");
		exit(EX_USAGE);
	}

	/*
	**  Disallow special shell syntax.  This is overly restrictive,
	**  but it should shut down all attacks.
	**  Be sure to include 8-bit versions, since many shells strip
	**  the address to 7 bits before checking.
	*/

	strcpy(cmdbuf, SPECIALS);
	for (p = cmdbuf; *p != '\0'; p++)
		*p |= '\200';
	strcat(cmdbuf, SPECIALS);
	p = strpbrk(argv[2], cmdbuf);
	if (p != NULL)
	{
		fprintf(stderr, "%s: cannot use %c in command\n",
			argv[0], *p);
		syslog(LOG_CRIT, "uid %d: attempt to use %c in command: %s",
			getuid(), *p, argv[2]);
		exit(EX_UNAVAILABLE);
	}

	/*
	**  Do a quick sanity check on command line length.
	*/

	i = strlen(argv[2]);
	if (i > (sizeof cmdbuf - sizeof CMDDIR - 2))
	{
		fprintf(stderr, "%s: command too long: %s\n", argv[0], argv[2]);
		syslog(LOG_WARNING, "command too long: %.40s", argv[2]);
		exit(EX_UNAVAILABLE);
	}

	/*
	**  Strip off a leading pathname on the command name.  For
	**  example, change /usr/ucb/vacation to vacation.
	*/

	/* strip leading spaces */
	for (q = argv[2]; *q != '\0' && isascii(*q) && isspace(*q); )
		q++;

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

	/*
	**  Check to see if the command name is legal.
	*/

	(void) strcpy(cmdbuf, CMDDIR);
	(void) strcat(cmdbuf, "/");
	(void) strcat(cmdbuf, cmd);
#ifdef DEBUG
	printf("Trying %s\n", cmdbuf);
#endif
	if (access(cmdbuf, X_OK) < 0)
	{
		/* oops....  crack attack possiblity */
		fprintf(stderr, "%s: %s not available for sendmail programs\n",
			argv[0], cmd);
		if (p != NULL)
			*p = ' ';
		syslog(LOG_CRIT, "uid %d: attempt to use %s", getuid(), cmd);
		exit(EX_UNAVAILABLE);
	}
	if (p != NULL)
		*p = ' ';

	/*
	**  Create the actual shell input.
	*/

	strcpy(cmdbuf, CMDDIR);
	strcat(cmdbuf, "/");
	strcat(cmdbuf, cmd);

	/*
	**  Now invoke the shell
	*/

#ifdef DEBUG
	printf("%s\n", cmdbuf);
#endif
	execle("/bin/sh", "/bin/sh", "-c", cmdbuf, NULL, newenv);
	syslog(LOG_CRIT, "Cannot exec /bin/sh: %m");
	perror("/bin/sh");
	exit(EX_OSFILE);
}
