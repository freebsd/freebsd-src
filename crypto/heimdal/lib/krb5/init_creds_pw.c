/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

RCSID("$Id: init_creds_pw.c,v 1.51 2001/09/18 09:36:39 joda Exp $");

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
    krb5_realm *client_realm;
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

    client_realm = krb5_princ_realm (context, cred->client);

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
	server_realm = strdup (*client_realm);
	free (*krb5_princ_realm(context, cred->server));
	krb5_princ_set_realm (context, cred->server, &server_realm);
    } else {
	ret = krb5_make_principal(context, &cred->server, 
				  *client_realm, KRB5_TGS_NAME, *client_realm,
				  NULL);
	if (ret)
	    goto out;
    }
    return 0;

out:
    krb5_free_creds_contents (context, cred);
    return ret;
}

/*
 * Parse the last_req data and show it to the user if it's interesting
 */

static void
print_expire (krb5_context context,
	      krb5_realm *realm,
	      krb5_kdc_rep *rep,
	      krb5_prompter_fct prompter,
	      krb5_data *data)
{
    int i;
    LastReq *lr = &rep->enc_part.last_req;
    krb5_timestamp sec;
    time_t t;

    krb5_timeofday (context, &sec);

    t = sec + get_config_time (context,
			       *realm,
			       "warn_pwexpire",
			       7 * 24 * 60 * 60);

    for (i = 0; i < lr->len; ++i) {
	if (abs(lr->val[i].lr_type) == LR_PW_EXPTIME
	    && lr->val[i].lr_value <= t) {
	    char *p;
	    time_t tmp = lr->val[i].lr_value;
	    
	    asprintf (&p, "Your password will expire at %s", ctime(&tmp));
	    (*prompter) (context, data, NULL, p, 0, NULL);
	    free (p);
	    return;
	}
    }

    if (rep->enc_part.key_expiration
	&& *rep->enc_part.key_expiration <= t) {
	char *p;
	time_t t = *rep->enc_part.key_expiration;

	asprintf (&p, "Your password/account will expire at %s", ctime(&t));
	(*prompter) (context, data, NULL, p, 0, NULL);
	free (p);
    }
}

static krb5_error_code
get_init_creds_common(krb5_context context,
		      krb5_creds *creds,
		      krb5_principal client,
		      krb5_deltat start_time,
		      const char *in_tkt_service,
		      krb5_get_init_creds_opt *options,
		      krb5_addresses **addrs,
		      krb5_enctype **etypes,
		      krb5_creds *cred,
		      krb5_preauthtype **pre_auth_types,
		      krb5_kdc_flags *flags)
{
    krb5_error_code ret;
    krb5_realm *client_realm;
    krb5_get_init_creds_opt default_opt;

    if (options == NULL) {
	krb5_get_init_creds_opt_init (&default_opt);
	options = &default_opt;
    }

    ret = init_cred (context, cred, client, start_time,
		     in_tkt_service, options);
    if (ret)
	return ret;

    client_realm = krb5_princ_realm (context, cred->client);

    flags->i = 0;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)
	flags->b.forwardable = options->forwardable;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE)
	flags->b.proxiable = options->proxiable;

    if (start_time)
	flags->b.postdated = 1;
    if (cred->times.renew_till)
	flags->b.renewable = 1;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST)
	*addrs = options->address_list;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST) {
	*etypes = malloc((options->etype_list_length + 1)
			* sizeof(krb5_enctype));
	if (*etypes == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	memcpy (*etypes, options->etype_list,
		options->etype_list_length * sizeof(krb5_enctype));
	(*etypes)[options->etype_list_length] = ETYPE_NULL;
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST) {
	*pre_auth_types = malloc((options->preauth_list_length + 1)
				 * sizeof(krb5_preauthtype));
	if (*pre_auth_types == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	memcpy (*pre_auth_types, options->preauth_list,
		options->preauth_list_length * sizeof(krb5_preauthtype));
	(*pre_auth_types)[options->preauth_list_length] = KRB5_PADATA_NONE;
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_SALT)
	;			/* XXX */
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ANONYMOUS)
	flags->b.request_anonymous = options->anonymous;
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
    if (old_options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST)
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
	      (char*)result_string.data);

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
    krb5_free_creds_contents (context, &cpw_cred);
    return ret;
}

krb5_error_code
krb5_get_init_creds_password(krb5_context context,
			     krb5_creds *creds,
			     krb5_principal client,
			     const char *password,
			     krb5_prompter_fct prompter,
			     void *data,
			     krb5_deltat start_time,
			     const char *in_tkt_service,
			     krb5_get_init_creds_opt *options)
{
    krb5_error_code ret;
    krb5_kdc_flags flags;
    krb5_addresses *addrs = NULL;
    krb5_enctype *etypes = NULL;
    krb5_preauthtype *pre_auth_types = NULL;
    krb5_creds this_cred;
    krb5_kdc_rep kdc_reply;
    char buf[BUFSIZ];
    krb5_data password_data;
    int done;

    ret = get_init_creds_common(context, creds, client, start_time,
				in_tkt_service, options,
				&addrs, &etypes, &this_cred, &pre_auth_types,
				&flags);
    if(ret)
	goto out;

    if (password == NULL) {
	krb5_prompt prompt;
	char *p;

	krb5_unparse_name (context, this_cred.client, &p);
	asprintf (&prompt.prompt, "%s's Password: ", p);
	free (p);
	password_data.data   = buf;
	password_data.length = sizeof(buf);
	prompt.hidden = 1;
	prompt.reply  = &password_data;
	prompt.type   = KRB5_PROMPT_TYPE_PASSWORD;

	ret = (*prompter) (context, data, NULL, NULL, 1, &prompt);
	free (prompt.prompt);
	if (ret) {
	    memset (buf, 0, sizeof(buf));
	    ret = KRB5_LIBOS_PWDINTR;
	    krb5_clear_error_string (context);
	    goto out;
	}
	password = password_data.data;
    }

    done = 0;
    while(!done) {
	memset(&kdc_reply, 0, sizeof(kdc_reply));
	ret = krb5_get_in_cred (context,
				flags.i,
				addrs,
				etypes,
				pre_auth_types,
				NULL,
				krb5_password_key_proc,
				password,
				NULL,
				NULL,
				&this_cred,
				&kdc_reply);
	switch (ret) {
	case 0 :
	    done = 1;
	    break;
	case KRB5KDC_ERR_KEY_EXPIRED :
	    /* try to avoid recursion */

	    krb5_clear_error_string (context);

	    if (in_tkt_service != NULL
		&& strcmp (in_tkt_service, "kadmin/changepw") == 0)
		goto out;

	    ret = change_password (context,
				   client,
				   password,
				   buf,
				   sizeof(buf),
				   prompter,
				   data,
				   options);
	    if (ret)
		goto out;
	    password = buf;
	    break;
	default:
	    goto out;
	}
    }

    if (prompter)
	print_expire (context,
		      krb5_princ_realm (context, this_cred.client),
		      &kdc_reply,
		      prompter,
		      data);
out:
    memset (buf, 0, sizeof(buf));
    if (ret == 0)
	krb5_free_kdc_rep (context, &kdc_reply);

    free (pre_auth_types);
    free (etypes);
    if (ret == 0 && creds)
	*creds = this_cred;
    else
	krb5_free_creds_contents (context, &this_cred);
    return ret;
}

krb5_error_code
krb5_keyblock_key_proc (krb5_context context,
			krb5_keytype type,
			krb5_data *salt,
			krb5_const_pointer keyseed,
			krb5_keyblock **key)
{
    return krb5_copy_keyblock (context, keyseed, key);
}

krb5_error_code
krb5_get_init_creds_keytab(krb5_context context,
			   krb5_creds *creds,
			   krb5_principal client,
			   krb5_keytab keytab,
			   krb5_deltat start_time,
			   const char *in_tkt_service,
			   krb5_get_init_creds_opt *options)
{
    krb5_error_code ret;
    krb5_kdc_flags flags;
    krb5_addresses *addrs = NULL;
    krb5_enctype *etypes = NULL;
    krb5_preauthtype *pre_auth_types = NULL;
    krb5_creds this_cred;
    krb5_keytab_key_proc_args *a;
    
    ret = get_init_creds_common(context, creds, client, start_time,
				in_tkt_service, options,
				&addrs, &etypes, &this_cred, &pre_auth_types,
				&flags);
    if(ret)
	goto out;

    a = malloc (sizeof(*a));
    if (a == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    a->principal = this_cred.client;
    a->keytab    = keytab;

    ret = krb5_get_in_cred (context,
			    flags.i,
			    addrs,
			    etypes,
			    pre_auth_types,
			    NULL,
			    krb5_keytab_key_proc,
			    a,
			    NULL,
			    NULL,
			    &this_cred,
			    NULL);
    free (a);

    if (ret)
	goto out;
    free (pre_auth_types);
    free (etypes);
    if (creds)
	*creds = this_cred;
    else
	krb5_free_creds_contents (context, &this_cred);
    return 0;

out:
    free (pre_auth_types);
    free (etypes);
    krb5_free_creds_contents (context, &this_cred);
    return ret;
}
