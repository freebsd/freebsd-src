/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)pw_copy.c	5.3 (Berkeley) 5/2/91";
#endif /* not lint */

/*
 * This module is used to copy the master password file, replacing a single
 * record, by chpass(1) and passwd(1).
 */

#include <pwd.h>
#include <stdio.h>
#include <string.h>

extern char *progname, *tempname;

int globcnt;

/*
 * NB: Use of pw_copy() to update the insecure passwd file
 * necessitates that this routine be wrapperized
 * so that it can handle the formats used by both 
 * /etc/master.passwd and /etc/passwd
 *
 * pw_copy() and pw_copy_insecure() both call pw_copy_drv(), which
 * does the work.
 */
static pw_copy_drv(ffd, tfd, pw, secure_format)
	int ffd, tfd;
	struct passwd *pw;
	int secure_format;
{
	register FILE *from, *to;
	register int done;
	register char *p;
	char buf[8192];
        int tmpcnt;

	if (!(from = fdopen(ffd, "r")))
		pw_error(_PATH_MASTERPASSWD, 1, 1);
	if (!(to = fdopen(tfd, "w")))
		pw_error(tempname, 1, 1);

        tmpcnt=0;
	for (done = 0; fgets(buf, sizeof(buf), from);) {
            tmpcnt++;
		if (!index(buf, '\n')) {
			(void)fprintf(stderr, "%s: %s: line too long\n",
			    progname, _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		if (done) {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		if (!(p = index(buf, ':'))) {
			(void)fprintf(stderr, "%s: %s: corrupted entry\n",
			    progname, _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		*p = '\0';
		if (strcmp(buf, pw->pw_name)) {
			*p = ':';
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
                globcnt = tmpcnt;
		/*
		 * NB: /etc/passwd: insecure format does not have
		 * class, change and expire fields !
		 */
		if(secure_format)
			(void)fprintf(to, "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
		    		pw->pw_name, pw->pw_passwd, pw->pw_uid, 
				pw->pw_gid, pw->pw_class, pw->pw_change, 
				pw->pw_expire, pw->pw_gecos, pw->pw_dir, 
				pw->pw_shell);
		else
			(void)fprintf(to, "%s:%s:%d:%d:%s:%s:%s\n",
				pw->pw_name, pw->pw_passwd, pw->pw_uid,
				pw->pw_gid, pw->pw_gecos, pw->pw_dir,
				pw->pw_shell);
		done = 1;
		if (ferror(to))
			goto err;
	}
	if (!done)
        {
                globcnt = tmpcnt+1;
		/*
		 * NB: /etc/passwd: insecure format does not have
		 * class, change and expire fields !
		 */
		if(secure_format)
			(void)fprintf(to, "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
		    		pw->pw_name, pw->pw_passwd, pw->pw_uid, 
				pw->pw_gid, pw->pw_class, pw->pw_change, 
				pw->pw_expire, pw->pw_gecos, pw->pw_dir, 
				pw->pw_shell);
		else
			(void)fprintf(to, "%s:%s:%d:%d:%s:%s:%s\n",
				pw->pw_name, pw->pw_passwd, pw->pw_uid,
				pw->pw_gid, pw->pw_gecos, pw->pw_dir,
				pw->pw_shell);
        }

	if (ferror(to))
err:		pw_error(NULL, 1, 1);
	(void)fclose(to);
}


/*
 * Standard pw_copy routine - used to update master.passwd
 */
pw_copy(ffd, tfd, pw)
	int ffd, tfd;
	struct passwd *pw;
{
	pw_copy_drv(ffd, tfd, pw, 1);
}


/*
 * Special pw_copy routine used to update insecure passwd file
 */
pw_copy_insecure(ffd, tfd, pw)
	int ffd, tfd;
	struct passwd *pw;
{
	pw_copy_drv(ffd, tfd, pw, 0);
}
