/*-
 * Copyright (c) 1988, 1993, 1994
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
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "From: @(#)chpass.c	8.4 (Berkeley) 4/2/94";
static char rcsid[] =
	"$Id: chpass.c,v 1.5 1995/09/02 03:56:17 wpaul Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pw_scan.h>
#include <pw_util.h>
#include "pw_copy.h"
#ifdef YP
#include "pw_yp.h"
#endif

#include "chpass.h"
#include "pathnames.h"

char *progname = "chpass";
char *tempname;
uid_t uid;

void	baduser __P((void));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	enum { NEWSH, LOADENTRY, EDITENTRY, NEWPW } op;
	struct passwd *pw, lpw;
	int ch, pfd, tfd;
	char *arg;
#ifdef YP
	int force_local = 0;
	int force_yp = 0;
#endif

	op = EDITENTRY;
#ifdef YP
	while ((ch = getopt(argc, argv, "a:p:s:ly")) != EOF)
#else
	while ((ch = getopt(argc, argv, "a:p:s:")) != EOF)
#endif
		switch(ch) {
		case 'a':
			op = LOADENTRY;
			arg = optarg;
			break;
		case 's':
			op = NEWSH;
			arg = optarg;
			break;
		case 'p':
			op = NEWPW;
			arg = optarg;
			break;
#ifdef YP
		case 'l':
			force_local = 1;
			break;
		case 'y':
			force_yp = 1;
			break;
#endif
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	uid = getuid();

	if (op == EDITENTRY || op == NEWSH || op == NEWPW)
		switch(argc) {
		case 0:
			if (!(pw = getpwuid(uid)))
				errx(1, "unknown user: uid %u", uid);
			break;
		case 1:
			if (!(pw = getpwnam(*argv)))
				errx(1, "unknown user: %s", *argv);
			if (uid && uid != pw->pw_uid)
				baduser();
			break;
		default:
			usage();
		}
	if (op == NEWSH) {
		/* protect p_shell -- it thinks NULL is /bin/sh */
		if (!arg[0])
			usage();
		if (p_shell(arg, pw, (ENTRY *)NULL))
			pw_error((char *)NULL, 0, 1);
	}

	if (op == LOADENTRY) {
		if (uid)
			baduser();
		pw = &lpw;
		if (!pw_scan(arg, pw))
			exit(1);
	}

	if (op == NEWPW) {
		if (uid)
			baduser();

		if(strchr(arg, ':')) {
			errx(1, "invalid format for password");
		}
		pw->pw_passwd = arg;
	}

#ifdef YP
	pw->pw_name = strdup(pw->pw_name);
	_use_yp = use_yp(pw->pw_name);
	if (_use_yp == USER_YP_ONLY) {
		if (!force_local) {
			_use_yp = 1;
			pw = (struct passwd *)&yp_password;
		} else
			errx(1, "unknown local user: %s.", pw->pw_name);
	} else if (_use_yp == USER_LOCAL_ONLY) {
		if (!force_yp) {
			_use_yp = 0;
			pw = (struct passwd *)&local_password;
		} else
			errx(1, "unknown NIS user: %s.", pw->pw_name);
	} else if (_use_yp == USER_YP_AND_LOCAL) {
		if (!force_local) {
			_use_yp = 1;
			pw = (struct passwd *)&yp_password;
		} else {
			_use_yp = 0;
			pw = (struct passwd *)&local_password;
		}
	}
#endif /* YP */

	/*
	 * The temporary file/file descriptor usage is a little tricky here.
	 * 1:	We start off with two fd's, one for the master password
	 *	file (used to lock everything), and one for a temporary file.
	 * 2:	Display() gets an fp for the temporary file, and copies the
	 *	user's information into it.  It then gives the temporary file
	 *	to the user and closes the fp, closing the underlying fd.
	 * 3:	The user edits the temporary file some number of times.
	 * 4:	Verify() gets an fp for the temporary file, and verifies the
	 *	contents.  It can't use an fp derived from the step #2 fd,
	 *	because the user's editor may have created a new instance of
	 *	the file.  Once the file is verified, its contents are stored
	 *	in a password structure.  The verify routine closes the fp,
	 *	closing the underlying fd.
	 * 5:	Delete the temporary file.
	 * 6:	Get a new temporary file/fd.  Pw_copy() gets an fp for it
	 *	file and copies the master password file into it, replacing
	 *	the user record with a new one.  We can't use the first
	 *	temporary file for this because it was owned by the user.
	 *	Pw_copy() closes its fp, flushing the data and closing the
	 *	underlying file descriptor.  We can't close the master
	 *	password fp, or we'd lose the lock.
	 * 7:	Call pw_mkdb() (which renames the temporary file) and exit.
	 *	The exit closes the master passwd fp/fd.
	 */
	pw_init();
	pfd = pw_lock();
	tfd = pw_tmp();

	if (op == EDITENTRY) {
		display(tfd, pw);
		edit(pw);
		(void)unlink(tempname);
		tfd = pw_tmp();
	}

#ifdef YP
	if (_use_yp) {
		yp_submit(pw);
		(void)unlink(tempname);
	} else {
#endif /* YP */
	pw_copy(pfd, tfd, pw);

	if (!pw_mkdb())
		pw_error((char *)NULL, 0, 1);
#ifdef YP
	}
#endif /* YP */
	exit(0);
}

void
baduser()
{
	errx(1, "%s", strerror(EACCES));
}

void
usage()
{

	(void)fprintf(stderr,
#ifdef YP
		"usage: chpass [-l] [-y] [-a list] [-p encpass] [-s shell] [user]\n");
#else
		"usage: chpass [-a list] [-p encpass] [-s shell] [user]\n");
#endif
	exit(1);
}
