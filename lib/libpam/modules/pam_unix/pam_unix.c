/*-
 * Copyright 1998 Juniper Networks, Inc.
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
 *	$FreeBSD$
 */

#include <sys/types.h>
#include <sys/time.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#include <security/pam_modules.h>

#include "pam_mod_misc.h"

#define PASSWORD_PROMPT	"Password:"
#define DEFAULT_WARN  (2L * 7L * 86400L)  /* Two weeks */

enum { PAM_OPT_AUTH_AS_SELF=PAM_OPT_STD_MAX, PAM_OPT_NULLOK };

static struct opttab other_options[] = {
	{ "auth_as_self",	PAM_OPT_AUTH_AS_SELF },
	{ "nullok",		PAM_OPT_NULLOK },
	{ NULL, 0 }
};

/*
 * authentication management
 */

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct passwd *pwd;
	int retval;
	const char *password, *user;
	char *encrypted;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL))
		pwd = getpwuid(getuid());
	else {
		retval = pam_get_user(pamh, &user, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		pwd = getpwnam(user);
	}

	PAM_LOG("Got user: %s", user);

	if (pwd != NULL) {

		PAM_LOG("Doing real authentication");

		if (pwd->pw_passwd[0] == '\0'
		    && pam_test_option(&options, PAM_OPT_NULLOK, NULL)) {
			/*
			 * No password case. XXX Are we giving too much away
			 * by not prompting for a password?
			 */
			PAM_LOG("No password, and null password OK");
			PAM_RETURN(PAM_SUCCESS);
		}
		else {
			retval = pam_get_pass(pamh, &password, PASSWORD_PROMPT,
			    &options);
			if (retval != PAM_SUCCESS)
				PAM_RETURN(retval);
			PAM_LOG("Got password");
		}
		encrypted = crypt(password, pwd->pw_passwd);
		if (password[0] == '\0' && pwd->pw_passwd[0] != '\0')
			encrypted = ":";

		PAM_LOG("Encrypted passwords are: %s & %s", encrypted,
		    pwd->pw_passwd);

		retval = strcmp(encrypted, pwd->pw_passwd) == 0 ?
		    PAM_SUCCESS : PAM_AUTH_ERR;
	}
	else {

		PAM_LOG("Doing dummy authentication");

		/*
		 * User unknown.
		 * Encrypt a dummy password so as to not give away too much.
		 */
		retval = pam_get_pass(pamh, &password, PASSWORD_PROMPT,
		    &options);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		PAM_LOG("Got password");
		crypt(password, "xx");
		retval = PAM_AUTH_ERR;
	}

	/*
	 * The PAM infrastructure will obliterate the cleartext
	 * password before returning to the application.
	 */
	PAM_RETURN(retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

/* 
 * account management
 *
 * check pw_change and pw_expire fields
 */
PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct passwd *pw;
	struct timeval tp;
	login_cap_t *lc;
	time_t warntime;
	int retval;
	const char *user;
	char buf[128];

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (retval != PAM_SUCCESS || user == NULL)
		/* some implementations return PAM_SUCCESS here */
		PAM_RETURN(PAM_USER_UNKNOWN);

	pw = getpwnam(user);
	if (pw == NULL)
		PAM_RETURN(PAM_USER_UNKNOWN);

	PAM_LOG("Got user: %s", user);

	retval = PAM_SUCCESS;
	lc = login_getpwclass(pw);

	if (pw->pw_change || pw->pw_expire)
		gettimeofday(&tp, NULL);

	warntime = login_getcaptime(lc, "warnpassword", DEFAULT_WARN,
	    DEFAULT_WARN);

	PAM_LOG("Got login_cap");

	if (pw->pw_change) {
		if (tp.tv_sec >= pw->pw_change)
			/* some implementations return PAM_AUTHTOK_EXPIRED */
			retval = PAM_NEW_AUTHTOK_REQD;
		else if (pw->pw_change - tp.tv_sec < warntime) {
			snprintf(buf, sizeof(buf),
			    "Warning: your password expires on %s",
			    ctime(&pw->pw_change));
			pam_prompt(pamh, PAM_ERROR_MSG, buf, NULL);
		}
	}

	warntime = login_getcaptime(lc, "warnexpire", DEFAULT_WARN,
	    DEFAULT_WARN);

	if (pw->pw_expire) {
		if (tp.tv_sec >= pw->pw_expire)
			retval = PAM_ACCT_EXPIRED;
		else if (pw->pw_expire - tp.tv_sec < warntime) {
			snprintf(buf, sizeof(buf),
			    "Warning: your account expires on %s",
			    ctime(&pw->pw_expire));
			pam_prompt(pamh, PAM_ERROR_MSG, buf, NULL);
		}
	}

	login_close(lc);

	PAM_RETURN(retval);
}

PAM_MODULE_ENTRY("pam_unix");
