/*-
 * Copyright 2001 Mark R V Murray
 * Copyright Frank Cusack fcusack@fcusack.com 1999-2000
 * All rights reserved
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
 * ---------------------------------------------------------------------------
 * 
 * This software may contain code from Naomaru Itoi:
 * 
 * PAM-kerberos5 module Copyright notice.
 * Naomaru Itoi <itoi@eecs.umich.edu>, June 24, 1997.
 * 
 * ----------------------------------------------------------------------------
 * COPYRIGHT (c)  1997
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 * 
 * PERMISSION IS GRANTED TO USE, COPY, CREATE DERIVATIVE WORKS AND REDISTRIBUTE
 * THIS SOFTWARE AND SUCH DERIVATIVE WORKS FOR ANY PURPOSE, SO LONG AS THE NAME
 * OF THE UNIVERSITY OF MICHIGAN IS NOT USED IN ANY ADVERTISING OR PUBLICITY
 * PERTAINING TO THE USE OR DISTRIBUTION OF THIS SOFTWARE WITHOUT SPECIFIC,
 * WRITTEN PRIOR AUTHORIZATION.  IF THE ABOVE COPYRIGHT NOTICE OR ANY OTHER
 * IDENTIFICATION OF THE UNIVERSITY OF MICHIGAN IS INCLUDED IN ANY COPY OF ANY
 * PORTION OF THIS SOFTWARE, THEN THE DISCLAIMER BELOW MUST ALSO BE INCLUDED.
 * 
 * THE SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE UNIVERSITY OF
 * MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND WITHOUT WARRANTY BY THE
 * UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF MERCHANTABITILY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OF THE SOFTWARE, EVEN IF IT HAS BEEN OR IS HEREAFTER
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * PAM-kerberos5 module is written based on PAM-kerberos4 module
 * by Derrick J. Brashear and kerberos5-1.0pl1 by M.I.T. kerberos team.
 * Permission to use, copy, modify, distribute this software is hereby
 * granted, as long as it is granted by Derrick J. Brashear and
 * M.I.T. kerberos team. Followings are their copyright information.  
 * ----------------------------------------------------------------------------
 * 
 * This software may contain code from Derrick J. Brashear:
 * 
 * 
 * Copyright (c) Derrick J. Brashear, 1996. All rights reserved
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
 * 
 * ----------------------------------------------------------------------------
 * 
 * This software may contain code from MIT Kerberos 5:
 * 
 * Copyright Notice and Legal Administrivia
 * ----------------------------------------
 * 
 * Copyright (C) 1996 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without
 * prior written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the
 * MIT trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 * 
 * The following copyright and permission notice applies to the
 * OpenVision Kerberos Administration system located in kadmin/create,
 * kadmin/dbutil, kadmin/passwd, kadmin/server, lib/kadm5, and portions
 * of lib/rpc:
 * 
 *    Copyright, OpenVision Technologies, Inc., 1996, All Rights Reserved
 * 
 *    WARNING: Retrieving the OpenVision Kerberos Administration system 
 *    source code, as described below, indicates your acceptance of the 
 *    following terms.  If you do not agree to the following terms, do not 
 *    retrieve the OpenVision Kerberos administration system.
 * 
 *    You may freely use and distribute the Source Code and Object Code
 *    compiled from it, with or without modification, but this Source
 *    Code is provided to you "AS IS" EXCLUSIVE OF ANY WARRANTY,
 *    INCLUDING, WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY OR
 *    FITNESS FOR A PARTICULAR PURPOSE, OR ANY OTHER WARRANTY, WHETHER
 *    EXPRESS OR IMPLIED.  IN NO EVENT WILL OPENVISION HAVE ANY LIABILITY
 *    FOR ANY LOST PROFITS, LOSS OF DATA OR COSTS OF PROCUREMENT OF 
 *    SUBSTITUTE GOODS OR SERVICES, OR FOR ANY SPECIAL, INDIRECT, OR
 *    CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, INCLUDING, 
 *    WITHOUT LIMITATION, THOSE RESULTING FROM THE USE OF THE SOURCE 
 *    CODE, OR THE FAILURE OF THE SOURCE CODE TO PERFORM, OR FOR ANY 
 *    OTHER REASON.
 * 
 *    OpenVision retains all copyrights in the donated Source Code. OpenVision
 *    also retains copyright to derivative works of the Source Code, whether
 *    created by OpenVision or by a third party. The OpenVision copyright 
 *    notice must be preserved if derivative works are made based on the 
 *    donated Source Code.
 * 
 *    OpenVision Technologies, Inc. has donated this Kerberos 
 *    Administration system to MIT for inclusion in the standard 
 *    Kerberos 5 distribution.  This donation underscores our 
 *    commitment to continuing Kerberos technology development 
 *    and our gratitude for the valuable work which has been 
 *    performed by MIT and the Kerberos community.
 * 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <krb5.h>
#include <com_err.h>

#define	PAM_SM_AUTH
#define	PAM_SM_ACCOUNT
#define	PAM_SM_SESSION
#define	PAM_SM_PASSWORD

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include "pam_mod_misc.h"

#define	COMPAT_HEIMDAL
/* #define	COMPAT_MIT */

extern	krb5_cc_ops	krb5_mcc_ops;

static int	verify_krb_v5_tgt(krb5_context, krb5_ccache, char *, int);
static void	cleanup_cache(pam_handle_t *, void *, int);
static const	char *compat_princ_component(krb5_context, krb5_principal, int);
static void	compat_free_data_contents(krb5_context, krb5_data *);

#define USER_PROMPT		"Username: "
#define PASSWORD_PROMPT		"Password:"
#define NEW_PASSWORD_PROMPT	"New Password:"
#define NEW_PASSWORD_PROMPT_2	"New Password (again):"

enum { PAM_OPT_AUTH_AS_SELF=PAM_OPT_STD_MAX, PAM_OPT_CCACHE, PAM_OPT_FORWARDABLE, PAM_OPT_NO_CCACHE, PAM_OPT_REUSE_CCACHE };

static struct opttab other_options[] = {
	{ "auth_as_self",	PAM_OPT_AUTH_AS_SELF },
	{ "ccache",		PAM_OPT_CCACHE },
	{ "forwardable",	PAM_OPT_FORWARDABLE },
	{ "no_ccache",		PAM_OPT_NO_CCACHE },
	{ "reuse_ccache",	PAM_OPT_REUSE_CCACHE },
	{ NULL, 0 }
};

/*
 * authentication management
 */
PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags __unused, int argc, const char **argv)
{
	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_creds creds;
	krb5_principal princ;
	krb5_ccache ccache, ccache_check;
	krb5_get_init_creds_opt opts;
	struct options options;
	struct passwd *pwd;
	int retval;
	const char *sourceuser, *user, *pass, *service;
	char *principal, *princ_name, *cache_name, luser[32];	

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_user(pamh, &user, USER_PROMPT);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got user: %s", user);

	retval = pam_get_item(pamh, PAM_RUSER, (const void **)&sourceuser);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got ruser: %s", sourceuser);

	service = NULL;
	pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
	if (service == NULL)
		service = "unknown";

	PAM_LOG("Got service: %s", service);

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_RETURN(PAM_SERVICE_ERR);
	}

	PAM_LOG("Context initialised");

	krb5_get_init_creds_opt_init(&opts);

	if (pam_test_option(&options, PAM_OPT_FORWARDABLE, NULL))
		krb5_get_init_creds_opt_set_forwardable(&opts, 1);

	PAM_LOG("Credentials initialised");

	krbret = krb5_cc_register(pam_context, &krb5_mcc_ops, FALSE);
	if (krbret != 0 && krbret != KRB5_CC_TYPE_EXISTS) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	PAM_LOG("Done krb5_cc_register()");

	/* Get principal name */
	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL))
		asprintf(&principal, "%s/%s", sourceuser, user);
	else
		principal = strdup(user);

	PAM_LOG("Created principal: %s", principal);

	krbret = krb5_parse_name(pam_context, principal, &princ);
	free(principal);
	if (krbret != 0) {
		PAM_LOG("Error krb5_parse_name(): %s", error_message(krbret));
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	PAM_LOG("Done krb5_parse_name()");

	/* Now convert the principal name into something human readable */
	princ_name = NULL;
	krbret = krb5_unparse_name(pam_context, princ, &princ_name);
	if (krbret != 0) {
		PAM_LOG("Error krb5_unparse_name(): %s", error_message(krbret));
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Got principal: %s", princ_name);

	/* Get password */
	retval = pam_get_pass(pamh, &pass, PASSWORD_PROMPT, &options);
	if (retval != PAM_SUCCESS)
		goto cleanup2;

	PAM_LOG("Got password");

	/* Verify the local user exists (AFTER getting the password) */
	if (strchr(user, '@')) {
		/* get a local account name for this principal */
		krbret = krb5_aname_to_localname(pam_context, princ,
		    sizeof(luser), luser);
		if (krbret != 0) {
			PAM_VERBOSE_ERROR("Kerberos 5 error");
			PAM_LOG("Error krb5_aname_to_localname(): %s",
			    error_message(krbret));
			retval = PAM_USER_UNKNOWN;
			goto cleanup2;
		}

		retval = pam_set_item(pamh, PAM_USER, luser);
		if (retval != PAM_SUCCESS)
			goto cleanup2;

		retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
		if (retval != PAM_SUCCESS)
			goto cleanup2;

		PAM_LOG("PAM_USER Redone");
	}

	pwd = getpwnam(user);
	if (pwd == NULL) {
		retval = PAM_USER_UNKNOWN;
		goto cleanup2;
	}

	PAM_LOG("Done getpwnam()");

	/* Get a TGT */
	memset(&creds, 0, sizeof(krb5_creds));
	krbret = krb5_get_init_creds_password(pam_context, &creds, princ,
	    pass, NULL, pamh, 0, NULL, &opts);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_LOG("Error krb5_get_init_creds_password(): %s",
		    error_message(krbret));
		retval = PAM_AUTH_ERR;
		goto cleanup2;
	}

	PAM_LOG("Got TGT");

	/* Generate a unique cache_name */
	asprintf(&cache_name, "MEMORY:/tmp/%s.%d", service, getpid());
	krbret = krb5_cc_resolve(pam_context, cache_name, &ccache);
	free(cache_name);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_LOG("Error krb5_cc_resolve(): %s", error_message(krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}
	krbret = krb5_cc_initialize(pam_context, ccache, princ);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_LOG("Error krb5_cc_initialize(): %s", error_message(krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}
	krbret = krb5_cc_store_cred(pam_context, ccache, &creds);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_LOG("Error krb5_cc_store_cred(): %s", error_message(krbret));
		krb5_cc_destroy(pam_context, ccache);
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	PAM_LOG("Credentials stashed");

	/* Verify them */
	if (verify_krb_v5_tgt(pam_context, ccache, service,
	    pam_test_option(&options, PAM_OPT_FORWARDABLE, NULL)) == -1) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		krb5_cc_destroy(pam_context, ccache);
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

	PAM_LOG("Credentials stash verified");

	retval = pam_get_data(pamh, "ccache", (const void **)&ccache_check);
	if (retval == PAM_SUCCESS) {
		krb5_cc_destroy(pam_context, ccache);
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

	PAM_LOG("Credentials stash not pre-existing");

	retval = pam_set_data(pamh, "ccache", ccache, cleanup_cache);
	if (retval != 0) {
		krb5_cc_destroy(pam_context, ccache);
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	PAM_LOG("Credentials stash saved");

cleanup:
	krb5_free_cred_contents(pam_context, &creds);
	PAM_LOG("Done cleanup");
cleanup2:
	krb5_free_principal(pam_context, princ);
	PAM_LOG("Done cleanup2");
cleanup3:
	if (princ_name)
		free(princ_name);

	krb5_free_context(pam_context);

	PAM_LOG("Done cleanup3");

	if (retval != PAM_SUCCESS)
		PAM_VERBOSE_ERROR("Kerberos 5 refuses you");

	PAM_RETURN(retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{

	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_principal princ;
	krb5_creds creds;
	krb5_ccache ccache_temp, ccache_perm;
	krb5_cc_cursor cursor;
	struct options options;
	struct passwd *pwd = NULL;
	int retval;
	char *user;
	char *cache_name, *cache_env_name, *p, *q;

	uid_t euid;
	gid_t egid;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	if (flags & PAM_DELETE_CRED)
		PAM_RETURN(PAM_SUCCESS);

	if (flags & PAM_REFRESH_CRED)
		PAM_RETURN(PAM_SUCCESS);

	if (flags & PAM_REINITIALIZE_CRED)
		PAM_RETURN(PAM_SUCCESS);

	if (!(flags & PAM_ESTABLISH_CRED))
		PAM_RETURN(PAM_SERVICE_ERR);

	PAM_LOG("Establishing credentials");

	/* Get username */
	retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got user: %s", user);

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_LOG("Error krb5_init_context(): %s", error_message(krbret));
		PAM_RETURN(PAM_SERVICE_ERR);
	}

	PAM_LOG("Context initialised");

	euid = geteuid();	/* Usually 0 */
	egid = getegid();

	PAM_LOG("Got euid, egid: %d %d", euid, egid);

	/* Retrieve the cache name */
	retval = pam_get_data(pamh, "ccache", (const void **)&ccache_temp);
	if (retval != PAM_SUCCESS)
		goto cleanup3;

	/* Get the uid. This should exist. */
	pwd = getpwnam(user);
	if (pwd == NULL) {
		retval = PAM_USER_UNKNOWN;
		goto cleanup3;
	}

	PAM_LOG("Done getpwnam()");

	/* Avoid following a symlink as root */
	if (setegid(pwd->pw_gid)) {
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}
	if (seteuid(pwd->pw_uid)) {
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	PAM_LOG("Done setegid() & seteuid()");

	/* Get the cache name */
	cache_name = NULL;
	pam_test_option(&options, PAM_OPT_CCACHE, &cache_name);
	if (cache_name == NULL)
		asprintf(&cache_name, "FILE:/tmp/krb5cc_%d", pwd->pw_uid);

	p = calloc(PATH_MAX + 16, sizeof(char));
	q = cache_name;

	if (p == NULL) {
		PAM_LOG("Error malloc(): failure");
		retval = PAM_BUF_ERR;
		goto cleanup3;
	}
	cache_name = p;

	/* convert %u and %p */
	while (*q) {
		if (*q == '%') {
			q++;
			if (*q == 'u') {
				sprintf(p, "%d", pwd->pw_uid);
				p += strlen(p);
			}
			else if (*q == 'p') {
				sprintf(p, "%d", getpid());
				p += strlen(p);
			}
			else {
				/* Not a special token */
				*p++ = '%';
				q--;
			}
			q++;
		}
		else {
			*p++ = *q++;
		}
	}

	PAM_LOG("Got cache_name: %s", cache_name);

	/* Initialize the new ccache */
	krbret = krb5_cc_get_principal(pam_context, ccache_temp, &princ);
	if (krbret != 0) {
		PAM_LOG("Error krb5_cc_get_principal(): %s",
		    error_message(krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}
	krbret = krb5_cc_resolve(pam_context, cache_name, &ccache_perm);
	if (krbret != 0) {
		PAM_LOG("Error krb5_cc_resolve(): %s", error_message(krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}
	krbret = krb5_cc_initialize(pam_context, ccache_perm, princ);
	if (krbret != 0) {
		PAM_LOG("Error krb5_cc_initialize(): %s", error_message(krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Cache initialised");

	/* Prepare for iteration over creds */
	krbret = krb5_cc_start_seq_get(pam_context, ccache_temp, &cursor);
	if (krbret != 0) {
		PAM_LOG("Error krb5_cc_start_seq_get(): %s", error_message(krbret));
		krb5_cc_destroy(pam_context, ccache_perm);
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Prepared for iteration");

	/* Copy the creds (should be two of them) */
	while ((krbret = krb5_cc_next_cred(pam_context, ccache_temp,
				&cursor, &creds) == 0)) {
		krbret = krb5_cc_store_cred(pam_context, ccache_perm, &creds);
		if (krbret != 0) {
			PAM_LOG("Error krb5_cc_store_cred(): %s",
			    error_message(krbret));
			krb5_cc_destroy(pam_context, ccache_perm);
			krb5_free_cred_contents(pam_context, &creds);
			retval = PAM_SERVICE_ERR;
			goto cleanup2;
		}
		krb5_free_cred_contents(pam_context, &creds);
		PAM_LOG("Iteration");
	}
	krb5_cc_end_seq_get(pam_context, ccache_temp, &cursor);

	PAM_LOG("Done iterating");

	if (strstr(cache_name, "FILE:") == cache_name) {
		if (chown(&cache_name[5], pwd->pw_uid, pwd->pw_gid) == -1) {
			PAM_LOG("Error chown(): %s", strerror(errno));
			krb5_cc_destroy(pam_context, ccache_perm);
			retval = PAM_SERVICE_ERR;
			goto cleanup2;
		}
		PAM_LOG("Done chown()");

		if (chmod(&cache_name[5], (S_IRUSR | S_IWUSR)) == -1) {
			PAM_LOG("Error chmod(): %s", strerror(errno));
			krb5_cc_destroy(pam_context, ccache_perm);
			retval = PAM_SERVICE_ERR;
			goto cleanup2;
		}
		PAM_LOG("Done chmod()");
	}

	krb5_cc_close(pam_context, ccache_perm);

	PAM_LOG("Cache closed");

	cache_env_name = malloc(strlen(cache_name) + 12);
	if (!cache_env_name) {
		PAM_LOG("Error malloc(): failure");
		krb5_cc_destroy(pam_context, ccache_perm);
		retval = PAM_BUF_ERR;
		goto cleanup2;
	}

	sprintf(cache_env_name, "KRB5CCNAME=%s", cache_name);
	if ((retval = pam_putenv(pamh, cache_env_name)) != 0) {
		PAM_LOG("Error pam_putenv(): %s", pam_strerror(pamh, retval));
		krb5_cc_destroy(pam_context, ccache_perm);
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Environment done: KRB5CCNAME=%s", cache_name);

cleanup2:
	krb5_free_principal(pam_context, princ);
	PAM_LOG("Done cleanup2");
cleanup3:
	krb5_free_context(pam_context);
	PAM_LOG("Done cleanup3");

	seteuid(euid);
	setegid(egid);

	PAM_LOG("Done seteuid() & setegid()");
	
	PAM_RETURN(retval);
}

/* 
 * account management
 */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags __unused, int argc, const char **argv)
{
	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_ccache ccache;
	krb5_principal princ;
	struct options options;
	int retval;
	const char *user;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got user: %s", user);

	retval = pam_get_data(pamh, "ccache", (const void **)&ccache);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(PAM_SUCCESS);

	PAM_LOG("Got ccache");

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_LOG("Error krb5_init_context(): %s", error_message(krbret));
		PAM_RETURN(PAM_PERM_DENIED);
	}

	PAM_LOG("Context initialised");

	krbret = krb5_cc_get_principal(pam_context, ccache, &princ);
	if (krbret != 0) {
		PAM_LOG("Error krb5_cc_get_principal(): %s", error_message(krbret));
		retval = PAM_PERM_DENIED;;
		goto cleanup;
	}

	PAM_LOG("Got principal");

	if (krb5_kuserok(pam_context, princ, user))
		retval = PAM_SUCCESS;
	else
		retval = PAM_PERM_DENIED;
	krb5_free_principal(pam_context, princ);

	PAM_LOG("Done kuserok()");

cleanup:
	krb5_free_context(pam_context);
	PAM_LOG("Done cleanup");

	PAM_RETURN(retval);

}

/* 
 * session management
 *
 * logging only
 */
PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh __unused, int flags __unused, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

/* 
 * password management
 */
PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_creds creds;
	krb5_principal princ;
	krb5_get_init_creds_opt opts;
	krb5_data result_code_string, result_string;
	struct options options;
	int result_code, retval;
	const char *user, *pass, *pass2;
	char *princ_name;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	if (!(flags & PAM_UPDATE_AUTHTOK))
		PAM_RETURN(PAM_AUTHTOK_ERR);

	retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got user: %s", user);

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_LOG("Error krb5_init_context(): %s", error_message(krbret));
		PAM_RETURN(PAM_SERVICE_ERR);
	}

	PAM_LOG("Context initialised");

	krb5_get_init_creds_opt_init(&opts);

	PAM_LOG("Credentials options initialised");

	/* Get principal name */
	krbret = krb5_parse_name(pam_context, user, &princ);
	if (krbret != 0) {
		PAM_LOG("Error krb5_parse_name(): %s", error_message(krbret));
		retval = PAM_USER_UNKNOWN;
		goto cleanup3;
	}

	/* Now convert the principal name into something human readable */
	princ_name = NULL;
	krbret = krb5_unparse_name(pam_context, princ, &princ_name);
	if (krbret != 0) {
		PAM_LOG("Error krb5_unparse_name(): %s", error_message(krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Got principal: %s", princ_name);

	/* Get password */
	retval = pam_get_pass(pamh, &pass, PASSWORD_PROMPT, &options);
	if (retval != PAM_SUCCESS)
		goto cleanup2;

	PAM_LOG("Got password");

	memset(&creds, 0, sizeof(krb5_creds));
	krbret = krb5_get_init_creds_password(pam_context, &creds, princ,
	    pass, NULL, pamh, 0, "kadmin/changepw", &opts);
	if (krbret != 0) {
		PAM_LOG("Error krb5_get_init_creds_password()",
		    error_message(krbret));
		retval = PAM_AUTH_ERR;
		goto cleanup2;
	}

	PAM_LOG("Credentials established");

	/* Now get the new password */
	retval = pam_get_pass(pamh, &pass, NEW_PASSWORD_PROMPT, &options);
	if (retval != PAM_SUCCESS)
		goto cleanup;

	retval = pam_get_pass(pamh, &pass2, NEW_PASSWORD_PROMPT_2, &options);
	if (retval != PAM_SUCCESS)
		goto cleanup;

	PAM_LOG("Got new password twice");

	if (strcmp(pass, pass2) != 0) {
		PAM_LOG("Error strcmp(): passwords are different");
		retval = PAM_AUTHTOK_ERR;
		goto cleanup;
	}

	PAM_LOG("New passwords are the same");

	/* Change it */
	krbret = krb5_change_password(pam_context, &creds, pass,
	    &result_code, &result_code_string, &result_string);
	if (krbret != 0) {
		PAM_LOG("Error krb5_change_password(): %s",
		    error_message(krbret));
		retval = PAM_AUTHTOK_ERR;
		goto cleanup;
	}
	if (result_code) {
		PAM_LOG("Error krb5_change_password(): (result_code)");
		retval = PAM_AUTHTOK_ERR;
		goto cleanup;
	}

	PAM_LOG("Password changed");

	if (result_string.data)
		free(result_string.data);
	if (result_code_string.data)
		free(result_code_string.data);

cleanup:
	krb5_free_cred_contents(pam_context, &creds);
	PAM_LOG("Done cleanup");
cleanup2:
	krb5_free_principal(pam_context, princ);
	PAM_LOG("Done cleanup2");
cleanup3:
	if (princ_name)
		free(princ_name);

	krb5_free_context(pam_context);

	PAM_LOG("Done cleanup3");

	PAM_RETURN(retval);
}

PAM_MODULE_ENTRY("pam_krb5");

/*
 * This routine with some modification is from the MIT V5B6 appl/bsd/login.c
 * Modified by Sam Hartman <hartmans@mit.edu> to support PAM services
 * for Debian.
 *
 * Verify the Kerberos ticket-granting ticket just retrieved for the
 * user.  If the Kerberos server doesn't respond, assume the user is
 * trying to fake us out (since we DID just get a TGT from what is
 * supposedly our KDC).  If the host/<host> service is unknown (i.e.,
 * the local keytab doesn't have it), and we cannot find another
 * service we do have, let her in.
 *
 * Returns 1 for confirmation, -1 for failure, 0 for uncertainty.
 */
static int
verify_krb_v5_tgt(krb5_context context, krb5_ccache ccache,
    char *pam_service, int debug)
{
	krb5_error_code retval;
	krb5_principal princ;
	krb5_keyblock *keyblock;
	krb5_data packet;
	krb5_auth_context auth_context;
	char phost[BUFSIZ];
	const char *services[3], **service;

	packet.data = 0;

	/* If possible we want to try and verify the ticket we have
	 * received against a keytab.  We will try multiple service
	 * principals, including at least the host principal and the PAM
	 * service principal.  The host principal is preferred because access
	 * to that key is generally sufficient to compromise root, while the
	 * service key for this PAM service may be less carefully guarded.
	 * It is important to check the keytab first before the KDC so we do
	 * not get spoofed by a fake KDC.
	 */
	services[0] = "host";
	services[1] = pam_service;
	services[2] = NULL;
	keyblock = 0;
	retval = -1;
	for (service = &services[0]; *service != NULL; service++) {
		retval = krb5_sname_to_principal(context, NULL, *service,
		    KRB5_NT_SRV_HST, &princ);
		if (retval != 0) {
			if (debug)
				syslog(LOG_DEBUG, "pam_krb5: verify_krb_v5_tgt(): %s: %s", "krb5_sname_to_principal()", error_message(retval));
			return -1;
		}

		/* Extract the name directly. */
		strncpy(phost, compat_princ_component(context, princ, 1),
		    BUFSIZ);
		phost[BUFSIZ - 1] = '\0';

		/*
	         * Do we have service/<host> keys?
	         * (use default/configured keytab, kvno IGNORE_VNO to get the
	         * first match, and ignore enctype.)
	         */
		retval = krb5_kt_read_service_key(context, NULL, princ, 0, 0,
		    &keyblock);
		if (retval != 0)
			continue;
		break;
	}
	if (retval != 0) {	/* failed to find key */
		/* Keytab or service key does not exist */
		if (debug)
			syslog(LOG_DEBUG, "pam_krb5: verify_krb_v5_tgt(): %s: %s", "krb5_kt_read_service_key()", error_message(retval));
		retval = 0;
		goto cleanup;
	}
	if (keyblock)
		krb5_free_keyblock(context, keyblock);

	/* Talk to the kdc and construct the ticket. */
	auth_context = NULL;
	retval = krb5_mk_req(context, &auth_context, 0, *service, phost,
		NULL, ccache, &packet);
	if (auth_context) {
		krb5_auth_con_free(context, auth_context);
		auth_context = NULL;	/* setup for rd_req */
	}
	if (retval) {
		if (debug)
			syslog(LOG_DEBUG, "pam_krb5: verify_krb_v5_tgt(): %s: %s", "krb5_mk_req()", error_message(retval));
		retval = -1;
		goto cleanup;
	}

	/* Try to use the ticket. */
	retval = krb5_rd_req(context, &auth_context, &packet, princ, NULL,
	    NULL, NULL);
	if (retval) {
		if (debug)
			syslog(LOG_DEBUG, "pam_krb5: verify_krb_v5_tgt(): %s: %s", "krb5_rd_req()", error_message(retval));
		retval = -1;
	}
	else
		retval = 1;

cleanup:
	if (packet.data)
		compat_free_data_contents(context, &packet);
	krb5_free_principal(context, princ);
	return retval;
}

/* Free the memory for cache_name. Called by pam_end() */
static void
cleanup_cache(pam_handle_t *pamh __unused, void *data, int pam_end_status __unused)
{
	krb5_context pam_context;
	krb5_ccache ccache;

	if (krb5_init_context(&pam_context))
		return;

	ccache = (krb5_ccache)data;
	krb5_cc_destroy(pam_context, ccache);
	krb5_free_context(pam_context);
}

#ifdef COMPAT_HEIMDAL
#ifdef COMPAT_MIT
#error This cannot be MIT and Heimdal compatible!
#endif
#endif

#ifndef COMPAT_HEIMDAL
#ifndef COMPAT_MIT
#error One of COMPAT_MIT and COMPAT_HEIMDAL must be specified!
#endif
#endif

#ifdef COMPAT_HEIMDAL
static const char *
compat_princ_component(krb5_context context __unused, krb5_principal princ, int n)
{
	return princ->name.name_string.val[n];
}

static void
compat_free_data_contents(krb5_context context __unused, krb5_data * data)
{
	krb5_xfree(data->data);
}
#endif

#ifdef COMPAT_MIT
static const char *
compat_princ_component(krb5_context context, krb5_principal princ, int n)
{
	return krb5_princ_component(context, princ, n)->data;
}

static void
compat_free_data_contents(krb5_context context, krb5_data * data)
{
	krb5_free_data_contents(context, data);
}
#endif
