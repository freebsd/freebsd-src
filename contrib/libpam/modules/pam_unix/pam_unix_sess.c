/*
 * $Id: pam_unix_sess.c,v 1.3 2000/12/20 05:15:05 vorlon Exp $
 *
 * Copyright Alexander O. Yuriev, 1996.  All rights reserved.
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

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* indicate the following groups are defined */

#define PAM_SM_SESSION

#include <security/_pam_macros.h>
#include <security/pam_modules.h>

#ifndef LINUX_PAM
#include <security/pam_appl.h>
#endif				/* LINUX_PAM */

#include "support.h"

/*
 * PAM framework looks for these entry-points to pass control to the
 * session module.
 */

PAM_EXTERN int pam_sm_open_session(pam_handle_t * pamh, int flags,
				   int argc, const char **argv)
{
	char *user_name, *service;
	unsigned int ctrl;
	int retval;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, (void *) &user_name);
	if (user_name == NULL || retval != PAM_SUCCESS) {
		_log_err(LOG_CRIT, pamh,
		         "open_session - error recovering username");
		return PAM_SESSION_ERR;		/* How did we get authenticated with
						   no username?! */
	}
	retval = pam_get_item(pamh, PAM_SERVICE, (void *) &service);
	if (service == NULL || retval != PAM_SUCCESS) {
		_log_err(LOG_CRIT, pamh,
		         "open_session - error recovering service");
		return PAM_SESSION_ERR;
	}
	_log_err(LOG_INFO, pamh, "session opened for user %s by %s(uid=%d)"
		 ,user_name
		 ,PAM_getlogin() == NULL ? "" : PAM_getlogin(), getuid());

	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t * pamh, int flags,
				    int argc, const char **argv)
{
	char *user_name, *service;
	unsigned int ctrl;
	int retval;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, (void *) &user_name);
	if (user_name == NULL || retval != PAM_SUCCESS) {
		_log_err(LOG_CRIT, pamh,
		         "close_session - error recovering username");
		return PAM_SESSION_ERR;		/* How did we get authenticated with
						   no username?! */
	}
	retval = pam_get_item(pamh, PAM_SERVICE, (void *) &service);
	if (service == NULL || retval != PAM_SUCCESS) {
		_log_err(LOG_CRIT, pamh,
		         "close_session - error recovering service");
		return PAM_SESSION_ERR;
	}
	_log_err(LOG_INFO, pamh, "session closed for user %s"
		 ,user_name);

	return PAM_SUCCESS;
}

/* static module data */
#ifdef PAM_STATIC
struct pam_module _pam_unix_session_modstruct = {
    "pam_unix_session",
    NULL,
    NULL,
    NULL,
    pam_sm_open_session,
    pam_sm_close_session,
    NULL,
};
#endif

