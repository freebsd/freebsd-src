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
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>

#include "pam_mod_misc.h"

#define PASSWORD_PROMPT	"Password:"

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
    const char **argv)
{
	int retval;
	const char *user;
	const char *password;
	struct passwd *pwd;
	char *encrypted;
	int options;
	int i;

	options = 0;
	for (i = 0;  i < argc;  i++)
		pam_std_option(&options, argv[i]);
	if ((retval = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS)
		return retval;
	if ((retval = pam_get_pass(pamh, &password, PASSWORD_PROMPT,
	    options)) != PAM_SUCCESS)
		return retval;
	if ((pwd = getpwnam(user)) != NULL) {
		encrypted = crypt(password, pwd->pw_passwd);
		if (password[0] == '\0' && pwd->pw_passwd[0] != '\0')
			encrypted = ":";

		retval = strcmp(encrypted, pwd->pw_passwd) == 0 ?
		    PAM_SUCCESS : PAM_AUTH_ERR;
	} else {
		/*
		 * User unknown.  Encrypt anyway so that it takes the
		 * same amount of time.
		 */
		crypt(password, "xx");
		retval = PAM_AUTH_ERR;
	}
	/*
	 * The PAM infrastructure will obliterate the cleartext
	 * password before returning to the application.
	 */
	return retval;
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_MODULE_ENTRY("pam_unix");
