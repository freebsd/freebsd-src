/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "pwupd.h"

#define HAVE_PWDB_C	1

static int
pwdb(char *arg,...)
{
	int             i = 0;
	pid_t           pid;
	va_list         ap;
	char           *args[8];

	args[i++] = _PATH_PWD_MKDB;
	va_start(ap, arg);
	while (i < 6 && arg != NULL) {
		args[i++] = arg;
		arg = va_arg(ap, char *);
	}
	args[i++] = _PATH_MASTERPASSWD;
	args[i] = NULL;

	if ((pid = fork()) == -1)	/* Error (errno set) */
		i = -1;
	else if (pid == 0) {	/* Child */
		execv(args[0], args);
		_exit(1);
	} else {		/* Parent */
		waitpid(pid, &i, 0);
		if ((i = WEXITSTATUS(i)) != 0)
			errno = EIO;	/* set SOMETHING */
	}
	return i;
}

int
fmtpwentry(char *buf, struct passwd * pwd, int type)
{
	int             l;
	char           *pw;

	pw = (pwd->pw_passwd == NULL || !*pwd->pw_passwd) ? "" : (type == PWF_MASTER) ? pwd->pw_passwd : "*";

	if (type == PWF_PASSWD)
		l = sprintf(buf, "%s:*:%ld:%ld:%s:%s:%s\n",
		       pwd->pw_name, (long) pwd->pw_uid, (long) pwd->pw_gid,
			    pwd->pw_gecos ? pwd->pw_gecos : "User &",
			    pwd->pw_dir, pwd->pw_shell);
	else
		l = sprintf(buf, "%s:%s:%ld:%ld:%s:%lu:%lu:%s:%s:%s\n",
		   pwd->pw_name, pw, (long) pwd->pw_uid, (long) pwd->pw_gid,
			    pwd->pw_class ? pwd->pw_class : "",
			    (unsigned long) pwd->pw_change,
			    (unsigned long) pwd->pw_expire,
			    pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);
	return l;
}


int
fmtpwent(char *buf, struct passwd * pwd)
{
	return fmtpwentry(buf, pwd, PWF_STANDARD);
}

static int
pw_update(struct passwd * pwd, char const * user, int mode)
{
	int             rc = 0;

	endpwent();

	/*
	 * First, let's check the see if the database is alright
	 * Note: -c is only available in FreeBSD 2.2 and above
	 */
#ifdef HAVE_PWDB_C
	if (pwdb("-c", NULL) == 0) {	/* Check only */
#else
	{				/* No -c */
#endif
		char            pfx[32];
		char            pwbuf[PWBUFSZ];
		int             l = sprintf(pfx, "%s:", user);

		/*
		 * Update the passwd file first
		 */
		if (pwd == NULL)
			*pwbuf = '\0';
		else
			fmtpwentry(pwbuf, pwd, PWF_PASSWD);
		if ((rc = fileupdate(_PATH_PASSWD, 0644, pwbuf, pfx, l, mode)) != 0) {

			/*
			 * Then the master.passwd file
			 */
			if (pwd != NULL)
				fmtpwentry(pwbuf, pwd, PWF_MASTER);
			if ((rc = fileupdate(_PATH_MASTERPASSWD, 0644, pwbuf, pfx, l, mode)) != 0)
				rc = pwdb(NULL) == 0;
		}
	}
	return rc;
}

int
addpwent(struct passwd * pwd)
{
	return pw_update(pwd, pwd->pw_name, UPD_CREATE);
}

int
chgpwent(char const * login, struct passwd * pwd)
{
	return pw_update(pwd, login, UPD_REPLACE);
}

int
delpwent(struct passwd * pwd)
{
	return pw_update(NULL, pwd->pw_name, UPD_DELETE);
}
