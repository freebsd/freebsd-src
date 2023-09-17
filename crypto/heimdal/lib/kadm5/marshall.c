/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska Högskolan
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

#include "kadm5_locl.h"

RCSID("$Id$");
#define CHECK(e) do { if (e) return EINVAL; } while (0)

kadm5_ret_t
kadm5_store_key_data(krb5_storage *sp,
		     krb5_key_data *key)
{
    krb5_data c;
    CHECK(krb5_store_int32(sp, key->key_data_ver));
    CHECK(krb5_store_int32(sp, key->key_data_kvno));
    CHECK(krb5_store_int32(sp, key->key_data_type[0]));
    c.length = key->key_data_length[0];
    c.data = key->key_data_contents[0];
    CHECK(krb5_store_data(sp, c));
    CHECK(krb5_store_int32(sp, key->key_data_type[1]));
    c.length = key->key_data_length[1];
    c.data = key->key_data_contents[1];
    CHECK(krb5_store_data(sp, c));
    return 0;
}

kadm5_ret_t
kadm5_ret_key_data(krb5_storage *sp,
		   krb5_key_data *key)
{
    kadm5_ret_t ret;
    krb5_data c;
    int32_t tmp;
    ret = krb5_ret_int32(sp, &tmp);
    if (ret == 0) {
	key->key_data_ver = tmp;
	ret = krb5_ret_int32(sp, &tmp);
    }
    if (ret == 0) {
	key->key_data_kvno = tmp;
	ret = krb5_ret_int32(sp, &tmp);
    }
    if (ret == 0) {
	key->key_data_type[0] = tmp;
	ret = krb5_ret_data(sp, &c);
    }
    if (ret == 0) {
	key->key_data_length[0] = c.length;
	key->key_data_contents[0] = c.data;
	ret = krb5_ret_int32(sp, &tmp);
    }
    if (ret == 0) {
	key->key_data_type[1] = tmp;
	ret = krb5_ret_data(sp, &c);
    }
    if (ret == 0) {
	key->key_data_length[1] = c.length;
	key->key_data_contents[1] = c.data;
	return 0;
    }
    return KADM5_FAILURE;
}

kadm5_ret_t
kadm5_store_tl_data(krb5_storage *sp,
		    krb5_tl_data *tl)
{
    krb5_data c;
    CHECK(krb5_store_int32(sp, tl->tl_data_type));
    c.length = tl->tl_data_length;
    c.data = tl->tl_data_contents;
    CHECK(krb5_store_data(sp, c));
    return 0;
}

kadm5_ret_t
kadm5_ret_tl_data(krb5_storage *sp,
		  krb5_tl_data *tl)
{
    krb5_data c;
    int32_t tmp;
    krb5_ret_int32(sp, &tmp);
    tl->tl_data_type = tmp;
    CHECK(krb5_ret_data(sp, &c));
    tl->tl_data_length = c.length;
    tl->tl_data_contents = c.data;
    return 0;
}

static kadm5_ret_t
store_principal_ent(krb5_storage *sp,
		    kadm5_principal_ent_t princ,
		    uint32_t mask)
{
    int i;

    if (mask & KADM5_PRINCIPAL)
	CHECK(krb5_store_principal(sp, princ->principal));
    if (mask & KADM5_PRINC_EXPIRE_TIME)
	CHECK(krb5_store_int32(sp, princ->princ_expire_time));
    if (mask & KADM5_PW_EXPIRATION)
	CHECK(krb5_store_int32(sp, princ->pw_expiration));
    if (mask & KADM5_LAST_PWD_CHANGE)
	CHECK(krb5_store_int32(sp, princ->last_pwd_change));
    if (mask & KADM5_MAX_LIFE)
	CHECK(krb5_store_int32(sp, princ->max_life));
    if (mask & KADM5_MOD_NAME) {
	CHECK(krb5_store_int32(sp, princ->mod_name != NULL));
	if(princ->mod_name)
	    CHECK(krb5_store_principal(sp, princ->mod_name));
    }
    if (mask & KADM5_MOD_TIME)
	CHECK(krb5_store_int32(sp, princ->mod_date));
    if (mask & KADM5_ATTRIBUTES)
	CHECK(krb5_store_int32(sp, princ->attributes));
    if (mask & KADM5_KVNO)
	CHECK(krb5_store_int32(sp, princ->kvno));
    if (mask & KADM5_MKVNO)
	CHECK(krb5_store_int32(sp, princ->mkvno));
    if (mask & KADM5_POLICY) {
	CHECK(krb5_store_int32(sp, princ->policy != NULL));
	if(princ->policy)
	    CHECK(krb5_store_string(sp, princ->policy));
    }
    if (mask & KADM5_AUX_ATTRIBUTES)
	CHECK(krb5_store_int32(sp, princ->aux_attributes));
    if (mask & KADM5_MAX_RLIFE)
	CHECK(krb5_store_int32(sp, princ->max_renewable_life));
    if (mask & KADM5_LAST_SUCCESS)
	CHECK(krb5_store_int32(sp, princ->last_success));
    if (mask & KADM5_LAST_FAILED)
	CHECK(krb5_store_int32(sp, princ->last_failed));
    if (mask & KADM5_FAIL_AUTH_COUNT)
	CHECK(krb5_store_int32(sp, princ->fail_auth_count));
    if (mask & KADM5_KEY_DATA) {
	CHECK(krb5_store_int32(sp, princ->n_key_data));
	for(i = 0; i < princ->n_key_data; i++)
	    CHECK(kadm5_store_key_data(sp, &princ->key_data[i]));
    }
    if (mask & KADM5_TL_DATA) {
	krb5_tl_data *tp;

	CHECK(krb5_store_int32(sp, princ->n_tl_data));
	for(tp = princ->tl_data; tp; tp = tp->tl_data_next)
	    CHECK(kadm5_store_tl_data(sp, tp));
    }
    return 0;
}


kadm5_ret_t
kadm5_store_principal_ent(krb5_storage *sp,
			  kadm5_principal_ent_t princ)
{
    return store_principal_ent (sp, princ, ~0);
}

kadm5_ret_t
kadm5_store_principal_ent_mask(krb5_storage *sp,
			       kadm5_principal_ent_t princ,
			       uint32_t mask)
{
    kadm5_ret_t ret;

    ret = krb5_store_int32(sp, mask);
    if (ret == 0)
	ret = store_principal_ent (sp, princ, mask);
    return (ret);
}

static kadm5_ret_t
ret_principal_ent(krb5_storage *sp,
		  kadm5_principal_ent_t princ,
		  uint32_t mask)
{
    int i;
    int32_t tmp;

    if (mask & KADM5_PRINCIPAL) 
	CHECK(krb5_ret_principal(sp, &princ->principal));
    if (mask & KADM5_PRINC_EXPIRE_TIME) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->princ_expire_time = tmp;
    }
    if (mask & KADM5_PW_EXPIRATION) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->pw_expiration = tmp;
    }
    if (mask & KADM5_LAST_PWD_CHANGE) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->last_pwd_change = tmp;
    }
    if (mask & KADM5_MAX_LIFE) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->max_life = tmp;
    }
    if (mask & KADM5_MOD_NAME) {
	CHECK(krb5_ret_int32(sp, &tmp));
	if(tmp)
	    CHECK(krb5_ret_principal(sp, &princ->mod_name));
	else
	    princ->mod_name = NULL;
    }
    if (mask & KADM5_MOD_TIME) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->mod_date = tmp;
    }
    if (mask & KADM5_ATTRIBUTES) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->attributes = tmp;
    }
    if (mask & KADM5_KVNO) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->kvno = tmp;
    }
    if (mask & KADM5_MKVNO) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->mkvno = tmp;
    }
    if (mask & KADM5_POLICY) {
	CHECK(krb5_ret_int32(sp, &tmp));
	if(tmp)
	    CHECK(krb5_ret_string(sp, &princ->policy));
	else
	    princ->policy = NULL;
    }
    if (mask & KADM5_AUX_ATTRIBUTES) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->aux_attributes = tmp;
    }
    if (mask & KADM5_MAX_RLIFE) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->max_renewable_life = tmp;
    }
    if (mask & KADM5_LAST_SUCCESS) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->last_success = tmp;
    }
    if (mask & KADM5_LAST_FAILED) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->last_failed = tmp;
    }
    if (mask & KADM5_FAIL_AUTH_COUNT) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->fail_auth_count = tmp;
    }
    if (mask & KADM5_KEY_DATA) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->n_key_data = tmp;
	princ->key_data = malloc(princ->n_key_data * sizeof(*princ->key_data));
	if (princ->key_data == NULL && princ->n_key_data != 0)
	    return ENOMEM;
	for(i = 0; i < princ->n_key_data; i++)
	    CHECK(kadm5_ret_key_data(sp, &princ->key_data[i]));
    }
    if (mask & KADM5_TL_DATA) {
	CHECK(krb5_ret_int32(sp, &tmp));
	princ->n_tl_data = tmp;
	princ->tl_data = NULL;
	for(i = 0; i < princ->n_tl_data; i++){
	    krb5_tl_data *tp = malloc(sizeof(*tp));
	    if (tp == NULL)
		return ENOMEM;
	    CHECK(kadm5_ret_tl_data(sp, tp));
	    tp->tl_data_next = princ->tl_data;
	    princ->tl_data = tp;
	}
    }
    return 0;
}

kadm5_ret_t
kadm5_ret_principal_ent(krb5_storage *sp,
			kadm5_principal_ent_t princ)
{
    return ret_principal_ent (sp, princ, ~0);
}

kadm5_ret_t
kadm5_ret_principal_ent_mask(krb5_storage *sp,
			     kadm5_principal_ent_t princ,
			     uint32_t *mask)
{
    kadm5_ret_t ret;
    int32_t tmp;

    ret = krb5_ret_int32 (sp, &tmp);
    if (ret) {
	*mask = 0;
	return (ret);
    }
    *mask = tmp;
    return ret_principal_ent (sp, princ, *mask);
}

kadm5_ret_t
_kadm5_marshal_params(krb5_context context,
		      kadm5_config_params *params,
		      krb5_data *out)
{
    kadm5_ret_t ret;

    krb5_storage *sp = krb5_storage_emem();

    ret = krb5_store_int32(sp, params->mask & (KADM5_CONFIG_REALM));

    if (ret == 0 && params->mask & KADM5_CONFIG_REALM)
	ret = krb5_store_string(sp, params->realm);
    if (ret == 0)
	krb5_storage_to_data(sp, out);
    krb5_storage_free(sp);

    return (ret);
}

kadm5_ret_t
_kadm5_unmarshal_params(krb5_context context,
			krb5_data *in,
			kadm5_config_params *params)
{
    krb5_error_code ret;
    krb5_storage *sp;
    int32_t mask;

    sp = krb5_storage_from_data(in);
    if (sp == NULL)
	return ENOMEM;

    ret = krb5_ret_int32(sp, &mask);
    if (ret)
	goto out;
    if (mask & KADM5_CONFIG_REALM & KADM5_CONFIG_DBNAME
	& KADM5_CONFIG_ACL_FILE & KADM5_CONFIG_STASH_FILE) {
	    ret = EINVAL;
	    goto out;
    }
    params->mask = mask;

    if (params->mask & KADM5_CONFIG_REALM) {
	ret = krb5_ret_string(sp, &params->realm);
	if (params->realm == NULL) {
	    ret = EINVAL;
	    goto out;
	}
    }
    if (params->mask & KADM5_CONFIG_DBNAME) {
	ret = krb5_ret_string(sp, &params->dbname);
	if (params->dbname == NULL) {
	    ret = EINVAL;
	    goto out;
	}
    }
    if (params->mask & KADM5_CONFIG_ACL_FILE) {
	ret = krb5_ret_string(sp, &params->acl_file);
	if (params->acl_file == NULL) {
	    ret = EINVAL;
	    goto out;
	}
    }
    if (params->mask & KADM5_CONFIG_STASH_FILE) {
	ret = krb5_ret_string(sp, &params->stash_file);
	if (params->stash_file == NULL) {
	    ret = EINVAL;
	}
    }
 out:
    krb5_storage_free(sp);

    return ret;
}
