/*-
 * Copyright 1998 Juniper Networks, Inc.
 * All rights reserved.
 * Copyright (c) 2002 Networks Associates Technology, Inc.
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

#include <sys/param.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAM_SM_AUTH
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

#include "klogin.h"

/* Globals used by klogin.c */
int	notickets = 1;
int	noticketsdontcomplain = 1;
char	*krbtkfile_env;

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
    int argc __unused, const char *argv[] __unused)
{
	int retval;
	const char *user;
	char *principal;
	char *instance;
	const char *password;
	char localhost[MAXHOSTNAMELEN + 1];
	struct passwd *pwd;

	retval = pam_get_user(pamh, &user, NULL);
	if (retval != PAM_SUCCESS)
		return (retval);

	PAM_LOG("Got user: %s", user);

	retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, NULL);
	if (retval != PAM_SUCCESS)
		return (retval);

	PAM_LOG("Got password");

	if (gethostname(localhost, sizeof localhost - 1) == -1)
		return (PAM_SYSTEM_ERR);

	PAM_LOG("Got localhost: %s", localhost);

	principal = strdup(user);
	if (principal == NULL)
		return (PAM_BUF_ERR);

	instance = strchr(principal, '.');
	if (instance != NULL)
		*instance++ = '\0';
	else
		instance = strchr(principal, '\0');

	PAM_LOG("Got principal.instance: %s.%s", principal, instance);

	retval = PAM_AUTH_ERR;
	pwd = getpwnam(user);
	if (pwd != NULL) {
		if (klogin(pwd, instance, localhost, password) == 0) {
			if (notickets && !noticketsdontcomplain)
				PAM_VERBOSE_ERROR("Warning: no Kerberos tickets issued");
			/*
			 * XXX - I think the ticket file isn't supposed to
			 * be created until pam_sm_setcred() is called.
			 */
			if (krbtkfile_env != NULL)
				setenv("KRBTKFILE", krbtkfile_env, 1);
			retval = PAM_SUCCESS;
		}

		PAM_LOG("Done klogin()");

	}
	/*
	 * The PAM infrastructure will obliterate the cleartext
	 * password before returning to the application.
	 */
	free(principal);

	if (retval != PAM_SUCCESS)
		PAM_VERBOSE_ERROR("Kerberos IV refuses you");

	return (retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh __unused, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{

	return (PAM_SUCCESS);
}

PAM_MODULE_ENTRY("pam_kerberosIV");
