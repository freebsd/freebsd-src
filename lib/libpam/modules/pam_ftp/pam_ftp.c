/*-
 * Copyright (c) 2001 Mark R V Murray
 * All rights reserved.
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

#define	PROMPT		"Password required for %s."
#define	GUEST_PROMPT	"Guest login ok, send your e-mail address as password."

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#include <security/pam_modules.h>
#include <pam_mod_misc.h>

#include <security/_pam_macros.h>

enum { PAM_OPT_NO_ANON=PAM_OPT_STD_MAX, PAM_OPT_IGNORE, PAM_OPT_USERS };

static struct opttab other_options[] = {
	{ "no_anon",	PAM_OPT_NO_ANON },
	{ "ignore",	PAM_OPT_IGNORE },
	{ "users",	PAM_OPT_USERS },
	{ NULL, 0 }
};

static const char *anonusers[] = {"ftp", "anonymous", NULL};

/* Check if *user is in supplied *list or *anonusers[] list.
 * Place username in *userret
 * Return 1 if listed 0 otherwise
 */
static int 
lookup(const char *user, char *list, const char **userret)
{
	int anon, i;
	char *item, *context, *locallist;

	anon = 0;
	*userret = user;		/* this is the default */
	if (list) {
		*userret = NULL;
		locallist = list;
		while ((item = strtok_r(locallist, ",", &context))) {
			if (*userret == NULL)
				*userret = item;
			if (strcmp(user, item) == 0) {
				anon = 1;
				break;
			}
			locallist = NULL;
		}
	}
	else {
		for (i = 0; anonusers[i] != NULL; i++) {
			if (strcmp(anonusers[i], user) == 0) {
				*userret = anonusers[0];
				anon = 1;
				break;
			}
		}
	}
	return anon;
}

/* Check if the user name is 'ftp' or 'anonymous'.
 * If this is the case, set the PAM_RUSER to the entered email address
 * and succeed, otherwise fail.
 */
PAM_EXTERN int 
pam_sm_authenticate(pam_handle_t * pamh, int flags __unused, int argc, const char **argv)
{
	struct options options;
	int retval, anon;
	char *users, *context, *token, *p;
	const char *user, *prompt;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_user(pamh, &user, NULL);
	if (retval != PAM_SUCCESS || user == NULL)
		PAM_RETURN(PAM_USER_UNKNOWN);

	PAM_LOG("Got user: %s", user);

	users = NULL;
	if (pam_test_option(&options, PAM_OPT_USERS, &users))
		PAM_LOG("Got extra anonymous users: %s", users);

	anon = 0;
	if (!pam_test_option(&options, PAM_OPT_NO_ANON, NULL))
		anon = lookup(user, users, &user);

	PAM_LOG("Done user: %s", user);

	if (anon) {
		retval = pam_set_item(pamh, PAM_USER, (const void *)user);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		prompt = GUEST_PROMPT;
		PAM_LOG("Doing anonymous");
	}
	else {
		prompt = PROMPT;
		PAM_LOG("Doing non-anonymous");
	}

	retval = pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, prompt, &token);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval == PAM_CONV_AGAIN
			? PAM_INCOMPLETE : PAM_AUTHINFO_UNAVAIL);

	PAM_LOG("Got password");

	if (anon) {
		if (!pam_test_option(&options, PAM_OPT_IGNORE, NULL)) {
			p = strtok_r(token, "@", &context);
			if (p != NULL) {
				pam_set_item(pamh, PAM_RUSER, p);
				PAM_LOG("Got ruser: %s", p);
				if (retval == PAM_SUCCESS) {
					p = strtok_r(NULL, "@", &context);
					if (p != NULL) {
						pam_set_item(pamh, PAM_RHOST, p);
						PAM_LOG("Got rhost: %s", p);
					}
				}
			}
		}
		else
			PAM_LOG("Ignoring supplied password structure");

		PAM_LOG("Done anonymous");

		retval = PAM_SUCCESS;

	}
	else {
		pam_set_item(pamh, PAM_AUTHTOK, token);

		PAM_VERBOSE_ERROR("Anonymous module reject");

		PAM_LOG("Done non-anonymous");

		retval = PAM_AUTH_ERR;
	}

	PAM_RETURN(retval);
}

PAM_EXTERN int 
pam_sm_setcred(pam_handle_t * pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
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
pam_sm_open_session(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_MODULE_ENTRY("pam_ftp");
