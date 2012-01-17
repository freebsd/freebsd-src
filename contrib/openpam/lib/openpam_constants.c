/*-
 * Copyright (c) 2001-2003 Networks Associates Technology, Inc.
 * Copyright (c) 2004-2011 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * Network Associates Laboratories, the Security Research Division of
 * Network Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
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
 *
 * $Id: openpam_constants.c 491 2011-11-12 00:12:32Z des $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <security/pam_appl.h>

#include "openpam_impl.h"

const char *pam_err_name[PAM_NUM_ERRORS] = {
	"PAM_SUCCESS",
	"PAM_OPEN_ERR",
	"PAM_SYMBOL_ERR",
	"PAM_SERVICE_ERR",
	"PAM_SYSTEM_ERR",
	"PAM_BUF_ERR",
	"PAM_CONV_ERR",
	"PAM_PERM_DENIED",
	"PAM_MAXTRIES",
	"PAM_AUTH_ERR",
	"PAM_NEW_AUTHTOK_REQD",
	"PAM_CRED_INSUFFICIENT",
	"PAM_AUTHINFO_UNAVAIL",
	"PAM_USER_UNKNOWN",
	"PAM_CRED_UNAVAIL",
	"PAM_CRED_EXPIRED",
	"PAM_CRED_ERR",
	"PAM_ACCT_EXPIRED",
	"PAM_AUTHTOK_EXPIRED",
	"PAM_SESSION_ERR",
	"PAM_AUTHTOK_ERR",
	"PAM_AUTHTOK_RECOVERY_ERR",
	"PAM_AUTHTOK_LOCK_BUSY",
	"PAM_AUTHTOK_DISABLE_AGING",
	"PAM_NO_MODULE_DATA",
	"PAM_IGNORE",
	"PAM_ABORT",
	"PAM_TRY_AGAIN",
	"PAM_MODULE_UNKNOWN",
	"PAM_DOMAIN_UNKNOWN"
};

const char *pam_item_name[PAM_NUM_ITEMS] = {
	"(NO ITEM)",
	"PAM_SERVICE",
	"PAM_USER",
	"PAM_TTY",
	"PAM_RHOST",
	"PAM_CONV",
	"PAM_AUTHTOK",
	"PAM_OLDAUTHTOK",
	"PAM_RUSER",
	"PAM_USER_PROMPT",
	"PAM_REPOSITORY",
	"PAM_AUTHTOK_PROMPT",
	"PAM_OLDAUTHTOK_PROMPT",
	"PAM_HOST",
};

const char *pam_facility_name[PAM_NUM_FACILITIES] = {
	[PAM_ACCOUNT]		= "account",
	[PAM_AUTH]		= "auth",
	[PAM_PASSWORD]		= "password",
	[PAM_SESSION]		= "session",
};

const char *pam_control_flag_name[PAM_NUM_CONTROL_FLAGS] = {
	[PAM_BINDING]		= "binding",
	[PAM_OPTIONAL]		= "optional",
	[PAM_REQUIRED]		= "required",
	[PAM_REQUISITE]		= "requisite",
	[PAM_SUFFICIENT]	= "sufficient",
};

const char *pam_func_name[PAM_NUM_PRIMITIVES] = {
	"pam_authenticate",
	"pam_setcred",
	"pam_acct_mgmt",
	"pam_open_session",
	"pam_close_session",
	"pam_chauthtok"
};

const char *pam_sm_func_name[PAM_NUM_PRIMITIVES] = {
	"pam_sm_authenticate",
	"pam_sm_setcred",
	"pam_sm_acct_mgmt",
	"pam_sm_open_session",
	"pam_sm_close_session",
	"pam_sm_chauthtok"
};
