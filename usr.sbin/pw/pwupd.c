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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "pwupd.h"

#define HAVE_PWDB_C	1
#define	HAVE_PWDB_U	1

static char pathpwd[] = _PATH_PWD;
static char * pwpath = pathpwd;
 
int
setpwdir(const char * dir)
{
	if (dir == NULL)
		return -1;
	else {
		char * d = malloc(strlen(dir)+1);
		if (d == NULL)
			return -1;
		pwpath = strcpy(d, dir);
	}
	return 0;
}

char *
getpwpath(char const * file)
{
	static char pathbuf[MAXPATHLEN];

	snprintf(pathbuf, sizeof pathbuf, "%s/%s", pwpath, file);
	return pathbuf;
}

int
pwdb(char *arg,...)
{
	int             i = 0;
	pid_t           pid;
	va_list         ap;
	char           *args[10];

	args[i++] = _PATH_PWD_MKDB;
	va_start(ap, arg);
	while (i < 6 && arg != NULL) {
		args[i++] = arg;
		arg = va_arg(ap, char *);
	}
	if (pwpath != pathpwd) {
		args[i++] = "-d";
		args[i++] = pwpath;
	}
	args[i++] = getpwpath(_MASTERPASSWD);
	args[i] = NULL;

	if ((pid = fork()) == -1)	/* Error (errno set) */
		i = errno;
	else if (pid == 0) {	/* Child */
		execv(args[0], args);
		_exit(1);
	} else {		/* Parent */
		waitpid(pid, &i, 0);
		if (WEXITSTATUS(i))
			i = EIO;
	}
	return i;
}

int
fmtpwentry(char *buf, struct passwd * pwd, int type)
{
	int             l;
	char           *pw;

	pw = (type == PWF_MASTER) ?
	    ((pwd->pw_passwd == NULL) ? "" : pwd->pw_passwd) : "*";

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

	ENDPWENT();

	/*
	 * First, let's check the see if the database is alright
	 * Note: -C is only available in FreeBSD 2.2 and above
	 */
#ifdef HAVE_PWDB_C
	if (pwdb("-C", NULL) == 0) {	/* Check only */
#else
	{				/* No -C */
#endif
		char            pfx[PWBUFSZ];
		char            pwbuf[PWBUFSZ];
		int             l = snprintf(pfx, PWBUFSZ, "%s:", user);
#ifdef HAVE_PWDB_U
		int		isrename = pwd!=NULL && strcmp(user, pwd->pw_name);
#endif

		/*
		 * Update the passwd file first
		 */
		if (pwd == NULL)
			*pwbuf = '\0';
		else
			fmtpwentry(pwbuf, pwd, PWF_PASSWD);

		if (l < 0)
			l = 0;
		rc = fileupdate(getpwpath(_PASSWD), 0644, pwbuf, pfx, l, mode);
		if (rc == 0) {

			/*
			 * Then the master.passwd file
			 */
			if (pwd != NULL)
				fmtpwentry(pwbuf, pwd, PWF_MASTER);
			rc = fileupdate(getpwpath(_MASTERPASSWD), 0600, pwbuf, pfx, l, mode);
			if (rc == 0) {
#ifdef HAVE_PWDB_U
				if (mode == UPD_DELETE || isrename)
#endif
					rc = pwdb(NULL);
#ifdef HAVE_PWDB_U
				else
					rc = pwdb("-u", user, NULL);
#endif
			}
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
