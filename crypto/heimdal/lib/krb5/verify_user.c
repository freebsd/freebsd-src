/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"

RCSID("$Id: verify_user.c,v 1.12 2001/01/04 17:40:00 joda Exp $");

static krb5_error_code
verify_common (krb5_context context,
	       krb5_principal principal,
	       krb5_ccache ccache,
	       krb5_boolean secure,
	       const char *service,
	       krb5_creds cred)
{
    krb5_error_code ret;
    krb5_principal server;
    krb5_verify_init_creds_opt vopt;
    krb5_ccache id;

    ret = krb5_sname_to_principal (context, NULL, service, KRB5_NT_SRV_HST,
				   &server);
    if(ret) return ret;

    krb5_verify_init_creds_opt_init(&vopt);
    krb5_verify_init_creds_opt_set_ap_req_nofail(&vopt, secure);

    ret = krb5_verify_init_creds(context,
				 &cred,
				 server,
				 NULL,
				 NULL,
				 &vopt);
    krb5_free_principal(context, server);
    if(ret) return ret;
    if(ccache == NULL)
	ret = krb5_cc_default (context, &id);
    else
	id = ccache;
    if(ret == 0){
	ret = krb5_cc_initialize(context, id, principal);
	if(ret == 0){
	    ret = krb5_cc_store_cred(context, id, &cred);
	}
	if(ccache == NULL)
	    krb5_cc_close(context, id);
    }
    krb5_free_creds_contents(context, &cred);
    return ret;
}

/*
 * Verify user `principal' with `password'.
 *
 * If `secure', also verify against local service key for `service'.
 *
 * As a side effect, fresh tickets are obtained and stored in `ccache'.
 */

krb5_error_code
krb5_verify_user(krb5_context context, 
		 krb5_principal principal,
		 krb5_ccache ccache,
		 const char *password,
		 krb5_boolean secure,
		 const char *service)
{

    krb5_error_code ret;
    krb5_get_init_creds_opt opt;
    krb5_creds cred;
    
    krb5_get_init_creds_opt_init (&opt);
    krb5_get_init_creds_opt_set_default_flags(context, NULL, 
					      *krb5_princ_realm(context, principal), 
					      &opt);

    ret = krb5_get_init_creds_password (context,
					&cred,
					principal,
					(char*)password,
					krb5_prompter_posix,
					NULL,
					0,
					NULL,
					&opt);
    
    if(ret)
	return ret;
    return verify_common (context, principal, ccache, secure, service, cred);
}

/*
 * A variant of `krb5_verify_user'.  The realm of `principal' is
 * ignored and all the local realms are tried.
 */

krb5_error_code
krb5_verify_user_lrealm(krb5_context context, 
			krb5_principal principal,
			krb5_ccache ccache,
			const char *password,
			krb5_boolean secure,
			const char *service)
{
    krb5_error_code ret;
    krb5_get_init_creds_opt opt;
    krb5_realm *realms, *r;
    krb5_creds cred;
    
    krb5_get_init_creds_opt_init (&opt);

    ret = krb5_get_default_realms (context, &realms);
    if (ret)
	return ret;
    ret = KRB5_CONFIG_NODEFREALM;

    for (r = realms; *r != NULL && ret != 0; ++r) {
	char *tmp = strdup (*r);

	if (tmp == NULL) {
	    krb5_free_host_realm (context, realms);
	    return ENOMEM;
	}
	free (*krb5_princ_realm (context, principal));
	krb5_princ_set_realm (context, principal, &tmp);

	krb5_get_init_creds_opt_set_default_flags(context, NULL, 
						  *krb5_princ_realm(context, principal), 
						  &opt);
	ret = krb5_get_init_creds_password (context,
					    &cred,
					    principal,
					    (char*)password,
					    krb5_prompter_posix,
					    NULL,
					    0,
					    NULL,
					    &opt);
    }
    krb5_free_host_realm (context, realms);
    if(ret)
	return ret;

    return verify_common (context, principal, ccache, secure, service, cred);
}
