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
 * $P4: //depot/projects/openpam/include/security/pam_modules.h#5 $
 */

#ifndef _PAM_MODULES_H_INCLUDED
#define _PAM_MODULES_H_INCLUDED

#include <security/pam_types.h>
#include <security/pam_constants.h>
#include <security/openpam.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XSSO 4.2.2, 6
 */

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *_pamh,
	int _flags,
	int _argc,
	const char **_argv);

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *_pamh,
	int _flags,
	int _argc,
	const char **_argv);

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *_pamh,
	int _flags,
	int _argc,
	const char **_argv);

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *_pamh,
	int _flags,
	int _args,
	const char **_argv);

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *_pamh,
	int _flags,
	int _argc,
	const char **_argv);

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *_pamh,
	int _flags,
	int _argc,
	const char **_argv);

/*
 * Single Sign-On extensions
 */
#if 0
PAM_EXTERN int
pam_sm_authenticate_secondary(pam_handle_t *_pamh,
	char *_target_username,
	char *_target_module_type,
	char *_target_authn_domain,
	char *_target_supp_data,
	unsigned char *_target_module_authtok,
	int _flags,
	int _argc,
	const char **_argv);

PAM_EXTERN int
pam_sm_get_mapped_authtok(pam_handle_t *_pamh,
	char *_target_module_username,
	char *_target_module_type,
	char *_target_authn_domain,
	size_t *_target_authtok_len,
	unsigned char **_target_module_authtok,
	int _argc,
	char *_argv);

PAM_EXTERN int
pam_sm_get_mapped_username(pam_handle_t *_pamh,
	char *_src_username,
	char *_src_module_type,
	char *_src_authn_domain,
	char *_target_module_type,
	char *_target_authn_domain,
	char **_target_module_username,
	int _argc,
	const char **_argv);

PAM_EXTERN int
pam_sm_set_mapped_authtok(pam_handle_t *_pamh,
	char *_target_module_username,
	size_t _target_authtok_len,
	unsigned char *_target_module_authtok,
	char *_target_module_type,
	char *_target_authn_domain,
	int _argc,
	const char *_argv);

PAM_EXTERN int
pam_sm_set_mapped_username(pam_handle_t *_pamh,
	char *_target_module_username,
	char *_target_module_type,
	char *_target_authn_domain,
	int _argc,
	const char **_argv);

#endif /* 0 */

#ifdef __cplusplus
}
#endif

#endif
