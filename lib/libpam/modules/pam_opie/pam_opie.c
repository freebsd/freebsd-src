/*-
 * Copyright 2000 James Bloom
 * All rights reserved.
 * Based upon code Copyright 1998 Juniper Networks, Inc. 
 * Copyright (c) 2001 Networks Associates Technologies, Inc.
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

#include <sys/types.h>
#include <opie.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

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
	int retval, i, pwok;
	char *(promptstr[]) = { "%s\nPassword: ", "%s\nPassword [echo on]: "};
	char challenge[OPIE_CHALLENGE_MAX];
	char prompt[OPIE_CHALLENGE_MAX+22];
	char resp[OPIE_SECRET_MAX];
	const char *user, *response, *rhost;
	char *encrypted;

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
		pwd = getpwnam(getlogin());
		user = pwd->pw_name;
	}
	else {
		retval = pam_get_user(pamh, (const char **)&user, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		pwd = getpwnam(user);
	}

	PAM_LOG("Got user: %s", user);

	/*
	 * Don't call the OPIE atexit() handler when our program exits,
	 * since the module has been unloaded and we will SEGV.
	 */
	opiedisableaeh();

	if (opiechallenge(&opie, (char *)user, challenge) == 0) {
		rhost = NULL;
		(void) pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
		pwok = (pwd != NULL) &&
		       (rhost != NULL) && (*rhost != '\0') &&
		       opieaccessfile((char *)rhost) &&
		       opiealways(pwd->pw_dir);
	} else
		pwok = 1;
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
	if (opieverify(&opie, resp) == 0)
		retval = PAM_SUCCESS;
	else if (pwok) {
		encrypted = crypt(resp, pwd->pw_passwd);
		if (resp[0] == '\0' && pwd->pw_passwd[0] != '\0')
			encrypted = ":";
		if (strcmp(encrypted, pwd->pw_passwd) != 0 ||
		    (pwd->pw_expire && time(NULL) >= pwd->pw_expire))
			retval = PAM_AUTH_ERR;
		else
			retval = PAM_SUCCESS;
	} else
		retval = PAM_AUTH_ERR;
	PAM_RETURN(retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc ,const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
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

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_MODULE_ENTRY("pam_opie");
