/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
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
 * $P4: //depot/projects/openpam/lib/pam_strerror.c#9 $
 */

#include <stdio.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

/*
 * XSSO 4.2.1
 * XSSO 6 page 92
 *
 * Get PAM standard error message string
 */

const char *
pam_strerror(pam_handle_t *pamh,
	int error_number)
{
	static char unknown[16];

	pamh = pamh;

	switch (error_number) {
	case PAM_SUCCESS:
		return ("success");
	case PAM_OPEN_ERR:
		return ("failed to load module");
	case PAM_SYMBOL_ERR:
		return ("invalid symbol");
	case PAM_SERVICE_ERR:
		return ("error in service module");
	case PAM_SYSTEM_ERR:
		return ("system error");
	case PAM_BUF_ERR:
		return ("memory buffer error");
	case PAM_CONV_ERR:
		return ("conversation failure");
	case PAM_PERM_DENIED:
		return ("permission denied");
	case PAM_MAXTRIES:
		return ("maximum number of tries exceeded");
	case PAM_AUTH_ERR:
		return ("authentication error");
	case PAM_NEW_AUTHTOK_REQD:
		return ("new authentication token required");
	case PAM_CRED_INSUFFICIENT:
		return ("insufficient credentials");
	case PAM_AUTHINFO_UNAVAIL:
		return ("authentication information is unavailable");
	case PAM_USER_UNKNOWN:
		return ("unknown user");
	case PAM_CRED_UNAVAIL:
		return ("failed to retrieve user credentials");
	case PAM_CRED_EXPIRED:
		return ("user credentials have expired");
	case PAM_CRED_ERR:
		return ("failed to set user credentials");
	case PAM_ACCT_EXPIRED:
		return ("user accound has expired");
	case PAM_AUTHTOK_EXPIRED:
		return ("password has expired");
	case PAM_SESSION_ERR:
		return ("session failure");
	case PAM_AUTHTOK_ERR:
		return ("authentication token failure");
	case PAM_AUTHTOK_RECOVERY_ERR:
		return ("failed to recover old authentication token");
	case PAM_AUTHTOK_LOCK_BUSY:
		return ("authentication token lock busy");
	case PAM_AUTHTOK_DISABLE_AGING:
		return ("authentication token aging disabled");
	case PAM_NO_MODULE_DATA:
		return ("module data not found");
	case PAM_IGNORE:
		return ("ignore this module");
	case PAM_ABORT:
		return ("general failure");
	case PAM_TRY_AGAIN:
		return ("try again");
	case PAM_MODULE_UNKNOWN:
		return ("unknown module type");
	case PAM_DOMAIN_UNKNOWN:
		return ("unknown authentication domain");
	default:
		snprintf(unknown, sizeof unknown, "#%d", error_number);
		return (unknown);
	}
}

/**
 * The =pam_strerror function returns a pointer to a string containing a
 * textual description of the error indicated by the =error_number
 * argument, in the context of the PAM transaction described by the =pamh
 * argument.
 */
