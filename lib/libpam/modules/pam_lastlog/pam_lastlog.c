/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2001 Mark R V Murray
 * All rights reserved.
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _BSD_SOURCE

#include <sys/param.h>

#include <fcntl.h>
#include <libutil.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

extern int login_access(const char *, const char *);

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh __unused, int flags __unused, int argc ,const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct passwd *pwd;
	struct utmp utmp;
	struct lastlog ll;
	const char *rhost, *user, *tty;
	off_t llpos;
	int fd, pam_err;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	pam_err = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (pam_err != PAM_SUCCESS)
		PAM_RETURN(pam_err);
	if (user == NULL || (pwd = getpwnam(user)) == NULL)
		PAM_RETURN(PAM_SERVICE_ERR);
	PAM_LOG("Got user: %s", user);

	pam_err = pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
	if (pam_err != PAM_SUCCESS)
		PAM_RETURN(pam_err);
	
	pam_err = pam_get_item(pamh, PAM_TTY, (const void **)&tty);
	if (pam_err != PAM_SUCCESS)
		PAM_RETURN(pam_err);
	if (tty == NULL)
		PAM_RETURN(PAM_SERVICE_ERR);
	
	fd = open(_PATH_LASTLOG, O_RDWR|O_CREAT, 0644);
	if (fd == -1) {
		syslog(LOG_ERR, "cannot open %s: %m", _PATH_LASTLOG);
		PAM_RETURN(PAM_SERVICE_ERR);
	}

	/*
	 * Record session in lastlog(5).
	 */
	llpos = (off_t)(pwd->pw_uid * sizeof(ll));
	if (lseek(fd, llpos, L_SET) != llpos)
		goto file_err;
	if ((flags & PAM_SILENT) == 0) {
		if (read(fd, &ll, sizeof(ll)) == sizeof(ll) &&
		    ll.ll_time != 0) {
			pam_info(pamh, "Last login: %.*s ", 24 - 5,
			    ctime(&ll.ll_time));
			if (*ll.ll_host != '\0')
				pam_info(pamh, "from %.*s\n",
				    (int)sizeof(ll.ll_host), ll.ll_host);
			else
				pam_info(pamh, "on %.*s\n",
				    (int)sizeof(ll.ll_line), ll.ll_line);
		}
		if (lseek(fd, llpos, L_SET) != llpos)
			goto file_err;
	}
	
	bzero(&ll, sizeof(ll));
	time(&ll.ll_time);
	
	/* note: does not need to be NUL-terminated */
	strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
	if (rhost != NULL)
		/* note: does not need to be NUL-terminated */
		strncpy(ll.ll_host, rhost, sizeof(ll.ll_host));
	
	if (write(fd, (char *)&ll, sizeof(ll)) != sizeof(ll) || close(fd) != 0)
		goto file_err;

	PAM_LOG("Login recorded in %s", _PATH_LASTLOG);
	
	/*
	 * Record session in utmp(5) and wtmp(5).
	 */
	bzero(&utmp, sizeof(utmp));
	time(&utmp.ut_time);
	/* note: does not need to be NUL-terminated */
	strncpy(utmp.ut_name, user, sizeof(utmp.ut_name));
	if (rhost != NULL)
		strncpy(utmp.ut_host, rhost, sizeof(utmp.ut_host));
	(void)strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
	login(&utmp);
	
	PAM_RETURN(PAM_IGNORE);
	
 file_err:
	syslog(LOG_ERR, "%s: %m", _PATH_LASTLOG);
	close(fd);
	PAM_RETURN(PAM_SERVICE_ERR);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_MODULE_ENTRY("pam_lastlog");
