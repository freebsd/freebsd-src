/*
 * Copyright Elliot Lee, 1996.  All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <syslog.h>
#include <pwd.h>
#include <shadow.h>
#include <time.h>		/* for time() */

#include <security/_pam_macros.h>

/* indicate that the following groups are defined */

#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>

#ifndef LINUX_PAM
#include <security/pam_appl.h>
#endif				/* LINUX_PAM */

#include "support.h"
 
/*
 * PAM framework looks for this entry-point to pass control to the
 * account management module.
 */

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t * pamh, int flags,
				int argc, const char **argv)
{
	unsigned int ctrl;
	const char *uname;
	int retval, daysleft;
	time_t curdays;
	struct spwd *spent;
	struct passwd *pwent;
	char buf[80];

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, (const void **) &uname);
	D(("user = `%s'", uname));
	if (retval != PAM_SUCCESS || uname == NULL) {
		_log_err(LOG_ALERT, pamh
			 ,"could not identify user (from uid=%d)"
			 ,getuid());
		return PAM_USER_UNKNOWN;
	}

	pwent = getpwnam(uname);
	if (!pwent) {
		_log_err(LOG_ALERT, pamh
			 ,"could not identify user (from getpwnam(%s))"
			 ,uname);
		return PAM_USER_UNKNOWN;
	}

	if (!strcmp( pwent->pw_passwd, "*NP*" )) { /* NIS+ */
		uid_t save_euid, save_uid;

		save_euid = geteuid();
		save_uid = getuid();
		if (save_uid == pwent->pw_uid)
			setreuid( save_euid, save_uid );
		else  {
			setreuid( 0, -1 );
			if (setreuid( -1, pwent->pw_uid ) == -1) {
				setreuid( -1, 0 );
				setreuid( 0, -1 );
				if(setreuid( -1, pwent->pw_uid ) == -1)
					return PAM_CRED_INSUFFICIENT;
			}
		}
		spent = getspnam( uname );
		if (save_uid == pwent->pw_uid)
			setreuid( save_uid, save_euid );
		else {
			if (setreuid( -1, 0 ) == -1)
			setreuid( save_uid, -1 );
			setreuid( -1, save_euid );
		}

	} else if (!strcmp( pwent->pw_passwd, "x" )) {
		spent = getspnam(uname);
	} else {
		return PAM_SUCCESS;
	}

	if (!spent)
		return PAM_AUTHINFO_UNAVAIL;	/* Couldn't get username from shadow */

	curdays = time(NULL) / (60 * 60 * 24);
	D(("today is %d, last change %d", curdays, spent->sp_lstchg));
	if ((curdays > spent->sp_expire) && (spent->sp_expire != -1)
	    && (spent->sp_lstchg != 0)) {
		_log_err(LOG_NOTICE, pamh
			 ,"account %s has expired (account expired)"
			 ,uname);
		_make_remark(pamh, ctrl, PAM_ERROR_MSG,
			    "Your account has expired; please contact your system administrator");
		D(("account expired"));
		return PAM_ACCT_EXPIRED;
	}
	if ((curdays > (spent->sp_lstchg + spent->sp_max + spent->sp_inact))
	    && (spent->sp_max != -1) && (spent->sp_inact != -1)
	    && (spent->sp_lstchg != 0)) {
		_log_err(LOG_NOTICE, pamh
		    ,"account %s has expired (failed to change password)"
			 ,uname);
		_make_remark(pamh, ctrl, PAM_ERROR_MSG,
			    "Your account has expired; please contact your system administrator");
		D(("account expired 2"));
		return PAM_ACCT_EXPIRED;
	}
	D(("when was the last change"));
	if (spent->sp_lstchg == 0) {
		_log_err(LOG_NOTICE, pamh
			 ,"expired password for user %s (root enforced)"
			 ,uname);
		_make_remark(pamh, ctrl, PAM_ERROR_MSG,
			    "You are required to change your password immediately (root enforced)");
		D(("need a new password"));
		return PAM_NEW_AUTHTOK_REQD;
	}
	if (((spent->sp_lstchg + spent->sp_max) < curdays) && (spent->sp_max != -1)) {
		_log_err(LOG_DEBUG, pamh
			 ,"expired password for user %s (password aged)"
			 ,uname);
		_make_remark(pamh, ctrl, PAM_ERROR_MSG,
			    "You are required to change your password immediately (password aged)");
		D(("need a new password 2"));
		return PAM_NEW_AUTHTOK_REQD;
	}
	if ((curdays > (spent->sp_lstchg + spent->sp_max - spent->sp_warn))
	    && (spent->sp_max != -1) && (spent->sp_warn != -1)) {
		daysleft = (spent->sp_lstchg + spent->sp_max) - curdays;
		_log_err(LOG_DEBUG, pamh
			 ,"password for user %s will expire in %d days"
			 ,uname, daysleft);
		snprintf(buf, 80, "Warning: your password will expire in %d day%.2s",
			 daysleft, daysleft == 1 ? "" : "s");
		_make_remark(pamh, ctrl, PAM_TEXT_INFO, buf);
	}

	D(("all done"));

	return PAM_SUCCESS;
}


/* static module data */
#ifdef PAM_STATIC
struct pam_module _pam_unix_acct_modstruct = {
    "pam_unix_acct",
    NULL,
    NULL,
    pam_sm_acct_mgmt,
    NULL,
    NULL,
    NULL,
};
#endif
