/*-
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
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
 * $P4: //depot/projects/openpam/include/security/pam_constants.h#10 $
 */

#ifndef _PAM_CONSTANTS_H_INCLUDED
#define _PAM_CONSTANTS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XSSO 5.2
 */
enum {
	PAM_SUCCESS			=   0,
	PAM_OPEN_ERR			=   1,
	PAM_SYMBOL_ERR			=   2,
	PAM_SERVICE_ERR			=   3,
	PAM_SYSTEM_ERR			=   4,
	PAM_BUF_ERR			=   5,
	PAM_CONV_ERR			=   6,
	PAM_PERM_DENIED			=   7,
	PAM_MAXTRIES			=   8,
	PAM_AUTH_ERR			=   9,
	PAM_NEW_AUTHTOK_REQD		=  10,
	PAM_CRED_INSUFFICIENT		=  11,
	PAM_AUTHINFO_UNAVAIL		=  12,
	PAM_USER_UNKNOWN		=  13,
	PAM_CRED_UNAVAIL		=  14,
	PAM_CRED_EXPIRED		=  15,
	PAM_CRED_ERR			=  16,
	PAM_ACCT_EXPIRED		=  17,
	PAM_AUTHTOK_EXPIRED		=  18,
	PAM_SESSION_ERR			=  19,
	PAM_AUTHTOK_ERR			=  20,
	PAM_AUTHTOK_RECOVERY_ERR	=  21,
	PAM_AUTHTOK_LOCK_BUSY		=  22,
	PAM_AUTHTOK_DISABLE_AGING	=  23,
	PAM_NO_MODULE_DATA		=  24,
	PAM_IGNORE			=  25,
	PAM_ABORT			=  26,
	PAM_TRY_AGAIN			=  27,
	PAM_MODULE_UNKNOWN		=  28,
	PAM_DOMAIN_UNKNOWN		=  29
};

/*
 * XSSO 5.3
 */
enum {
	PAM_PROMPT_ECHO_OFF		=   1,
	PAM_PROMPT_ECHO_ON		=   2,
	PAM_ERROR_MSG			=   3,
	PAM_TEXT_INFO			=   4,
	PAM_MAX_NUM_MSG			=  32,
	PAM_MAX_MSG_SIZE		= 512,
	PAM_MAX_RESP_SIZE		= 512
};

/*
 * XSSO 5.4
 */
enum {
	PAM_SILENT			= 0x80000000,
	PAM_DISALLOW_NULL_AUTHTOK	= 0x1,
	PAM_ESTABLISH_CRED		= 0x1,
	PAM_DELETE_CRED			= 0x2,
	PAM_REINITIALISE_CRED		= 0x4,
	PAM_REFRESH_CRED		= 0x8,
	PAM_PRELIM_CHECK		= 0x1,
	PAM_UPDATE_AUTHTOK		= 0x2,
	PAM_CHANGE_EXPIRED_AUTHTOK	= 0x4
};

/*
 * XSSO 5.5
 */
enum {
	PAM_SERVICE			=   1,
	PAM_USER			=   2,
	PAM_TTY				=   3,
	PAM_RHOST			=   4,
	PAM_CONV			=   5,
	PAM_AUTHTOK			=   6,
	PAM_OLDAUTHTOK			=   7,
	PAM_RUSER			=   8,
	PAM_USER_PROMPT			=   9,
	PAM_AUTHTOK_PROMPT		=  10,		/* OpenPAM extension */
	PAM_NUM_ITEMS					/* OpenPAM extension */
};

#ifdef __cplusplus
}
#endif

#endif
