/*
 *    Kerberos v5 authentication and ticket-passing routines.
 *
 * $xFreeBSD: src/crypto/openssh/auth-krb5.c,v 1.6 2001/02/13 16:58:04 assar Exp$
 */
/*
 * Copyright (c) 2002 Daniel Kouril.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: auth-krb5.c,v 1.15 2003/11/21 11:57:02 djm Exp $");
RCSID("$FreeBSD$");

#include "ssh.h"
#include "ssh1.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "servconf.h"
#include "uidswap.h"
#include "auth.h"

#ifdef KRB5
#include <krb5.h>

extern ServerOptions	 options;

static int
krb5_init(void *context)
{
	Authctxt *authctxt = (Authctxt *)context;
	krb5_error_code problem;

	if (authctxt->krb5_ctx == NULL) {
		problem = krb5_init_context(&authctxt->krb5_ctx);
		if (problem)
			return (problem);
#ifdef KRB5_INIT_ETS
		krb5_init_ets(authctxt->krb5_ctx);
#endif
	}
	return (0);
}

int
auth_krb5_password(Authctxt *authctxt, const char *password)
{
#ifndef HEIMDAL
	krb5_creds creds;
	krb5_principal server;
	char ccname[40];
	int tmpfd;
#endif
	krb5_error_code problem;
	krb5_ccache ccache = NULL;
	int len;

	if (!authctxt->valid)
		return (0);

	temporarily_use_uid(authctxt->pw);

	problem = krb5_init(authctxt);
	if (problem)
		goto out;

	problem = krb5_parse_name(authctxt->krb5_ctx, authctxt->pw->pw_name,
		    &authctxt->krb5_user);
	if (problem)
		goto out;

#ifdef HEIMDAL
	problem = krb5_cc_gen_new(authctxt->krb5_ctx, &krb5_mcc_ops, &ccache);
	if (problem)
		goto out;

	problem = krb5_cc_initialize(authctxt->krb5_ctx, ccache,
		authctxt->krb5_user);
	if (problem)
		goto out;

	restore_uid();

	problem = krb5_verify_user(authctxt->krb5_ctx, authctxt->krb5_user,
	    ccache, password, 1, NULL);

	temporarily_use_uid(authctxt->pw);

	if (problem)
		goto out;

	problem = krb5_cc_gen_new(authctxt->krb5_ctx, &krb5_fcc_ops,
	    &authctxt->krb5_fwd_ccache);
	if (problem)
		goto out;

	problem = krb5_cc_copy_cache(authctxt->krb5_ctx, ccache,
	    authctxt->krb5_fwd_ccache);
	krb5_cc_destroy(authctxt->krb5_ctx, ccache);
	ccache = NULL;
	if (problem)
		goto out;

#else
	problem = krb5_get_init_creds_password(authctxt->krb5_ctx, &creds,
	    authctxt->krb5_user, (char *)password, NULL, NULL, 0, NULL, NULL);
	if (problem)
		goto out;

	problem = krb5_sname_to_principal(authctxt->krb5_ctx, NULL, NULL,
	    KRB5_NT_SRV_HST, &server);
	if (problem)
		goto out;

	restore_uid();
	problem = krb5_verify_init_creds(authctxt->krb5_ctx, &creds, server,
	    NULL, NULL, NULL);
	krb5_free_principal(authctxt->krb5_ctx, server);
	temporarily_use_uid(authctxt->pw);
	if (problem)
		goto out;

	if (!krb5_kuserok(authctxt->krb5_ctx, authctxt->krb5_user,
			  authctxt->pw->pw_name)) {
		problem = -1;
		goto out;
	}

	snprintf(ccname,sizeof(ccname),"FILE:/tmp/krb5cc_%d_XXXXXX",geteuid());

	if ((tmpfd = mkstemp(ccname+strlen("FILE:")))==-1) {
		logit("mkstemp(): %.100s", strerror(errno));
		problem = errno;
		goto out;
	}

	if (fchmod(tmpfd,S_IRUSR | S_IWUSR) == -1) {
		logit("fchmod(): %.100s", strerror(errno));
		close(tmpfd);
		problem = errno;
		goto out;
	}
	close(tmpfd);

	problem = krb5_cc_resolve(authctxt->krb5_ctx, ccname, &authctxt->krb5_fwd_ccache);
	if (problem)
		goto out;

	problem = krb5_cc_initialize(authctxt->krb5_ctx, authctxt->krb5_fwd_ccache,
				     authctxt->krb5_user);
	if (problem)
		goto out;

	problem= krb5_cc_store_cred(authctxt->krb5_ctx, authctxt->krb5_fwd_ccache,
				 &creds);
	if (problem)
		goto out;
#endif

	authctxt->krb5_ticket_file = (char *)krb5_cc_get_name(authctxt->krb5_ctx, authctxt->krb5_fwd_ccache);

	len = strlen(authctxt->krb5_ticket_file) + 6;
	authctxt->krb5_ccname = xmalloc(len);
	snprintf(authctxt->krb5_ccname, len, "FILE:%s",
	    authctxt->krb5_ticket_file);

 out:
	restore_uid();

	if (problem) {
		if (ccache)
			krb5_cc_destroy(authctxt->krb5_ctx, ccache);

		if (authctxt->krb5_ctx != NULL && problem!=-1)
			debug("Kerberos password authentication failed: %s",
			    krb5_get_err_text(authctxt->krb5_ctx, problem));
		else
			debug("Kerberos password authentication failed: %d",
			    problem);

		krb5_cleanup_proc(authctxt);

		if (options.kerberos_or_local_passwd)
			return (-1);
		else
			return (0);
	}
	return (1);
}

void
krb5_cleanup_proc(Authctxt *authctxt)
{
	debug("krb5_cleanup_proc called");
	if (authctxt->krb5_fwd_ccache) {
		krb5_cc_destroy(authctxt->krb5_ctx, authctxt->krb5_fwd_ccache);
		authctxt->krb5_fwd_ccache = NULL;
	}
	if (authctxt->krb5_user) {
		krb5_free_principal(authctxt->krb5_ctx, authctxt->krb5_user);
		authctxt->krb5_user = NULL;
	}
	if (authctxt->krb5_ctx) {
		krb5_free_context(authctxt->krb5_ctx);
		authctxt->krb5_ctx = NULL;
	}
}

#endif /* KRB5 */
