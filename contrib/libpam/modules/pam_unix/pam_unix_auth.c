/*
 * Copyright Alexander O. Yuriev, 1996.  All rights reserved.
 * NIS+ support by Thorsten Kukuk <kukuk@weber.uni-paderborn.de>
 * Copyright Jan Rêkorajski, 1999.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* #define DEBUG */

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

/* indicate the following groups are defined */

#define PAM_SM_AUTH

#define _PAM_EXTERN_FUNCTIONS
#include <security/_pam_macros.h>
#include <security/pam_modules.h>

#ifndef LINUX_PAM
#include <security/pam_appl.h>
#endif				/* LINUX_PAM */

#include "support.h"

/*
 * PAM framework looks for these entry-points to pass control to the
 * authentication module.
 */

/* Fun starts here :)

 * pam_sm_authenticate() performs UNIX/shadow authentication
 *
 *      First, if shadow support is available, attempt to perform
 *      authentication using shadow passwords. If shadow is not
 *      available, or user does not have a shadow password, fallback
 *      onto a normal UNIX authentication
 */

#define _UNIX_AUTHTOK  "-UN*X-PASS"

#define AUTH_RETURN						\
{								\
	if (on(UNIX_LIKE_AUTH, ctrl) && ret_data) {		\
		D(("recording return code for next time [%d]",	\
					retval));		\
		pam_set_data(pamh, "unix_setcred_return",	\
				(void *) retval, NULL);	        \
	}							\
	D(("done. [%s]", pam_strerror(pamh, retval)));		\
	return retval;						\
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t * pamh, int flags
				   ,int argc, const char **argv)
{
	unsigned int ctrl;
	int retval, *ret_data = NULL;
	const char *name, *p;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, argc, argv);

	/* Get a few bytes so we can pass our return value to
	   pam_sm_setcred(). */
	ret_data = malloc(sizeof(int));

	/* get the user'name' */

	retval = pam_get_user(pamh, &name, "login: ");
	if (retval == PAM_SUCCESS) {
		/*
		 * Various libraries at various times have had bugs related to
		 * '+' or '-' as the first character of a user name. Don't take
		 * any chances here. Require that the username starts with an
		 * alphanumeric character.
		 */
		if (name == NULL || !isalnum(*name)) {
			_log_err(LOG_ERR, pamh, "bad username [%s]", name);
			retval = PAM_USER_UNKNOWN;
			AUTH_RETURN
		}
		if (retval == PAM_SUCCESS && on(UNIX_DEBUG, ctrl))
			D(("username [%s] obtained", name));
	} else {
		D(("trouble reading username"));
		if (retval == PAM_CONV_AGAIN) {
			D(("pam_get_user/conv() function is not ready yet"));
			/* it is safe to resume this function so we translate this
			 * retval to the value that indicates we're happy to resume.
			 */
			retval = PAM_INCOMPLETE;
		}
		AUTH_RETURN
	}

	/* if this user does not have a password... */

	if (_unix_blankpasswd(ctrl, name)) {
		D(("user '%s' has blank passwd", name));
		name = NULL;
		retval = PAM_SUCCESS;
		AUTH_RETURN
	}
	/* get this user's authentication token */

	retval = _unix_read_password(pamh, ctrl, NULL, "Password: ", NULL
				     ,_UNIX_AUTHTOK, &p);
	if (retval != PAM_SUCCESS) {
		if (retval != PAM_CONV_AGAIN) {
			_log_err(LOG_CRIT, pamh, "auth could not identify password for [%s]"
				 ,name);
		} else {
			D(("conversation function is not ready yet"));
			/*
			 * it is safe to resume this function so we translate this
			 * retval to the value that indicates we're happy to resume.
			 */
			retval = PAM_INCOMPLETE;
		}
		name = NULL;
		AUTH_RETURN
	}
	D(("user=%s, password=[%s]", name, p));

	/* verify the password of this user */
	retval = _unix_verify_password(pamh, name, p, ctrl);
	name = p = NULL;

	AUTH_RETURN
}


/* 
 * The only thing _pam_set_credentials_unix() does is initialization of
 * UNIX group IDs.
 *
 * Well, everybody but me on linux-pam is convinced that it should not
 * initialize group IDs, so I am not doing it but don't say that I haven't
 * warned you. -- AOY
 */

PAM_EXTERN int pam_sm_setcred(pam_handle_t * pamh, int flags
			      ,int argc, const char **argv)
{
	unsigned int ctrl;
	int retval;

	D(("called."));

	/* FIXME: it shouldn't be necessary to parse the arguments again. The
	   only argument we need is UNIX_LIKE_AUTH: if it was set,
	   pam_get_data will succeed. If it wasn't, it will fail, and we
	   return PAM_SUCCESS.  -SRL */
	ctrl = _set_ctrl(pamh, flags, NULL, argc, argv);
	retval = PAM_SUCCESS;

	if (on(UNIX_LIKE_AUTH, ctrl)) {
		int *pretval = NULL;

		D(("recovering return code from auth call"));
		pam_get_data(pamh, "unix_setcred_return", (const void **) pretval);
		if(pretval) {
			retval = *pretval;
			free(pretval);
			D(("recovered data indicates that old retval was %d", retval));
		}
	}
	return retval;
}

#ifdef PAM_STATIC
struct pam_module _pam_unix_auth_modstruct = {
    "pam_unix_auth",
    pam_sm_authenticate,
    pam_sm_setcred,
    NULL,
    NULL,
    NULL,
    NULL,
};
#endif
