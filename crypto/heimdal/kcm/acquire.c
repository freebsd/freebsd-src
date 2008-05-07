/*
 * Copyright (c) 2005, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kcm_locl.h"

RCSID("$Id: acquire.c 22118 2007-12-03 21:44:00Z lha $");

static krb5_error_code
change_pw_and_update_keytab(krb5_context context, kcm_ccache ccache);

/*
 * Get a new ticket using a keytab/cached key and swap it into
 * an existing redentials cache
 */

krb5_error_code
kcm_ccache_acquire(krb5_context context,
		   kcm_ccache ccache,
		   krb5_creds **credp)
{
    krb5_error_code ret = 0;
    krb5_creds cred;
    krb5_const_realm realm;
    krb5_get_init_creds_opt opt;
    krb5_ccache_data ccdata;
    char *in_tkt_service = NULL;
    int done = 0;

    memset(&cred, 0, sizeof(cred));

    KCM_ASSERT_VALID(ccache);

    /* We need a cached key or keytab to acquire credentials */
    if (ccache->flags & KCM_FLAGS_USE_CACHED_KEY) {
	if (ccache->key.keyblock.keyvalue.length == 0)
	    krb5_abortx(context,
			"kcm_ccache_acquire: KCM_FLAGS_USE_CACHED_KEY without key");
    } else if (ccache->flags & KCM_FLAGS_USE_KEYTAB) {
	if (ccache->key.keytab == NULL)
	    krb5_abortx(context,
			"kcm_ccache_acquire: KCM_FLAGS_USE_KEYTAB without keytab");
    } else {
	kcm_log(0, "Cannot acquire initial credentials for cache %s without key",
		ccache->name);
	return KRB5_FCC_INTERNAL;
    }
	
    HEIMDAL_MUTEX_lock(&ccache->mutex);

    /* Fake up an internal ccache */
    kcm_internal_ccache(context, ccache, &ccdata);

    /* Now, actually acquire the creds */
    if (ccache->server != NULL) {
	ret = krb5_unparse_name(context, ccache->server, &in_tkt_service);
	if (ret) {
	    kcm_log(0, "Failed to unparse service principal name for cache %s: %s",
		    ccache->name, krb5_get_err_text(context, ret));
	    return ret;
	}
    }

    realm = krb5_principal_get_realm(context, ccache->client);

    krb5_get_init_creds_opt_init(&opt);
    krb5_get_init_creds_opt_set_default_flags(context, "kcm", realm, &opt);
    if (ccache->tkt_life != 0)
	krb5_get_init_creds_opt_set_tkt_life(&opt, ccache->tkt_life);
    if (ccache->renew_life != 0)
	krb5_get_init_creds_opt_set_renew_life(&opt, ccache->renew_life);

    if (ccache->flags & KCM_FLAGS_USE_CACHED_KEY) {
	ret = krb5_get_init_creds_keyblock(context,
					   &cred,
					   ccache->client,
					   &ccache->key.keyblock,
					   0,
					   in_tkt_service,
					   &opt);
    } else {
	/* loosely based on lib/krb5/init_creds_pw.c */
	while (!done) {
	    ret = krb5_get_init_creds_keytab(context,
					     &cred,
					     ccache->client,
					     ccache->key.keytab,
					     0,
					     in_tkt_service,
					     &opt);
	    switch (ret) {
	    case KRB5KDC_ERR_KEY_EXPIRED:
		if (in_tkt_service != NULL &&
		    strcmp(in_tkt_service, "kadmin/changepw") == 0) {
		    goto out;
		}

		ret = change_pw_and_update_keytab(context, ccache);
		if (ret)
		    goto out;
		break;
	    case 0:
	    default:
		done = 1;
		break;
	    }
	}
    }

    if (ret) {
	kcm_log(0, "Failed to acquire credentials for cache %s: %s",
		ccache->name, krb5_get_err_text(context, ret));
	if (in_tkt_service != NULL)
	    free(in_tkt_service);
	goto out;
    }

    if (in_tkt_service != NULL)
	free(in_tkt_service);

    /* Swap them in */
    kcm_ccache_remove_creds_internal(context, ccache);

    ret = kcm_ccache_store_cred_internal(context, ccache, &cred, 0, credp);
    if (ret) {
	kcm_log(0, "Failed to store credentials for cache %s: %s",
		ccache->name, krb5_get_err_text(context, ret));
	krb5_free_cred_contents(context, &cred);
	goto out;
    }

out:
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

static krb5_error_code
change_pw(krb5_context context,
	  kcm_ccache ccache,
	  char *cpn,
	  char *newpw)
{
    krb5_error_code ret;
    krb5_creds cpw_cred;
    int result_code;
    krb5_data result_code_string;
    krb5_data result_string;
    krb5_get_init_creds_opt options;

    memset(&cpw_cred, 0, sizeof(cpw_cred));

    krb5_get_init_creds_opt_init(&options);
    krb5_get_init_creds_opt_set_tkt_life(&options, 60);
    krb5_get_init_creds_opt_set_forwardable(&options, FALSE);
    krb5_get_init_creds_opt_set_proxiable(&options, FALSE);

    krb5_data_zero(&result_code_string);
    krb5_data_zero(&result_string);

    ret = krb5_get_init_creds_keytab(context,
				     &cpw_cred,
				     ccache->client,
				     ccache->key.keytab,
				     0,
				     "kadmin/changepw",
				     &options);
    if (ret) {
	kcm_log(0, "Failed to acquire password change credentials "
		"for principal %s: %s", 
		cpn, krb5_get_err_text(context, ret));
	goto out;
    }

    ret = krb5_set_password(context,
			    &cpw_cred,
			    newpw,
			    ccache->client,
			    &result_code,
			    &result_code_string,
			    &result_string);
    if (ret) {
	kcm_log(0, "Failed to change password for principal %s: %s",
		cpn, krb5_get_err_text(context, ret));
	goto out;
    }

    if (result_code) {
	kcm_log(0, "Failed to change password for principal %s: %.*s",
		cpn,
		(int)result_string.length,
		result_string.length > 0 ? (char *)result_string.data : "");
	goto out;
    }

out:
    krb5_data_free(&result_string);
    krb5_data_free(&result_code_string);
    krb5_free_cred_contents(context, &cpw_cred);

    return ret;
}

struct kcm_keyseed_data {
    krb5_salt salt;
    const char *password;
};

static krb5_error_code
kcm_password_key_proc(krb5_context context,
		      krb5_enctype etype,
		      krb5_salt salt,
		      krb5_const_pointer keyseed,
		      krb5_keyblock **key)
{
    krb5_error_code ret;
    struct kcm_keyseed_data *s = (struct kcm_keyseed_data *)keyseed;

    /* we may be called multiple times */
    krb5_free_salt(context, s->salt);
    krb5_data_zero(&s->salt.saltvalue);

    /* stash the salt */
    s->salt.salttype = salt.salttype;

    ret = krb5_data_copy(&s->salt.saltvalue,
		         salt.saltvalue.data,
			 salt.saltvalue.length);
    if (ret)
	return ret;

    *key = (krb5_keyblock *)malloc(sizeof(**key));
    if (*key == NULL) {
	return ENOMEM;
    }

    ret = krb5_string_to_key_salt(context, etype, s->password,
				  s->salt, *key);
    if (ret) {
	free(*key);
	*key = NULL;
    }

    return ret;
}

static krb5_error_code
get_salt_and_kvno(krb5_context context,
		  kcm_ccache ccache,
		  krb5_enctype *etypes,
		  char *cpn,
		  char *newpw,
		  krb5_salt *salt,
		  unsigned *kvno)
{
    krb5_error_code ret;
    krb5_creds creds;
    krb5_ccache_data ccdata;
    krb5_flags options = 0;
    krb5_kdc_rep reply;
    struct kcm_keyseed_data s;

    memset(&creds, 0, sizeof(creds));
    memset(&reply, 0, sizeof(reply));

    s.password = NULL;
    s.salt.salttype = (int)ETYPE_NULL;
    krb5_data_zero(&s.salt.saltvalue);

    *kvno = 0;
    kcm_internal_ccache(context, ccache, &ccdata);
    s.password = newpw;

    /* Do an AS-REQ to determine salt and key version number */
    ret = krb5_copy_principal(context, ccache->client, &creds.client);
    if (ret)
	return ret;

    /* Yes, get a ticket to ourselves */
    ret = krb5_copy_principal(context, ccache->client, &creds.server);
    if (ret) {
	krb5_free_principal(context, creds.client);
	return ret;
    }
	
    ret = krb5_get_in_tkt(context,
			  options,
			  NULL,
			  etypes,
			  NULL,
			  kcm_password_key_proc,
			  &s,
			  NULL,
			  NULL,
			  &creds,
			  &ccdata,
			  &reply);
    if (ret) {
	kcm_log(0, "Failed to get self ticket for principal %s: %s",
		cpn, krb5_get_err_text(context, ret));
	krb5_free_salt(context, s.salt);
    } else {
	*salt = s.salt; /* retrieve stashed salt */
	if (reply.kdc_rep.enc_part.kvno != NULL)
	    *kvno = *(reply.kdc_rep.enc_part.kvno);
    }
    /* ccache may have been modified but it will get trashed anyway */

    krb5_free_cred_contents(context, &creds);
    krb5_free_kdc_rep(context, &reply);

    return ret;
}

static krb5_error_code
update_keytab_entry(krb5_context context,
		    kcm_ccache ccache,
		    krb5_enctype etype,
		    char *cpn,
		    char *spn,
		    char *newpw,
		    krb5_salt salt,
		    unsigned kvno)
{
    krb5_error_code ret;
    krb5_keytab_entry entry;
    krb5_data pw;

    memset(&entry, 0, sizeof(entry));

    pw.data = (char *)newpw;
    pw.length = strlen(newpw);

    ret = krb5_string_to_key_data_salt(context, etype, pw,
				       salt, &entry.keyblock);
    if (ret) {
	kcm_log(0, "String to key conversion failed for principal %s "
		"and etype %d: %s",
		cpn, etype, krb5_get_err_text(context, ret)); 
	return ret;
    }

    if (spn == NULL) {
	ret = krb5_copy_principal(context, ccache->client,
				  &entry.principal);
	if (ret) {
	    kcm_log(0, "Failed to copy principal name %s: %s",
		    cpn, krb5_get_err_text(context, ret));
	    return ret;
	}
    } else {
	ret = krb5_parse_name(context, spn, &entry.principal);
	if (ret) {
	    kcm_log(0, "Failed to parse SPN alias %s: %s",
		    spn, krb5_get_err_text(context, ret));
	    return ret;
	}
    }

    entry.vno = kvno;
    entry.timestamp = time(NULL);

    ret = krb5_kt_add_entry(context, ccache->key.keytab, &entry);
    if (ret) {
	kcm_log(0, "Failed to update keytab for principal %s "
		"and etype %d: %s",
		cpn, etype, krb5_get_err_text(context, ret));
    }

    krb5_kt_free_entry(context, &entry);

    return ret; 
}

static krb5_error_code
update_keytab_entries(krb5_context context,
		      kcm_ccache ccache,
		      krb5_enctype *etypes,
		      char *cpn,
		      char *spn,
		      char *newpw,
		      krb5_salt salt,
		      unsigned kvno)
{
    krb5_error_code ret = 0;
    int i;

    for (i = 0; etypes[i] != ETYPE_NULL; i++) {
	ret = update_keytab_entry(context, ccache, etypes[i],
				  cpn, spn, newpw, salt, kvno);
	if (ret)
	    break;
    }

    return ret;
}

static void
generate_random_pw(krb5_context context,
		   char *buf,
		   size_t bufsiz)
{
    unsigned char x[512], *p;
    size_t i;

    memset(x, 0, sizeof(x));
    krb5_generate_random_block(x, sizeof(x));
    p = x;

    for (i = 0; i < bufsiz; i++) {
	while (isprint(*p) == 0)
	    p++;

	if (p - x >= sizeof(x)) {
	    krb5_generate_random_block(x, sizeof(x));
	    p = x;
	}
	buf[i] = (char)*p++;
    }
    buf[bufsiz - 1] = '\0';
    memset(x, 0, sizeof(x));
}

static krb5_error_code
change_pw_and_update_keytab(krb5_context context,
			    kcm_ccache ccache)
{
    char newpw[121];
    krb5_error_code ret;
    unsigned kvno;
    krb5_salt salt;
    krb5_enctype *etypes = NULL;
    int i;
    char *cpn = NULL;
    char **spns = NULL;

    krb5_data_zero(&salt.saltvalue);

    ret = krb5_unparse_name(context, ccache->client, &cpn);
    if (ret) {
	kcm_log(0, "Failed to unparse name: %s",
		krb5_get_err_text(context, ret));
	goto out;
    }

    ret = krb5_get_default_in_tkt_etypes(context, &etypes);
    if (ret) {
	kcm_log(0, "Failed to determine default encryption types: %s",
		krb5_get_err_text(context, ret));
	goto out;
    }

    /* Generate a random password (there is no set keys protocol) */
    generate_random_pw(context, newpw, sizeof(newpw));

    /* Change it */
    ret = change_pw(context, ccache, cpn, newpw);
    if (ret)
	goto out;

    /* Do an AS-REQ to determine salt and key version number */
    ret = get_salt_and_kvno(context, ccache, etypes, cpn, newpw,
			    &salt, &kvno);
    if (ret) {
	kcm_log(0, "Failed to determine salting principal for principal %s: %s",
		cpn, krb5_get_err_text(context, ret));
	goto out;
    }

    /* Add canonical name */
    ret = update_keytab_entries(context, ccache, etypes, cpn,
				NULL, newpw, salt, kvno);
    if (ret)
	goto out;

    /* Add SPN aliases, if any */
    spns = krb5_config_get_strings(context, NULL, "kcm",
				   "system_ccache", "spn_aliases", NULL);
    if (spns != NULL) {
	for (i = 0; spns[i] != NULL; i++) {
	    ret = update_keytab_entries(context, ccache, etypes, cpn,
					spns[i], newpw, salt, kvno);
	    if (ret)
		goto out;
	}
    }

    kcm_log(0, "Changed expired password for principal %s in cache %s",
	    cpn, ccache->name);

out:
    if (cpn != NULL)
	free(cpn);
    if (spns != NULL)
	krb5_config_free_strings(spns);
    if (etypes != NULL)
	free(etypes);
    krb5_free_salt(context, salt);
    memset(newpw, 0, sizeof(newpw));

    return ret;
}

