/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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

RCSID("$Id: init_creds_pw.c 21931 2007-08-27 14:11:55Z lha $");

typedef struct krb5_get_init_creds_ctx {
    KDCOptions flags;
    krb5_creds cred;
    krb5_addresses *addrs;
    krb5_enctype *etypes;
    krb5_preauthtype *pre_auth_types;
    const char *in_tkt_service;
    unsigned nonce;
    unsigned pk_nonce;

    krb5_data req_buffer;
    AS_REQ as_req;
    int pa_counter;

    const char *password;
    krb5_s2k_proc key_proc;

    krb5_get_init_creds_tristate req_pac;

    krb5_pk_init_ctx pk_init_ctx;
    int ic_flags;
} krb5_get_init_creds_ctx;

static krb5_error_code
default_s2k_func(krb5_context context, krb5_enctype type, 
		 krb5_const_pointer keyseed,
		 krb5_salt salt, krb5_data *s2kparms,
		 krb5_keyblock **key)
{
    krb5_error_code ret;
    krb5_data password;
    krb5_data opaque;

    password.data = rk_UNCONST(keyseed);
    password.length = strlen(keyseed);
    if (s2kparms)
	opaque = *s2kparms;
    else
	krb5_data_zero(&opaque);
	
    *key = malloc(sizeof(**key));
    if (*key == NULL)
	return ENOMEM;
    ret = krb5_string_to_key_data_salt_opaque(context, type, password,
					      salt, opaque, *key);
    if (ret) {
	free(*key);
	*key = NULL;
    }
    return ret;
}

static void
free_init_creds_ctx(krb5_context context, krb5_get_init_creds_ctx *ctx)
{
    if (ctx->etypes)
	free(ctx->etypes);
    if (ctx->pre_auth_types)
	free (ctx->pre_auth_types);
    free_AS_REQ(&ctx->as_req);
    memset(&ctx->as_req, 0, sizeof(ctx->as_req));
}

static int
get_config_time (krb5_context context,
		 const char *realm,
		 const char *name,
		 int def)
{
    int ret;

    ret = krb5_config_get_time (context, NULL,
				"realms",
				realm,
				name,
				NULL);
    if (ret >= 0)
	return ret;
    ret = krb5_config_get_time (context, NULL,
				"libdefaults",
				name,
				NULL);
    if (ret >= 0)
	return ret;
    return def;
}

static krb5_error_code
init_cred (krb5_context context,
	   krb5_creds *cred,
	   krb5_principal client,
	   krb5_deltat start_time,
	   const char *in_tkt_service,
	   krb5_get_init_creds_opt *options)
{
    krb5_error_code ret;
    krb5_const_realm client_realm;
    int tmp;
    krb5_timestamp now;

    krb5_timeofday (context, &now);

    memset (cred, 0, sizeof(*cred));
    
    if (client)
	krb5_copy_principal(context, client, &cred->client);
    else {
	ret = krb5_get_default_principal (context,
					  &cred->client);
	if (ret)
	    goto out;
    }

    client_realm = krb5_principal_get_realm (context, cred->client);

    if (start_time)
	cred->times.starttime  = now + start_time;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)
	tmp = options->tkt_life;
    else
	tmp = 10 * 60 * 60;
    cred->times.endtime = now + tmp;

    if ((options->flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE) &&
	options->renew_life > 0) {
	cred->times.renew_till = now + options->renew_life;
    }

    if (in_tkt_service) {
	krb5_realm server_realm;

	ret = krb5_parse_name (context, in_tkt_service, &cred->server);
	if (ret)
	    goto out;
	server_realm = strdup (client_realm);
	free (*krb5_princ_realm(context, cred->server));
	krb5_princ_set_realm (context, cred->server, &server_realm);
    } else {
	ret = krb5_make_principal(context, &cred->server, 
				  client_realm, KRB5_TGS_NAME, client_realm,
				  NULL);
	if (ret)
	    goto out;
    }
    return 0;

out:
    krb5_free_cred_contents (context, cred);
    return ret;
}

/*
 * Print a message (str) to the user about the expiration in `lr'
 */

static void
report_expiration (krb5_context context,
		   krb5_prompter_fct prompter,
		   krb5_data *data,
		   const char *str,
		   time_t now)
{
    char *p;
	    
    asprintf (&p, "%s%s", str, ctime(&now));
    (*prompter) (context, data, NULL, p, 0, NULL);
    free (p);
}

/*
 * Parse the last_req data and show it to the user if it's interesting
 */

static void
print_expire (krb5_context context,
	      krb5_const_realm realm,
	      krb5_kdc_rep *rep,
	      krb5_prompter_fct prompter,
	      krb5_data *data)
{
    int i;
    LastReq *lr = &rep->enc_part.last_req;
    krb5_timestamp sec;
    time_t t;
    krb5_boolean reported = FALSE;

    krb5_timeofday (context, &sec);

    t = sec + get_config_time (context,
			       realm,
			       "warn_pwexpire",
			       7 * 24 * 60 * 60);

    for (i = 0; i < lr->len; ++i) {
	if (lr->val[i].lr_value <= t) {
	    switch (abs(lr->val[i].lr_type)) {
	    case LR_PW_EXPTIME :
		report_expiration(context, prompter, data,
				  "Your password will expire at ",
				  lr->val[i].lr_value);
		reported = TRUE;
		break;
	    case LR_ACCT_EXPTIME :
		report_expiration(context, prompter, data,
				  "Your account will expire at ",
				  lr->val[i].lr_value);
		reported = TRUE;
		break;
	    }
	}
    }

    if (!reported
	&& rep->enc_part.key_expiration
	&& *rep->enc_part.key_expiration <= t) {
	report_expiration(context, prompter, data,
			  "Your password/account will expire at ",
			  *rep->enc_part.key_expiration);
    }
}

static krb5_addresses no_addrs = { 0, NULL };

static krb5_error_code
get_init_creds_common(krb5_context context,
		      krb5_principal client,
		      krb5_deltat start_time,
		      const char *in_tkt_service,
		      krb5_get_init_creds_opt *options,
		      krb5_get_init_creds_ctx *ctx)
{
    krb5_get_init_creds_opt default_opt;
    krb5_error_code ret;
    krb5_enctype *etypes;
    krb5_preauthtype *pre_auth_types;

    memset(ctx, 0, sizeof(*ctx));

    if (options == NULL) {
	krb5_get_init_creds_opt_init (&default_opt);
	options = &default_opt;
    } else {
	_krb5_get_init_creds_opt_free_krb5_error(options);
    }

    if (options->opt_private) {
	ctx->password = options->opt_private->password;
	ctx->key_proc = options->opt_private->key_proc;
	ctx->req_pac = options->opt_private->req_pac;
	ctx->pk_init_ctx = options->opt_private->pk_init_ctx;
	ctx->ic_flags = options->opt_private->flags;
    } else
	ctx->req_pac = KRB5_INIT_CREDS_TRISTATE_UNSET;

    if (ctx->key_proc == NULL)
	ctx->key_proc = default_s2k_func;

    if (ctx->ic_flags & KRB5_INIT_CREDS_CANONICALIZE)
	ctx->flags.canonicalize = 1;

    ctx->pre_auth_types = NULL;
    ctx->addrs = NULL;
    ctx->etypes = NULL;
    ctx->pre_auth_types = NULL;
    ctx->in_tkt_service = in_tkt_service;

    ret = init_cred (context, &ctx->cred, client, start_time,
		     in_tkt_service, options);
    if (ret)
	return ret;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)
	ctx->flags.forwardable = options->forwardable;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE)
	ctx->flags.proxiable = options->proxiable;

    if (start_time)
	ctx->flags.postdated = 1;
    if (ctx->cred.times.renew_till)
	ctx->flags.renewable = 1;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST) {
	ctx->addrs = options->address_list;
    } else if (options->opt_private) {
	switch (options->opt_private->addressless) {
	case KRB5_INIT_CREDS_TRISTATE_UNSET:
#if KRB5_ADDRESSLESS_DEFAULT == TRUE
	    ctx->addrs = &no_addrs;
#else
	    ctx->addrs = NULL;
#endif
	    break;
	case KRB5_INIT_CREDS_TRISTATE_FALSE:
	    ctx->addrs = NULL;
	    break;
	case KRB5_INIT_CREDS_TRISTATE_TRUE:
	    ctx->addrs = &no_addrs;
	    break;
	}
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST) {
	etypes = malloc((options->etype_list_length + 1)
			* sizeof(krb5_enctype));
	if (etypes == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	memcpy (etypes, options->etype_list,
		options->etype_list_length * sizeof(krb5_enctype));
	etypes[options->etype_list_length] = ETYPE_NULL;
	ctx->etypes = etypes;
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST) {
	pre_auth_types = malloc((options->preauth_list_length + 1)
				* sizeof(krb5_preauthtype));
	if (pre_auth_types == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	memcpy (pre_auth_types, options->preauth_list,
		options->preauth_list_length * sizeof(krb5_preauthtype));
	pre_auth_types[options->preauth_list_length] = KRB5_PADATA_NONE;
	ctx->pre_auth_types = pre_auth_types;
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_SALT)
	;			/* XXX */
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ANONYMOUS)
	ctx->flags.request_anonymous = options->anonymous;
    return 0;
}

static krb5_error_code
change_password (krb5_context context,
		 krb5_principal client,
		 const char *password,
		 char *newpw,
		 size_t newpw_sz,
		 krb5_prompter_fct prompter,
		 void *data,
		 krb5_get_init_creds_opt *old_options)
{
    krb5_prompt prompts[2];
    krb5_error_code ret;
    krb5_creds cpw_cred;
    char buf1[BUFSIZ], buf2[BUFSIZ];
    krb5_data password_data[2];
    int result_code;
    krb5_data result_code_string;
    krb5_data result_string;
    char *p;
    krb5_get_init_creds_opt options;

    memset (&cpw_cred, 0, sizeof(cpw_cred));

    krb5_get_init_creds_opt_init (&options);
    krb5_get_init_creds_opt_set_tkt_life (&options, 60);
    krb5_get_init_creds_opt_set_forwardable (&options, FALSE);
    krb5_get_init_creds_opt_set_proxiable (&options, FALSE);
    if (old_options && old_options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST)
	krb5_get_init_creds_opt_set_preauth_list (&options,
						  old_options->preauth_list,
						  old_options->preauth_list_length);					      

    krb5_data_zero (&result_code_string);
    krb5_data_zero (&result_string);

    ret = krb5_get_init_creds_password (context,
					&cpw_cred,
					client,
					password,
					prompter,
					data,
					0,
					"kadmin/changepw",
					&options);
    if (ret)
	goto out;

    for(;;) {
	password_data[0].data   = buf1;
	password_data[0].length = sizeof(buf1);

	prompts[0].hidden = 1;
	prompts[0].prompt = "New password: ";
	prompts[0].reply  = &password_data[0];
	prompts[0].type   = KRB5_PROMPT_TYPE_NEW_PASSWORD;

	password_data[1].data   = buf2;
	password_data[1].length = sizeof(buf2);

	prompts[1].hidden = 1;
	prompts[1].prompt = "Repeat new password: ";
	prompts[1].reply  = &password_data[1];
	prompts[1].type   = KRB5_PROMPT_TYPE_NEW_PASSWORD_AGAIN;

	ret = (*prompter) (context, data, NULL, "Changing password",
			   2, prompts);
	if (ret) {
	    memset (buf1, 0, sizeof(buf1));
	    memset (buf2, 0, sizeof(buf2));
	    goto out;
	}

	if (strcmp (buf1, buf2) == 0)
	    break;
	memset (buf1, 0, sizeof(buf1));
	memset (buf2, 0, sizeof(buf2));
    }
    
    ret = krb5_change_password (context,
				&cpw_cred,
				buf1,
				&result_code,
				&result_code_string,
				&result_string);
    if (ret)
	goto out;
    asprintf (&p, "%s: %.*s\n",
	      result_code ? "Error" : "Success",
	      (int)result_string.length,
	      result_string.length > 0 ? (char*)result_string.data : "");

    ret = (*prompter) (context, data, NULL, p, 0, NULL);
    free (p);
    if (result_code == 0) {
	strlcpy (newpw, buf1, newpw_sz);
	ret = 0;
    } else {
	krb5_set_error_string (context, "failed changing password");
	ret = ENOTTY;
    }

out:
    memset (buf1, 0, sizeof(buf1));
    memset (buf2, 0, sizeof(buf2));
    krb5_data_free (&result_string);
    krb5_data_free (&result_code_string);
    krb5_free_cred_contents (context, &cpw_cred);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_keyblock_key_proc (krb5_context context,
			krb5_keytype type,
			krb5_data *salt,
			krb5_const_pointer keyseed,
			krb5_keyblock **key)
{
    return krb5_copy_keyblock (context, keyseed, key);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_keytab(krb5_context context,
			   krb5_creds *creds,
			   krb5_principal client,
			   krb5_keytab keytab,
			   krb5_deltat start_time,
			   const char *in_tkt_service,
			   krb5_get_init_creds_opt *options)
{
    krb5_get_init_creds_ctx ctx;
    krb5_error_code ret;
    krb5_keytab_key_proc_args *a;
    
    ret = get_init_creds_common(context, client, start_time,
				in_tkt_service, options, &ctx);
    if (ret)
	goto out;

    a = malloc (sizeof(*a));
    if (a == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    a->principal = ctx.cred.client;
    a->keytab    = keytab;

    ret = krb5_get_in_cred (context,
			    KDCOptions2int(ctx.flags),
			    ctx.addrs,
			    ctx.etypes,
			    ctx.pre_auth_types,
			    NULL,
			    krb5_keytab_key_proc,
			    a,
			    NULL,
			    NULL,
			    &ctx.cred,
			    NULL);
    free (a);

    if (ret == 0 && creds)
	*creds = ctx.cred;
    else
	krb5_free_cred_contents (context, &ctx.cred);

 out:
    free_init_creds_ctx(context, &ctx);
    return ret;
}

/*
 *
 */

static krb5_error_code
init_creds_init_as_req (krb5_context context,
			KDCOptions opts,
			const krb5_creds *creds,
			const krb5_addresses *addrs,
			const krb5_enctype *etypes,
			AS_REQ *a)
{
    krb5_error_code ret;

    memset(a, 0, sizeof(*a));

    a->pvno = 5;
    a->msg_type = krb_as_req;
    a->req_body.kdc_options = opts;
    a->req_body.cname = malloc(sizeof(*a->req_body.cname));
    if (a->req_body.cname == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }
    a->req_body.sname = malloc(sizeof(*a->req_body.sname));
    if (a->req_body.sname == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }

    ret = _krb5_principal2principalname (a->req_body.cname, creds->client);
    if (ret)
	goto fail;
    ret = copy_Realm(&creds->client->realm, &a->req_body.realm);
    if (ret)
	goto fail;

    ret = _krb5_principal2principalname (a->req_body.sname, creds->server);
    if (ret)
	goto fail;

    if(creds->times.starttime) {
	a->req_body.from = malloc(sizeof(*a->req_body.from));
	if (a->req_body.from == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	*a->req_body.from = creds->times.starttime;
    }
    if(creds->times.endtime){
	ALLOC(a->req_body.till, 1);
	*a->req_body.till = creds->times.endtime;
    }
    if(creds->times.renew_till){
	a->req_body.rtime = malloc(sizeof(*a->req_body.rtime));
	if (a->req_body.rtime == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	*a->req_body.rtime = creds->times.renew_till;
    }
    a->req_body.nonce = 0;
    ret = krb5_init_etype (context,
			   &a->req_body.etype.len,
			   &a->req_body.etype.val,
			   etypes);
    if (ret)
	goto fail;

    /*
     * This means no addresses
     */

    if (addrs && addrs->len == 0) {
	a->req_body.addresses = NULL;
    } else {
	a->req_body.addresses = malloc(sizeof(*a->req_body.addresses));
	if (a->req_body.addresses == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}

	if (addrs)
	    ret = krb5_copy_addresses(context, addrs, a->req_body.addresses);
	else {
	    ret = krb5_get_all_client_addrs (context, a->req_body.addresses);
	    if(ret == 0 && a->req_body.addresses->len == 0) {
		free(a->req_body.addresses);
		a->req_body.addresses = NULL;
	    }
	}
	if (ret)
	    goto fail;
    }

    a->req_body.enc_authorization_data = NULL;
    a->req_body.additional_tickets = NULL;

    a->padata = NULL;

    return 0;
 fail:
    free_AS_REQ(a);
    memset(a, 0, sizeof(*a));
    return ret;
}

struct pa_info_data {
    krb5_enctype etype;
    krb5_salt salt;
    krb5_data *s2kparams;
};

static void
free_paid(krb5_context context, struct pa_info_data *ppaid)
{
    krb5_free_salt(context, ppaid->salt);
    if (ppaid->s2kparams)
	krb5_free_data(context, ppaid->s2kparams);
}


static krb5_error_code
set_paid(struct pa_info_data *paid, krb5_context context,
	 krb5_enctype etype,
	 krb5_salttype salttype, void *salt_string, size_t salt_len,
	 krb5_data *s2kparams)
{
    paid->etype = etype;
    paid->salt.salttype = salttype;
    paid->salt.saltvalue.data = malloc(salt_len + 1);
    if (paid->salt.saltvalue.data == NULL) {
	krb5_clear_error_string(context);
	return ENOMEM;
    }
    memcpy(paid->salt.saltvalue.data, salt_string, salt_len);
    ((char *)paid->salt.saltvalue.data)[salt_len] = '\0';
    paid->salt.saltvalue.length = salt_len;
    if (s2kparams) {
	krb5_error_code ret;

	ret = krb5_copy_data(context, s2kparams, &paid->s2kparams);
	if (ret) {
	    krb5_clear_error_string(context);
	    krb5_free_salt(context, paid->salt);
	    return ret;
	}
    } else
	paid->s2kparams = NULL;

    return 0;
}

static struct pa_info_data *
pa_etype_info2(krb5_context context,
	       const krb5_principal client, 
	       const AS_REQ *asreq,
	       struct pa_info_data *paid, 
	       heim_octet_string *data)
{
    krb5_error_code ret;
    ETYPE_INFO2 e;
    size_t sz;
    int i, j;

    memset(&e, 0, sizeof(e));
    ret = decode_ETYPE_INFO2(data->data, data->length, &e, &sz);
    if (ret)
	goto out;
    if (e.len == 0)
	goto out;
    for (j = 0; j < asreq->req_body.etype.len; j++) {
	for (i = 0; i < e.len; i++) {
	    if (asreq->req_body.etype.val[j] == e.val[i].etype) {
		krb5_salt salt;
		if (e.val[i].salt == NULL)
		    ret = krb5_get_pw_salt(context, client, &salt);
		else {
		    salt.saltvalue.data = *e.val[i].salt;
		    salt.saltvalue.length = strlen(*e.val[i].salt);
		    ret = 0;
		}
		if (ret == 0)
		    ret = set_paid(paid, context, e.val[i].etype,
				   KRB5_PW_SALT,
				   salt.saltvalue.data, 
				   salt.saltvalue.length,
				   e.val[i].s2kparams);
		if (e.val[i].salt == NULL)
		    krb5_free_salt(context, salt);
		if (ret == 0) {
		    free_ETYPE_INFO2(&e);
		    return paid;
		}
	    }
	}
    }
 out:
    free_ETYPE_INFO2(&e);
    return NULL;
}

static struct pa_info_data *
pa_etype_info(krb5_context context,
	      const krb5_principal client, 
	      const AS_REQ *asreq,
	      struct pa_info_data *paid,
	      heim_octet_string *data)
{
    krb5_error_code ret;
    ETYPE_INFO e;
    size_t sz;
    int i, j;

    memset(&e, 0, sizeof(e));
    ret = decode_ETYPE_INFO(data->data, data->length, &e, &sz);
    if (ret)
	goto out;
    if (e.len == 0)
	goto out;
    for (j = 0; j < asreq->req_body.etype.len; j++) {
	for (i = 0; i < e.len; i++) {
	    if (asreq->req_body.etype.val[j] == e.val[i].etype) {
		krb5_salt salt;
		salt.salttype = KRB5_PW_SALT;
		if (e.val[i].salt == NULL)
		    ret = krb5_get_pw_salt(context, client, &salt);
		else {
		    salt.saltvalue = *e.val[i].salt;
		    ret = 0;
		}
		if (e.val[i].salttype)
		    salt.salttype = *e.val[i].salttype;
		if (ret == 0) {
		    ret = set_paid(paid, context, e.val[i].etype,
				   salt.salttype,
				   salt.saltvalue.data, 
				   salt.saltvalue.length,
				   NULL);
		    if (e.val[i].salt == NULL)
			krb5_free_salt(context, salt);
		}
		if (ret == 0) {
		    free_ETYPE_INFO(&e);
		    return paid;
		}
	    }
	}
    }
 out:
    free_ETYPE_INFO(&e);
    return NULL;
}

static struct pa_info_data *
pa_pw_or_afs3_salt(krb5_context context,
		   const krb5_principal client, 
		   const AS_REQ *asreq,
		   struct pa_info_data *paid,
		   heim_octet_string *data)
{
    krb5_error_code ret;
    if (paid->etype == ENCTYPE_NULL)
	return NULL;
    ret = set_paid(paid, context, 
		   paid->etype,
		   paid->salt.salttype,
		   data->data, 
		   data->length,
		   NULL);
    if (ret)
	return NULL;
    return paid;
}


struct pa_info {
    krb5_preauthtype type;
    struct pa_info_data *(*salt_info)(krb5_context,
				      const krb5_principal, 
				      const AS_REQ *,
				      struct pa_info_data *, 
				      heim_octet_string *);
};

static struct pa_info pa_prefs[] = {
    { KRB5_PADATA_ETYPE_INFO2, pa_etype_info2 },
    { KRB5_PADATA_ETYPE_INFO, pa_etype_info },
    { KRB5_PADATA_PW_SALT, pa_pw_or_afs3_salt },
    { KRB5_PADATA_AFS3_SALT, pa_pw_or_afs3_salt }
};
    
static PA_DATA *
find_pa_data(const METHOD_DATA *md, int type)
{
    int i;
    if (md == NULL)
	return NULL;
    for (i = 0; i < md->len; i++)
	if (md->val[i].padata_type == type)
	    return &md->val[i];
    return NULL;
}

static struct pa_info_data *
process_pa_info(krb5_context context, 
		const krb5_principal client, 
		const AS_REQ *asreq,
		struct pa_info_data *paid,
		METHOD_DATA *md)
{
    struct pa_info_data *p = NULL;
    int i;

    for (i = 0; p == NULL && i < sizeof(pa_prefs)/sizeof(pa_prefs[0]); i++) {
	PA_DATA *pa = find_pa_data(md, pa_prefs[i].type);
	if (pa == NULL)
	    continue;
	paid->salt.salttype = pa_prefs[i].type;
	p = (*pa_prefs[i].salt_info)(context, client, asreq,
				     paid, &pa->padata_value);
    }
    return p;
}

static krb5_error_code
make_pa_enc_timestamp(krb5_context context, METHOD_DATA *md, 
		      krb5_enctype etype, krb5_keyblock *key)
{
    PA_ENC_TS_ENC p;
    unsigned char *buf;
    size_t buf_size;
    size_t len;
    EncryptedData encdata;
    krb5_error_code ret;
    int32_t usec;
    int usec2;
    krb5_crypto crypto;
    
    krb5_us_timeofday (context, &p.patimestamp, &usec);
    usec2         = usec;
    p.pausec      = &usec2;

    ASN1_MALLOC_ENCODE(PA_ENC_TS_ENC, buf, buf_size, &p, &len, ret);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret) {
	free(buf);
	return ret;
    }
    ret = krb5_encrypt_EncryptedData(context, 
				     crypto,
				     KRB5_KU_PA_ENC_TIMESTAMP,
				     buf,
				     len,
				     0,
				     &encdata);
    free(buf);
    krb5_crypto_destroy(context, crypto);
    if (ret)
	return ret;
		    
    ASN1_MALLOC_ENCODE(EncryptedData, buf, buf_size, &encdata, &len, ret);
    free_EncryptedData(&encdata);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_padata_add(context, md, KRB5_PADATA_ENC_TIMESTAMP, buf, len);
    if (ret)
	free(buf);
    return ret;
}

static krb5_error_code
add_enc_ts_padata(krb5_context context,
		  METHOD_DATA *md, 
		  krb5_principal client,
		  krb5_s2k_proc key_proc,
		  krb5_const_pointer keyseed,
		  krb5_enctype *enctypes,
		  unsigned netypes,
		  krb5_salt *salt,
		  krb5_data *s2kparams)
{
    krb5_error_code ret;
    krb5_salt salt2;
    krb5_enctype *ep;
    int i;
    
    if(salt == NULL) {
	/* default to standard salt */
	ret = krb5_get_pw_salt (context, client, &salt2);
	salt = &salt2;
    }
    if (!enctypes) {
	enctypes = context->etypes;
	netypes = 0;
	for (ep = enctypes; *ep != ETYPE_NULL; ep++)
	    netypes++;
    }

    for (i = 0; i < netypes; ++i) {
	krb5_keyblock *key;

	ret = (*key_proc)(context, enctypes[i], keyseed,
			  *salt, s2kparams, &key);
	if (ret)
	    continue;
	ret = make_pa_enc_timestamp (context, md, enctypes[i], key);
	krb5_free_keyblock (context, key);
	if (ret)
	    return ret;
    }
    if(salt == &salt2)
	krb5_free_salt(context, salt2);
    return 0;
}

static krb5_error_code
pa_data_to_md_ts_enc(krb5_context context,
		     const AS_REQ *a,
		     const krb5_principal client,
		     krb5_get_init_creds_ctx *ctx,
		     struct pa_info_data *ppaid,
		     METHOD_DATA *md)
{
    if (ctx->key_proc == NULL || ctx->password == NULL)
	return 0;

    if (ppaid) {
	add_enc_ts_padata(context, md, client, 
			  ctx->key_proc, ctx->password,
			  &ppaid->etype, 1,
			  &ppaid->salt, ppaid->s2kparams);
    } else {
	krb5_salt salt;
	
	/* make a v5 salted pa-data */
	add_enc_ts_padata(context, md, client, 
			  ctx->key_proc, ctx->password,
			  a->req_body.etype.val, a->req_body.etype.len, 
			  NULL, NULL);
	
	/* make a v4 salted pa-data */
	salt.salttype = KRB5_PW_SALT;
	krb5_data_zero(&salt.saltvalue);
	add_enc_ts_padata(context, md, client, 
			  ctx->key_proc, ctx->password, 
			  a->req_body.etype.val, a->req_body.etype.len, 
			  &salt, NULL);
    }
    return 0;
}

static krb5_error_code
pa_data_to_key_plain(krb5_context context,
		     const krb5_principal client,
		     krb5_get_init_creds_ctx *ctx,
		     krb5_salt salt,
		     krb5_data *s2kparams,
		     krb5_enctype etype,
		     krb5_keyblock **key)
{
    krb5_error_code ret;

    ret = (*ctx->key_proc)(context, etype, ctx->password,
			   salt, s2kparams, key);
    return ret;
}


static krb5_error_code
pa_data_to_md_pkinit(krb5_context context,
		     const AS_REQ *a,
		     const krb5_principal client,
		     krb5_get_init_creds_ctx *ctx,
		     METHOD_DATA *md)
{
    if (ctx->pk_init_ctx == NULL)
	return 0;
#ifdef PKINIT
    return _krb5_pk_mk_padata(context,
			     ctx->pk_init_ctx,
			     &a->req_body,
			     ctx->pk_nonce,
			     md);
#else
    krb5_set_error_string(context, "no support for PKINIT compiled in");
    return EINVAL;
#endif
}

static krb5_error_code
pa_data_add_pac_request(krb5_context context,
			krb5_get_init_creds_ctx *ctx,
			METHOD_DATA *md)
{
    size_t len, length;
    krb5_error_code ret;
    PA_PAC_REQUEST req;
    void *buf;
    
    switch (ctx->req_pac) {
    case KRB5_INIT_CREDS_TRISTATE_UNSET:
	return 0; /* don't bother */
    case KRB5_INIT_CREDS_TRISTATE_TRUE:
	req.include_pac = 1;
	break;
    case KRB5_INIT_CREDS_TRISTATE_FALSE:
	req.include_pac = 0;
    }	

    ASN1_MALLOC_ENCODE(PA_PAC_REQUEST, buf, length, 
		       &req, &len, ret);
    if (ret)
	return ret;
    if(len != length)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_padata_add(context, md, KRB5_PADATA_PA_PAC_REQUEST, buf, len);
    if (ret)
	free(buf);

    return 0;
}

/*
 * Assumes caller always will free `out_md', even on error.
 */

static krb5_error_code
process_pa_data_to_md(krb5_context context,
		      const krb5_creds *creds,
		      const AS_REQ *a,
		      krb5_get_init_creds_ctx *ctx,
		      METHOD_DATA *in_md,
		      METHOD_DATA **out_md,
		      krb5_prompter_fct prompter,
		      void *prompter_data)
{
    krb5_error_code ret;

    ALLOC(*out_md, 1);
    if (*out_md == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    (*out_md)->len = 0;
    (*out_md)->val = NULL;
    
    /*
     * Make sure we don't sent both ENC-TS and PK-INIT pa data, no
     * need to expose our password protecting our PKCS12 key.
     */

    if (ctx->pk_init_ctx) {

	ret = pa_data_to_md_pkinit(context, a, creds->client, ctx, *out_md);
	if (ret)
	    return ret;

    } else if (in_md->len != 0) {
	struct pa_info_data paid, *ppaid;
	
	memset(&paid, 0, sizeof(paid));
	
	paid.etype = ENCTYPE_NULL;
	ppaid = process_pa_info(context, creds->client, a, &paid, in_md);
	
	pa_data_to_md_ts_enc(context, a, creds->client, ctx, ppaid, *out_md);
	if (ppaid)
	    free_paid(context, ppaid);
    }

    pa_data_add_pac_request(context, ctx, *out_md);

    if ((*out_md)->len == 0) {
	free(*out_md);
	*out_md = NULL;
    }

    return 0;
}

static krb5_error_code
process_pa_data_to_key(krb5_context context,
		       krb5_get_init_creds_ctx *ctx,
		       krb5_creds *creds,
		       AS_REQ *a,
		       krb5_kdc_rep *rep,
		       const krb5_krbhst_info *hi,
		       krb5_keyblock **key)
{
    struct pa_info_data paid, *ppaid = NULL;
    krb5_error_code ret;
    krb5_enctype etype;
    PA_DATA *pa;

    memset(&paid, 0, sizeof(paid));

    etype = rep->kdc_rep.enc_part.etype;

    if (rep->kdc_rep.padata) {
	paid.etype = etype;
	ppaid = process_pa_info(context, creds->client, a, &paid, 
				rep->kdc_rep.padata);
    }
    if (ppaid == NULL) {
	ret = krb5_get_pw_salt (context, creds->client, &paid.salt);
	if (ret)
	    return ret;
	paid.etype = etype;
	paid.s2kparams = NULL;
    }

    pa = NULL;
    if (rep->kdc_rep.padata) {
	int idx = 0;
	pa = krb5_find_padata(rep->kdc_rep.padata->val, 
			      rep->kdc_rep.padata->len,
			      KRB5_PADATA_PK_AS_REP,
			      &idx);
	if (pa == NULL) {
	    idx = 0;
	    pa = krb5_find_padata(rep->kdc_rep.padata->val, 
				  rep->kdc_rep.padata->len,
				  KRB5_PADATA_PK_AS_REP_19,
				  &idx);
	}
    }
    if (pa && ctx->pk_init_ctx) {
#ifdef PKINIT
	ret = _krb5_pk_rd_pa_reply(context,
				   a->req_body.realm,
				   ctx->pk_init_ctx,
				   etype,
				   hi,
				   ctx->pk_nonce,
				   &ctx->req_buffer,
				   pa,
				   key);
#else
	krb5_set_error_string(context, "no support for PKINIT compiled in");
	ret = EINVAL;
#endif
    } else if (ctx->password)
	ret = pa_data_to_key_plain(context, creds->client, ctx, 
				   paid.salt, paid.s2kparams, etype, key);
    else {
	krb5_set_error_string(context, "No usable pa data type");
	ret = EINVAL;
    }

    free_paid(context, &paid);
    return ret;
}

static krb5_error_code
init_cred_loop(krb5_context context,
	       krb5_get_init_creds_opt *init_cred_opts,
	       const krb5_prompter_fct prompter,
	       void *prompter_data,
	       krb5_get_init_creds_ctx *ctx,
	       krb5_creds *creds,
	       krb5_kdc_rep *ret_as_reply)
{
    krb5_error_code ret;
    krb5_kdc_rep rep;
    METHOD_DATA md;
    krb5_data resp;
    size_t len;
    size_t size;
    krb5_krbhst_info *hi = NULL;
    krb5_sendto_ctx stctx = NULL;


    memset(&md, 0, sizeof(md));
    memset(&rep, 0, sizeof(rep));

    _krb5_get_init_creds_opt_free_krb5_error(init_cred_opts);

    if (ret_as_reply)
	memset(ret_as_reply, 0, sizeof(*ret_as_reply));

    ret = init_creds_init_as_req(context, ctx->flags, creds,
				 ctx->addrs, ctx->etypes, &ctx->as_req);
    if (ret)
	return ret;

    ret = krb5_sendto_ctx_alloc(context, &stctx);
    if (ret)
	goto out;
    krb5_sendto_ctx_set_func(stctx, _krb5_kdc_retry, NULL);

    /* Set a new nonce. */
    krb5_generate_random_block (&ctx->nonce, sizeof(ctx->nonce));
    ctx->nonce &= 0xffffffff;
    /* XXX these just needs to be the same when using Windows PK-INIT */
    ctx->pk_nonce = ctx->nonce;

    /*
     * Increase counter when we want other pre-auth types then
     * KRB5_PA_ENC_TIMESTAMP.
     */
#define MAX_PA_COUNTER 3 

    ctx->pa_counter = 0;
    while (ctx->pa_counter < MAX_PA_COUNTER) {

	ctx->pa_counter++;

	if (ctx->as_req.padata) {
	    free_METHOD_DATA(ctx->as_req.padata);
	    free(ctx->as_req.padata);
	    ctx->as_req.padata = NULL;
	}

	/* Set a new nonce. */
	ctx->as_req.req_body.nonce = ctx->nonce;

	/* fill_in_md_data */
	ret = process_pa_data_to_md(context, creds, &ctx->as_req, ctx,
				    &md, &ctx->as_req.padata,
				    prompter, prompter_data);
	if (ret)
	    goto out;

	krb5_data_free(&ctx->req_buffer);

	ASN1_MALLOC_ENCODE(AS_REQ, 
			   ctx->req_buffer.data, ctx->req_buffer.length, 
			   &ctx->as_req, &len, ret);
	if (ret)
	    goto out;
	if(len != ctx->req_buffer.length)
	    krb5_abortx(context, "internal error in ASN.1 encoder");

	ret = krb5_sendto_context (context, stctx, &ctx->req_buffer,
				   creds->client->realm, &resp);
    	if (ret)
	    goto out;

	memset (&rep, 0, sizeof(rep));
	ret = decode_AS_REP(resp.data, resp.length, &rep.kdc_rep, &size);
	if (ret == 0) {
	    krb5_data_free(&resp);
	    krb5_clear_error_string(context);
	    break;
	} else {
	    /* let's try to parse it as a KRB-ERROR */
	    KRB_ERROR error;

	    ret = krb5_rd_error(context, &resp, &error);
	    if(ret && resp.data && ((char*)resp.data)[0] == 4)
		ret = KRB5KRB_AP_ERR_V4_REPLY;
	    krb5_data_free(&resp);
	    if (ret)
		goto out;

	    ret = krb5_error_from_rd_error(context, &error, creds);

	    /*
	     * If no preauth was set and KDC requires it, give it one
	     * more try.
	     */

	    if (ret == KRB5KDC_ERR_PREAUTH_REQUIRED) {
		free_METHOD_DATA(&md);
		memset(&md, 0, sizeof(md));

		if (error.e_data) {
		    ret = decode_METHOD_DATA(error.e_data->data, 
					     error.e_data->length, 
					     &md, 
					     NULL);
		    if (ret)
			krb5_set_error_string(context,
					      "failed to decode METHOD DATA");
		} else {
		    /* XXX guess what the server want here add add md */
		}
		krb5_free_error_contents(context, &error);
		if (ret)
		    goto out;
	    } else {
		_krb5_get_init_creds_opt_set_krb5_error(context,
							init_cred_opts,
							&error);
		if (ret_as_reply)
		    rep.error = error;
		else
		    krb5_free_error_contents(context, &error);
		goto out;
	    }
	}
    }

    {
	krb5_keyblock *key = NULL;
	unsigned flags = 0;

	if (ctx->flags.request_anonymous)
	    flags |= EXTRACT_TICKET_ALLOW_SERVER_MISMATCH;
	if (ctx->flags.canonicalize) {
	    flags |= EXTRACT_TICKET_ALLOW_CNAME_MISMATCH;
	    flags |= EXTRACT_TICKET_ALLOW_SERVER_MISMATCH;
	    flags |= EXTRACT_TICKET_MATCH_REALM;
	}

	ret = process_pa_data_to_key(context, ctx, creds, 
				     &ctx->as_req, &rep, hi, &key);
	if (ret)
	    goto out;
	
	ret = _krb5_extract_ticket(context,
				   &rep,
				   creds,
				   key,
				   NULL,
				   KRB5_KU_AS_REP_ENC_PART,
				   NULL,
				   ctx->nonce,
				   flags,
				   NULL,
				   NULL);
	krb5_free_keyblock(context, key);
    }
    /*
     * Verify referral data
     */
    if ((ctx->ic_flags & KRB5_INIT_CREDS_CANONICALIZE) &&
	(ctx->ic_flags & KRB5_INIT_CREDS_NO_C_CANON_CHECK) == 0)
    {
	PA_ClientCanonicalized canon;
	krb5_crypto crypto;
	krb5_data data;
	PA_DATA *pa;
	size_t len;

	pa = find_pa_data(rep.kdc_rep.padata, KRB5_PADATA_CLIENT_CANONICALIZED);
	if (pa == NULL) {
	    ret = EINVAL;
	    krb5_set_error_string(context, "Client canonicalizion not signed");
	    goto out;
	}
	
	ret = decode_PA_ClientCanonicalized(pa->padata_value.data, 
					    pa->padata_value.length,
					    &canon, &len);
	if (ret) {
	    krb5_set_error_string(context, "Failed to decode "
				  "PA_ClientCanonicalized");
	    goto out;
	}

	ASN1_MALLOC_ENCODE(PA_ClientCanonicalizedNames, data.data, data.length,
			   &canon.names, &len, ret);
	if (ret) 
	    goto out;
	if (data.length != len)
	    krb5_abortx(context, "internal asn.1 error");

	ret = krb5_crypto_init(context, &creds->session, 0, &crypto);
	if (ret) {
	    free(data.data);
	    free_PA_ClientCanonicalized(&canon);
	    goto out;
	}

	ret = krb5_verify_checksum(context, crypto, KRB5_KU_CANONICALIZED_NAMES,
				   data.data, data.length,
				   &canon.canon_checksum);
	krb5_crypto_destroy(context, crypto);
	free(data.data);
	free_PA_ClientCanonicalized(&canon);
	if (ret) {
	    krb5_set_error_string(context, "Failed to verify "
				  "client canonicalized data");
	    goto out;
	}
    }
out:
    if (stctx)
	krb5_sendto_ctx_free(context, stctx);
    krb5_data_free(&ctx->req_buffer);
    free_METHOD_DATA(&md);
    memset(&md, 0, sizeof(md));

    if (ret == 0 && ret_as_reply)
	*ret_as_reply = rep;
    else 
	krb5_free_kdc_rep (context, &rep);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds(krb5_context context,
		    krb5_creds *creds,
		    krb5_principal client,
		    krb5_prompter_fct prompter,
		    void *data,
		    krb5_deltat start_time,
		    const char *in_tkt_service,
		    krb5_get_init_creds_opt *options)
{
    krb5_get_init_creds_ctx ctx;
    krb5_kdc_rep kdc_reply;
    krb5_error_code ret;
    char buf[BUFSIZ];
    int done;

    memset(&kdc_reply, 0, sizeof(kdc_reply));

    ret = get_init_creds_common(context, client, start_time,
				in_tkt_service, options, &ctx);
    if (ret)
	goto out;

    done = 0;
    while(!done) {
	memset(&kdc_reply, 0, sizeof(kdc_reply));

	ret = init_cred_loop(context,
			     options,
			     prompter,
			     data,
			     &ctx,
			     &ctx.cred,
			     &kdc_reply);
	
	switch (ret) {
	case 0 :
	    done = 1;
	    break;
	case KRB5KDC_ERR_KEY_EXPIRED :
	    /* try to avoid recursion */

	    /* don't try to change password where then where none */
	    if (prompter == NULL || ctx.password == NULL)
		goto out;

	    krb5_clear_error_string (context);

	    if (ctx.in_tkt_service != NULL
		&& strcmp (ctx.in_tkt_service, "kadmin/changepw") == 0)
		goto out;

	    ret = change_password (context,
				   client,
				   ctx.password,
				   buf,
				   sizeof(buf),
				   prompter,
				   data,
				   options);
	    if (ret)
		goto out;
	    ctx.password = buf;
	    break;
	default:
	    goto out;
	}
    }

    if (prompter)
	print_expire (context,
		      krb5_principal_get_realm (context, ctx.cred.client),
		      &kdc_reply,
		      prompter,
		      data);

 out:
    memset (buf, 0, sizeof(buf));
    free_init_creds_ctx(context, &ctx);
    krb5_free_kdc_rep (context, &kdc_reply);
    if (ret == 0)
	*creds = ctx.cred;
    else
	krb5_free_cred_contents (context, &ctx.cred);

    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_password(krb5_context context,
			     krb5_creds *creds,
			     krb5_principal client,
			     const char *password,
			     krb5_prompter_fct prompter,
			     void *data,
			     krb5_deltat start_time,
			     const char *in_tkt_service,
			     krb5_get_init_creds_opt *in_options)
{
    krb5_get_init_creds_opt *options;
    char buf[BUFSIZ];
    krb5_error_code ret;

    if (in_options == NULL) {
	const char *realm = krb5_principal_get_realm(context, client);
	ret = krb5_get_init_creds_opt_alloc(context, &options);
	if (ret == 0)
	    krb5_get_init_creds_opt_set_default_flags(context, 
						      NULL, 
						      realm, 
						      options);
    } else
	ret = _krb5_get_init_creds_opt_copy(context, in_options, &options);
    if (ret)
	return ret;

    if (password == NULL &&
	options->opt_private->password == NULL &&
	options->opt_private->pk_init_ctx == NULL)
    {
	krb5_prompt prompt;
	krb5_data password_data;
	char *p, *q;

	krb5_unparse_name (context, client, &p);
	asprintf (&q, "%s's Password: ", p);
	free (p);
	prompt.prompt = q;
	password_data.data   = buf;
	password_data.length = sizeof(buf);
	prompt.hidden = 1;
	prompt.reply  = &password_data;
	prompt.type   = KRB5_PROMPT_TYPE_PASSWORD;

	ret = (*prompter) (context, data, NULL, NULL, 1, &prompt);
	free (q);
	if (ret) {
	    memset (buf, 0, sizeof(buf));
	    krb5_get_init_creds_opt_free(context, options);
	    ret = KRB5_LIBOS_PWDINTR;
	    krb5_clear_error_string (context);
	    return ret;
	}
	password = password_data.data;
    }

    if (options->opt_private->password == NULL) {
	ret = krb5_get_init_creds_opt_set_pa_password(context, options,
						      password, NULL);
	if (ret) {
	    krb5_get_init_creds_opt_free(context, options);
	    memset(buf, 0, sizeof(buf));
	    return ret;
	}
    }

    ret = krb5_get_init_creds(context, creds, client, prompter,
			      data, start_time, in_tkt_service, options);
    krb5_get_init_creds_opt_free(context, options);
    memset(buf, 0, sizeof(buf));
    return ret;
}

static krb5_error_code
init_creds_keyblock_key_proc (krb5_context context,
			      krb5_enctype type,
			      krb5_salt salt,
			      krb5_const_pointer keyseed,
			      krb5_keyblock **key)
{
    return krb5_copy_keyblock (context, keyseed, key);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_keyblock(krb5_context context,
			     krb5_creds *creds,
			     krb5_principal client,
			     krb5_keyblock *keyblock,
			     krb5_deltat start_time,
			     const char *in_tkt_service,
			     krb5_get_init_creds_opt *options)
{
    struct krb5_get_init_creds_ctx ctx;
    krb5_error_code ret;
    
    ret = get_init_creds_common(context, client, start_time,
				in_tkt_service, options, &ctx);
    if (ret)
	goto out;

    ret = krb5_get_in_cred (context,
			    KDCOptions2int(ctx.flags),
			    ctx.addrs,
			    ctx.etypes,
			    ctx.pre_auth_types,
			    NULL,
			    init_creds_keyblock_key_proc,
			    keyblock,
			    NULL,
			    NULL,
			    &ctx.cred,
			    NULL);

    if (ret == 0 && creds)
	*creds = ctx.cred;
    else
	krb5_free_cred_contents (context, &ctx.cred);

 out:
    free_init_creds_ctx(context, &ctx);
    return ret;
}
