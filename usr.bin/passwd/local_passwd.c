/*-
 * Copyright (c) 1990, 1993, 1994
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
 * $Id: local_passwd.c,v 1.9 1996/07/01 19:38:24 guido Exp $
 */

#ifndef lint
static const char sccsid[] = "@(#)local_passwd.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pw_copy.h>
#include <pw_util.h>
#ifdef YP
#include <pw_yp.h>
#endif

#ifdef LOGGING
#include <syslog.h>
#endif

#include "extern.h"

static uid_t uid;

char   *tempname;

static unsigned char itoa64[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
to64(s, v, n)
	char *s;
	long v;
	int n;
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

char *
getnewpasswd(pw, nis)
	struct passwd *pw;
	int nis;
{
	int tries;
	char *p, *t;
	char buf[_PASSWORD_LEN+1], salt[10];
	struct timeval tv;

	if (!nis)
		(void)printf("Changing local password for %s.\n", pw->pw_name);

	if (uid && pw->pw_passwd[0] &&
	    strcmp(crypt(getpass("Old password:"), pw->pw_passwd),
	    pw->pw_passwd)) {
		errno = EACCES;
		pw_error(NULL, 1, 1);
	}

	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (!*p) {
			(void)printf("Password unchanged.\n");
			pw_error(NULL, 0, 0);
		}
		if (strlen(p) <= 5 && (uid != 0 || ++tries < 2)) {
			(void)printf("Please enter a longer password.\n");
			continue;
		}
		for (t = p; *t && islower(*t); ++t);
		if (!*t && (uid != 0 || ++tries < 2)) {
			(void)printf("Please don't use an all-lower case password.\nUnusual capitalization, control characters or digits are suggested.\n");
			continue;
		}
		(void)strcpy(buf, p);
		if (!strcmp(buf, getpass("Retype new password:")))
			break;
		(void)printf("Mismatch; try again, EOF to quit.\n");
	}
	/* grab a random printable character that isn't a colon */
	(void)srandom((int)time((time_t *)NULL));
#ifdef NEWSALT
	salt[0] = _PASSWORD_EFMT1;
	to64(&salt[1], (long)(29 * 25), 4);
	to64(&salt[5], random(), 4);
	salt[9] = '\0';
#else
	/* Make a good size salt for algoritms that can use it. */
	gettimeofday(&tv,0);
	if (strncmp(pw->pw_passwd, "$1$", 3)) {
	    /* DES Salt */
	    to64(&salt[0], random(), 3);
	    to64(&salt[3], tv.tv_usec, 3);
	    to64(&salt[6], tv.tv_sec, 2);
	    salt[8] = '\0';
	}
	else {
	    /* MD5 Salt */
	    strncpy(&salt[0], "$1$", 3);
	    to64(&salt[3], random(), 3);
	    to64(&salt[6], tv.tv_usec, 3);
	    salt[8] = '\0';
	}
#endif
	return (crypt(buf, salt));
}

int
local_passwd(uname)
	char *uname;
{
	struct passwd *pw;
	int pfd, tfd;

	if (!(pw = getpwnam(uname)))
		errx(1, "unknown user %s", uname);

#ifdef YP
	/* Use the right password information. */
	pw = (struct passwd *)&local_password;
#endif
	uid = getuid();
	if (uid && uid != pw->pw_uid)
		errx(1, "%s", strerror(EACCES));

	pw_init();
	pfd = pw_lock();
	tfd = pw_tmp();

	/*
	 * Get the new password.  Reset passwd change time to zero; when
	 * classes are implemented, go and get the "offset" value for this
	 * class and reset the timer.
	 */
	pw->pw_passwd = getnewpasswd(pw, 0);
	pw->pw_change = 0;
	pw_copy(pfd, tfd, pw);

	if (!pw_mkdb(uname))
		pw_error((char *)NULL, 0, 1);
#ifdef LOGGING
	syslog(LOG_DEBUG, "user %s changed their local password\n", uname);
#endif
	return (0);
}
