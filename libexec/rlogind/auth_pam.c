/*-
 * Copyright (c) 1999 John Polstra
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
 *	$FreeBSD: src/libexec/rlogind/auth_pam.c,v 1.1 1999/09/19 22:05:30 markm Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Attempt to authenticate the user using PAM.  Returns 0 if the user is
 * authenticated, or 1 if not authenticated.  If some sort of PAM system
 * error occurs (e.g., the "/etc/pam.conf" file is missing) then this
 * function returns -1.  This can be used as an indication that we should
 * fall back to a different authentication mechanism.
 */

int
auth_pam(char *username)
{
	struct passwd *pawd;
	pam_handle_t *pamh = NULL;
	const char *tmpl_user;
	const void *item;
	int rval;
	char *tty, *ttyn;
	char hostname[MAXHOSTNAMELEN];
	char tname[sizeof(_PATH_TTY) + 10];
	int e;

	static struct pam_conv conv = { misc_conv, NULL };

	ttyn = ttyname(STDIN_FILENO);

	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if ((tty = strrchr(ttyn, '/')) != NULL)
		++tty;
	else
		tty = ttyn;

	rval = gethostname(hostname, sizeof(hostname));

	if (rval < 0) {
		syslog(LOG_ERR, "auth_pam: Failed to resolve local hostname");
		return -1;
	}
	if ((e = pam_start("rshd", username, &conv, &pamh)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_start: %s", pam_strerror(pamh, e));
		return -1;
	}
	if ((e = pam_set_item(pamh, PAM_TTY, tty)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_set_item(PAM_TTY): %s",
			pam_strerror(pamh, e));
		return -1;
	}
	if (hostname != NULL &&
		(e = pam_set_item(pamh,PAM_RHOST, hostname)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_set_item(PAM_RHOST): %s",
			pam_strerror(pamh, e));
		return -1;
	}

	e = pam_authenticate(pamh, 0);

	switch (e) {
	case PAM_SUCCESS:
		/*
		 * With PAM we support the concept of a "template"
		 * user.  The user enters a login name which is
		 * authenticated by PAM, usually via a remote service
		 * such as RADIUS or TACACS+.  If authentication
		 * succeeds, a different but related "template" name
		 * is used for setting the credentials, shell, and
		 * home directory.  The name the user enters need only
		 * exist on the remote authentication server, but the
		 * template name must be present in the local password
		 * database.
		 *
		 * This is supported by two various mechanisms in the
		 * individual modules.  However, from the application's
		 * point of view, the template user is always passed
		 * back as a changed value of the PAM_USER item.
		 */
		if ((e = pam_get_item(pamh, PAM_USER, &item)) ==
			PAM_SUCCESS) {
			    tmpl_user = (const char *) item;
			    if (strcmp(username, tmpl_user) != 0)
			            pawd = getpwnam(tmpl_user);
		} else
			    syslog(LOG_ERR, "Couldn't get PAM_USER: %s", pam_strerror(pamh, e));
		rval = 0;
		break;

	case PAM_AUTH_ERR:
	case PAM_USER_UNKNOWN:
	case PAM_MAXTRIES:
		rval = 1;
		break;

	default:
		syslog(LOG_ERR, "auth_pam: %s", pam_strerror(pamh, e));
		rval = -1;
		break;
	}
	if ((e = pam_end(pamh, e)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_end: %s", pam_strerror(pamh, e));
		rval = -1;
	}
	return rval;
}
