/*-
 * Copyright 2000 James Bloom
 * All rights reserved.
 * Based upon code Copyright 1998 Juniper Networks, Inc.
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
 *	$FreeBSD$
 */

#include <sys/types.h>
#include <opie.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>
#include "pam_mod_misc.h"

enum { PAM_OPT_AUTH_AS_SELF=PAM_OPT_STD_MAX };

static struct opttab other_options[] = {
	{ "auth_as_self",	PAM_OPT_AUTH_AS_SELF },
	{ NULL, 0 }
};

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct opie opie;
	struct options options;
	struct passwd *pwd;
	int retval, i;
	char *(promptstr[]) = { "%s\nPassword: ", "%s\nPassword [echo on]: "};
	char challenge[OPIE_CHALLENGE_MAX];
	char prompt[OPIE_CHALLENGE_MAX+22];
	char resp[OPIE_SECRET_MAX];
	const char *user;
	const char *response;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	/*
	 * It doesn't make sense to use a password that has already been
	 * typed in, since we haven't presented the challenge to the user
	 * yet.
	 */
	if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) ||
	    pam_test_option(&options, PAM_OPT_TRY_FIRST_PASS, NULL))
		PAM_RETURN(PAM_AUTH_ERR);

	user = NULL;
	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL)) {
		pwd = getpwuid(getuid());
		user = pwd->pw_name;
	}
	else {
		retval = pam_get_user(pamh, (const char **)&user, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
	}

	PAM_LOG("Got user: %s", user);

	/*
	 * Don't call the OPIE atexit() handler when our program exits,
	 * since the module has been unloaded and we will SEGV.
	 */
	opiedisableaeh();

	opiechallenge(&opie, (char *)user, challenge);
	for (i = 0; i < 2; i++) {
		snprintf(prompt, sizeof prompt, promptstr[i], challenge);
		retval = pam_get_pass(pamh, &response, prompt, &options);
		if (retval != PAM_SUCCESS) {
			opieunlock();
			PAM_RETURN(retval);
		}

		PAM_LOG("Completed challenge %d: %s", i, response);

		if (response[0] != '\0')
			break;

		/* Second time round, echo the password */
		pam_set_option(&options, PAM_OPT_ECHO_PASS);
	}

	/* We have to copy the response, because opieverify mucks with it. */
	snprintf(resp, sizeof resp, "%s", response);

	/*
	 * Opieverify is supposed to return -1 only if an error occurs.
	 * But it returns -1 even if the response string isn't in the form
	 * it expects.  Thus we can't log an error and can only check for
	 * success or lack thereof.
	 */
	PAM_RETURN(opieverify(&opie, resp) == 0 ? PAM_SUCCESS : PAM_AUTH_ERR);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_MODULE_ENTRY("pam_opie");
