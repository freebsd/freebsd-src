/*-
 * Copyright (c) 2002 Brian Fundakowski Feldman
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *
 * $FreeBSD$
 */

/*
 * Implement a PAM module which will, given restrictions upon whether the
 * user to be authenticated is root or logging in on a given terminal,
 * will allow the user to be authenticated successfully if the user
 * is currently already logged in on another terminal.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

enum { PAM_OPT_NO_ROOT = PAM_OPT_STD_MAX, PAM_OPT_RESTRICT_TTY };
static struct opttab other_options[] = {
	{ "no_root", PAM_OPT_NO_ROOT },
	{ "restrict_tty", PAM_OPT_RESTRICT_TTY },
	{ NULL, 0 }
};

int getutmp(int *fd, struct utmp *utmp);
int inutmp(struct utmp *utmp, const char *username, uid_t uid);

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags __unused, int argc,
    const char **argv)
{
	struct utmp utmp;
	struct options options;
	struct passwd *pw;
	const char *logname;
	char *lineglob = NULL;
	unsigned int matched = 0;
	int retval, fd = -1;
	
        pam_std_option(&options, other_options, argc, argv);

        PAM_LOG("Options processed");

        retval = pam_get_user(pamh, &logname, NULL);
        if (retval != PAM_SUCCESS)
                PAM_RETURN(retval);
        if (pam_test_option(&options, PAM_OPT_RESTRICT_TTY, &lineglob) &&
	    lineglob != NULL) {
		const char *pam_tty;

		PAM_LOG("Using a restrict_tty glob of `%s'", lineglob);
		retval = pam_get_item(pamh, PAM_TTY, (const void **)&pam_tty);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		if (fnmatch(lineglob, pam_tty, 0) != 0)
			PAM_RETURN(PAM_AUTH_ERR);
	}
	pw = getpwnam(logname);
	if (pw == NULL) {
		warn("Can't look up user `%s'", logname);
		PAM_RETURN(PAM_AUTH_ERR);
	}
	if (pw->pw_uid == 0 &&
	    pam_test_option(&options, PAM_OPT_NO_ROOT, NULL))
		PAM_RETURN(PAM_AUTH_ERR);
	while (getutmp(&fd, &utmp) == 1) {
		if (inutmp(&utmp, logname, pw->pw_uid) == 1)
			matched++;
	}
	if (matched)
		PAM_RETURN(PAM_SUCCESS);
	PAM_RETURN(PAM_AUTH_ERR);
}

PAM_EXTERN int 
pam_sm_setcred(pam_handle_t *pamh __unused, int flags __unused, int argc,
    const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh __unused, int flags __unused, int argc,
    const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh __unused, int flags __unused, int argc,
    const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh __unused, int flags __unused, int argc,
    const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh __unused, int flags __unused, int argc,
    const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_MODULE_ENTRY("pam_alreadyloggedin");

int
getutmp(int *fd, struct utmp *utmp)
{

	if (*fd == -1) {
		*fd = open(_PATH_UTMP, O_RDONLY);
		if (*fd == -1) {
			warn("Failure opening %s", _PATH_UTMP);
			return (-1);
		}
	}
	if (read(*fd, utmp, sizeof(*utmp)) == sizeof(*utmp))
		return (1);
	(void)close(*fd);
	return (0);
}

int
inutmp(struct utmp *utmp, const char *username, uid_t uid)
{
	char ttypath[MAXPATHLEN];
	struct stat sb;

	if (utmp->ut_name[0] == '\0' || utmp->ut_line[0] == '\0')
		return (0);
	utmp->ut_line[sizeof(utmp->ut_line) - 1] = '\0';
	utmp->ut_name[sizeof(utmp->ut_name) - 1] = '\0';
	if (utmp->ut_line[strcspn(utmp->ut_line, "./")] != '\0') {
		warnx("Evil utmp line: `%s'", utmp->ut_line);
		return (-1);
	}
	if (*username && strcmp(username, utmp->ut_name) != 0)
		return (0);
	/* can't fail */
	(void)snprintf(ttypath, sizeof(ttypath), "/dev/%s", utmp->ut_line);
	if (stat(ttypath, &sb) == -1) {
		warn("Can't stat line `%s'", ttypath);
		return (-1);
	}
	if (sb.st_uid != uid) {
		warnx("Line's uid %d does not match %d", sb.st_uid,
		    uid);
		return (-1);
	}
	return (1);
}
