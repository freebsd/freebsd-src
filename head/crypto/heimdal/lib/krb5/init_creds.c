/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
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

RCSID("$Id: init_creds.c 21711 2007-07-27 14:22:02Z lha $");

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_init(krb5_get_init_creds_opt *opt)
{
    memset (opt, 0, sizeof(*opt));
    opt->flags = 0;
    opt->opt_private = NULL;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_alloc(krb5_context context, 
			      krb5_get_init_creds_opt **opt)
{
    krb5_get_init_creds_opt *o;
    
    *opt = NULL;
    o = calloc(1, sizeof(*o));
    if (o == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    krb5_get_init_creds_opt_init(o);
    o->opt_private = calloc(1, sizeof(*o->opt_private));
    if (o->opt_private == NULL) {
	krb5_set_error_string(context, "out of memory");
	free(o);
	return ENOMEM;
    }
    o->opt_private->refcount = 1;
    *opt = o;
    return 0;
}

krb5_error_code
_krb5_get_init_creds_opt_copy(krb5_context context, 
			      const krb5_get_init_creds_opt *in,
			      krb5_get_init_creds_opt **out)
{
    krb5_get_init_creds_opt *opt;

    *out = NULL;
    opt = calloc(1, sizeof(*opt));
    if (opt == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    if (in)
	*opt = *in;
    if(opt->opt_private == NULL) {
	opt->opt_private = calloc(1, sizeof(*opt->opt_private));
	if (opt->opt_private == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    free(opt);
	    return ENOMEM;
	}
	opt->opt_private->refcount = 1;
    } else
	opt->opt_private->refcount++;
    *out = opt;
    return 0;
}

void KRB5_LIB_FUNCTION
_krb5_get_init_creds_opt_free_krb5_error(krb5_get_init_creds_opt *opt)
{
    if (opt->opt_private == NULL || opt->opt_private->error == NULL)
	return;
    free_KRB_ERROR(opt->opt_private->error);
    free(opt->opt_private->error);
    opt->opt_private->error = NULL;
}

void KRB5_LIB_FUNCTION
_krb5_get_init_creds_opt_set_krb5_error(krb5_context context,
					krb5_get_init_creds_opt *opt, 
					const KRB_ERROR *error)
{
    krb5_error_code ret;

    if (opt->opt_private == NULL)
	return;

    _krb5_get_init_creds_opt_free_krb5_error(opt);

    opt->opt_private->error = malloc(sizeof(*opt->opt_private->error));
    if (opt->opt_private->error == NULL)
	return;
    ret = copy_KRB_ERROR(error, opt->opt_private->error);
    if (ret) {
	free(opt->opt_private->error);
	opt->opt_private->error = NULL;
    }	
}


void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_free(krb5_context context,
			     krb5_get_init_creds_opt *opt)
{
    if (opt == NULL || opt->opt_private == NULL)
	return;
    if (opt->opt_private->refcount < 1) /* abort ? */
	return;
    if (--opt->opt_private->refcount == 0) {
	_krb5_get_init_creds_opt_free_krb5_error(opt);
	_krb5_get_init_creds_opt_free_pkinit(opt);
	free(opt->opt_private);
    }
    memset(opt, 0, sizeof(*opt));
    free(opt);
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

static krb5_boolean
get_config_bool (krb5_context context,
		 const char *realm,
		 const char *name)
{
    return krb5_config_get_bool (context,
				 NULL,
				 "realms",
				 realm,
				 name,
				 NULL)
	|| krb5_config_get_bool (context,
				 NULL,
				 "libdefaults",
				 name,
				 NULL);
}

/*
 * set all the values in `opt' to the appropriate values for
 * application `appname' (default to getprogname() if NULL), and realm
 * `realm'.  First looks in [appdefaults] but falls back to
 * [realms] or [libdefaults] for some of the values.
 */

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_default_flags(krb5_context context,
					  const char *appname,
					  krb5_const_realm realm,
					  krb5_get_init_creds_opt *opt)
{
    krb5_boolean b;
    time_t t;

    b = get_config_bool (context, realm, "forwardable");
    krb5_appdefault_boolean(context, appname, realm, "forwardable", b, &b);
    krb5_get_init_creds_opt_set_forwardable(opt, b);

    b = get_config_bool (context, realm, "proxiable");
    krb5_appdefault_boolean(context, appname, realm, "proxiable", b, &b);
    krb5_get_init_creds_opt_set_proxiable (opt, b);

    krb5_appdefault_time(context, appname, realm, "ticket_lifetime", 0, &t);
    if (t == 0)
	t = get_config_time (context, realm, "ticket_lifetime", 0);
    if(t != 0)
	krb5_get_init_creds_opt_set_tkt_life(opt, t);

    krb5_appdefault_time(context, appname, realm, "renew_lifetime", 0, &t);
    if (t == 0)
	t = get_config_time (context, realm, "renew_lifetime", 0);
    if(t != 0)
	krb5_get_init_creds_opt_set_renew_life(opt, t);

    krb5_appdefault_boolean(context, appname, realm, "no-addresses", 
			    KRB5_ADDRESSLESS_DEFAULT, &b);
    krb5_get_init_creds_opt_set_addressless (context, opt, b);

#if 0
    krb5_appdefault_boolean(context, appname, realm, "anonymous", FALSE, &b);
    krb5_get_init_creds_opt_set_anonymous (opt, b);

    krb5_get_init_creds_opt_set_etype_list(opt, enctype,
					   etype_str.num_strings);

    krb5_get_init_creds_opt_set_salt(krb5_get_init_creds_opt *opt,
				     krb5_data *salt);

    krb5_get_init_creds_opt_set_preauth_list(krb5_get_init_creds_opt *opt,
					     krb5_preauthtype *preauth_list,
					     int preauth_list_length);
#endif
}


void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_tkt_life(krb5_get_init_creds_opt *opt,
				     krb5_deltat tkt_life)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
    opt->tkt_life = tkt_life;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_renew_life(krb5_get_init_creds_opt *opt,
				       krb5_deltat renew_life)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE;
    opt->renew_life = renew_life;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_forwardable(krb5_get_init_creds_opt *opt,
					int forwardable)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_FORWARDABLE;
    opt->forwardable = forwardable;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_proxiable(krb5_get_init_creds_opt *opt,
				      int proxiable)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_PROXIABLE;
    opt->proxiable = proxiable;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_etype_list(krb5_get_init_creds_opt *opt,
				       krb5_enctype *etype_list,
				       int etype_list_length)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST;
    opt->etype_list = etype_list;
    opt->etype_list_length = etype_list_length;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_address_list(krb5_get_init_creds_opt *opt,
					 krb5_addresses *addresses)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST;
    opt->address_list = addresses;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_preauth_list(krb5_get_init_creds_opt *opt,
					 krb5_preauthtype *preauth_list,
					 int preauth_list_length)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST;
    opt->preauth_list_length = preauth_list_length;
    opt->preauth_list = preauth_list;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_salt(krb5_get_init_creds_opt *opt,
				 krb5_data *salt)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_SALT;
    opt->salt = salt;
}

void KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_anonymous(krb5_get_init_creds_opt *opt,
				      int anonymous)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ANONYMOUS;
    opt->anonymous = anonymous;
}

static krb5_error_code
require_ext_opt(krb5_context context,
		krb5_get_init_creds_opt *opt,
		const char *type)
{
    if (opt->opt_private == NULL) {
	krb5_set_error_string(context, "%s on non extendable opt", type);
	return EINVAL;
    }
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_pa_password(krb5_context context,
					krb5_get_init_creds_opt *opt,
					const char *password,
					krb5_s2k_proc key_proc)
{
    krb5_error_code ret;
    ret = require_ext_opt(context, opt, "init_creds_opt_set_pa_password");
    if (ret)
	return ret;
    opt->opt_private->password = password;
    opt->opt_private->key_proc = key_proc;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_pac_request(krb5_context context,
					krb5_get_init_creds_opt *opt,
					krb5_boolean req_pac)
{
    krb5_error_code ret;
    ret = require_ext_opt(context, opt, "init_creds_opt_set_pac_req");
    if (ret)
	return ret;
    opt->opt_private->req_pac = req_pac ?
	KRB5_INIT_CREDS_TRISTATE_TRUE :
	KRB5_INIT_CREDS_TRISTATE_FALSE;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_get_error(krb5_context context,
				  krb5_get_init_creds_opt *opt,
				  KRB_ERROR **error)
{
    krb5_error_code ret;

    *error = NULL;

    ret = require_ext_opt(context, opt, "init_creds_opt_get_error");
    if (ret)
	return ret;

    if (opt->opt_private->error == NULL)
	return 0;

    *error = malloc(sizeof(**error));
    if (*error == NULL) {
	krb5_set_error_string(context, "malloc - out memory");
	return ENOMEM;
    }

    ret = copy_KRB_ERROR(opt->opt_private->error, *error);
    if (ret)
	krb5_clear_error_string(context);

    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_addressless(krb5_context context,
					krb5_get_init_creds_opt *opt,
					krb5_boolean addressless)
{
    krb5_error_code ret;
    ret = require_ext_opt(context, opt, "init_creds_opt_set_pac_req");
    if (ret)
	return ret;
    if (addressless)
	opt->opt_private->addressless = KRB5_INIT_CREDS_TRISTATE_TRUE;
    else
	opt->opt_private->addressless = KRB5_INIT_CREDS_TRISTATE_FALSE;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_canonicalize(krb5_context context,
					 krb5_get_init_creds_opt *opt,
					 krb5_boolean req)
{
    krb5_error_code ret;
    ret = require_ext_opt(context, opt, "init_creds_opt_set_canonicalize");
    if (ret)
	return ret;
    if (req)
	opt->opt_private->flags |= KRB5_INIT_CREDS_CANONICALIZE;
    else
	opt->opt_private->flags &= ~KRB5_INIT_CREDS_CANONICALIZE;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_init_creds_opt_set_win2k(krb5_context context,
				  krb5_get_init_creds_opt *opt,
				  krb5_boolean req)
{
    krb5_error_code ret;
    ret = require_ext_opt(context, opt, "init_creds_opt_set_win2k");
    if (ret)
	return ret;
    if (req)
	opt->opt_private->flags |= KRB5_INIT_CREDS_NO_C_CANON_CHECK;
    else
	opt->opt_private->flags &= ~KRB5_INIT_CREDS_NO_C_CANON_CHECK;
    return 0;
}

