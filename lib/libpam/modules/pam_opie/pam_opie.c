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
 *	$FreeBSD: src/lib/libpam/modules/pam_opie/pam_opie.c,v 1.1 2000/04/17 00:14:42 kris Exp $
 */

#include <syslog.h>	/* XXX */

#include <stdio.h>
#include <string.h>
#include <opie.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>

#include "pam_mod_misc.h"

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
    const char **argv)
{
	int retval;
	const char *user;
	const char *response;
	struct opie opie;
	char challenge[OPIE_CHALLENGE_MAX];
	char prompt[OPIE_CHALLENGE_MAX+22];
	char resp_buf[OPIE_SECRET_MAX];
	int options;
	int i;

	user = NULL;
	options = 0;
	for (i = 0;  i < argc;  i++)
		pam_std_option(&options, argv[i]);
	/*
	 * It doesn't make sense to use a password that has already been
	 * typed in, since we haven't presented the challenge to the user
	 * yet.
	 */
	options &= ~(PAM_OPT_USE_FIRST_PASS | PAM_OPT_TRY_FIRST_PASS);
	if ((retval = pam_get_user(pamh, (const char **)&user, NULL))
	    != PAM_SUCCESS)
		return retval;
	/*
	 * Don't call the OPIE atexit() handler when our program exits,
	 * since the module has been unloaded and we will SEGV.
	 */
	opiedisableaeh();

	if (opiechallenge(&opie, (char *)user, challenge) != 0)
		return PAM_AUTH_ERR;
	snprintf(prompt, sizeof prompt, "%s\nPassword: ", challenge);
	if ((retval = pam_get_pass(pamh, &response, prompt, options)) !=
	    PAM_SUCCESS) {
		opieunlock();
		return retval;
	}
	if (response[0] == '\0' && !(options & PAM_OPT_ECHO_PASS)) {
		options |= PAM_OPT_ECHO_PASS;
		snprintf(prompt, sizeof prompt,
			 "%s\nPassword [echo on]: ", challenge);
		if ((retval = pam_get_pass(pamh, &response, prompt,
		    options)) != PAM_SUCCESS) {
			opieunlock();
			return retval;
		}
	}
	/* We have to copy the response, because opieverify mucks with it. */
	snprintf(resp_buf, sizeof resp_buf, "%s", response);
	/*
	 * Opieverify is supposed to return -1 only if an error occurs.
	 * But it returns -1 even if the response string isn't in the form
	 * it expects.  Thus we can't log an error and can only check for
	 * success or lack thereof.
	 */
	return opieverify(&opie, resp_buf) == 0 ? PAM_SUCCESS : PAM_AUTH_ERR;
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_MODULE_ENTRY("pam_opie");
