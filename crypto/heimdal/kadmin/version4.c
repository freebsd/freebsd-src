/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "kadmin_locl.h"
#include <krb5-private.h>

#define Principal krb4_Principal
#define kadm_get krb4_kadm_get
#undef ALLOC
#include <krb.h>
#include <kadm.h>
#include <krb_err.h>
#include <kadm_err.h>

RCSID("$Id: version4.c,v 1.26.2.1 2002/10/21 14:52:59 joda Exp $");

#define KADM_NO_OPCODE -1
#define KADM_NO_ENCRYPT -2

/*
 * make an error packet if we fail encrypting
 */

static void
make_you_loose_packet(int code, krb5_data *reply)
{
    krb5_data_alloc(reply, KADM_VERSIZE + 4);
    memcpy(reply->data, KADM_ULOSE, KADM_VERSIZE);
    _krb5_put_int((char*)reply->data + KADM_VERSIZE, code, 4);
}

static int
ret_fields(krb5_storage *sp, char *fields)
{
    return krb5_storage_read(sp, fields, FLDSZ);
}

static int
store_fields(krb5_storage *sp, char *fields)
{
    return krb5_storage_write(sp, fields, FLDSZ);
}

static void
ret_vals(krb5_storage *sp, Kadm_vals *vals)
{
    int field;
    char *tmp_string;

    memset(vals, 0, sizeof(*vals));
    
    ret_fields(sp, vals->fields);
    
    for(field = 31; field >= 0; field--) {
	if(IS_FIELD(field, vals->fields)) {
	    switch(field) {
	    case KADM_NAME:
		krb5_ret_stringz(sp, &tmp_string);
		strlcpy(vals->name, tmp_string, sizeof(vals->name));
		free(tmp_string);
		break;
	    case KADM_INST:
		krb5_ret_stringz(sp, &tmp_string);
		strlcpy(vals->instance, tmp_string, 
				sizeof(vals->instance));
		free(tmp_string);
		break;
	    case KADM_EXPDATE:
		krb5_ret_int32(sp, &vals->exp_date);
		break;
	    case KADM_ATTR:
		krb5_ret_int16(sp, &vals->attributes);
		break;
	    case KADM_MAXLIFE:
		krb5_ret_int8(sp, &vals->max_life);
		break;
	    case KADM_DESKEY:
		krb5_ret_int32(sp, &vals->key_high);
		krb5_ret_int32(sp, &vals->key_low);
		break;
#ifdef EXTENDED_KADM
	    case KADM_MODDATE:
		krb5_ret_int32(sp, &vals->mod_date);
		break;
	    case KADM_MODNAME:
		krb5_ret_stringz(sp, &tmp_string);
		strlcpy(vals->mod_name, tmp_string, 
				sizeof(vals->mod_name));
		free(tmp_string);
		break;
	    case KADM_MODINST:
		krb5_ret_stringz(sp, &tmp_string);
		strlcpy(vals->mod_instance, tmp_string, 
				sizeof(vals->mod_instance));
		free(tmp_string);
		break;
	    case KADM_KVNO:
		krb5_ret_int8(sp, &vals->key_version);
		break;
#endif
	    default:
		break;
	    }
	}
    }
}

static void
store_vals(krb5_storage *sp, Kadm_vals *vals)
{
    int field;
    
    store_fields(sp, vals->fields);

    for(field = 31; field >= 0; field--) {
	if(IS_FIELD(field, vals->fields)) {
	    switch(field) {
	    case KADM_NAME:
		krb5_store_stringz(sp, vals->name);
		break;
	    case KADM_INST:
		krb5_store_stringz(sp, vals->instance);
		break;
	    case KADM_EXPDATE:
		krb5_store_int32(sp, vals->exp_date);
		break;
	    case KADM_ATTR:
		krb5_store_int16(sp, vals->attributes);
		break;
	    case KADM_MAXLIFE:
		krb5_store_int8(sp, vals->max_life);
		break;
	    case KADM_DESKEY:
		krb5_store_int32(sp, vals->key_high);
		krb5_store_int32(sp, vals->key_low);
		break;
#ifdef EXTENDED_KADM
	    case KADM_MODDATE:
		krb5_store_int32(sp, vals->mod_date);
		break;
	    case KADM_MODNAME:
		krb5_store_stringz(sp, vals->mod_name);
		break;
	    case KADM_MODINST:
		krb5_store_stringz(sp, vals->mod_instance);
		break;
	    case KADM_KVNO:
		krb5_store_int8(sp, vals->key_version);
		break;
#endif
	    default:
		break;
	    }
	}
    }
}

static int
flags_4_to_5(char *flags)
{
    int i;
    int32_t mask = 0;
    for(i = 31; i >= 0; i--) {
	if(IS_FIELD(i, flags))
	    switch(i) {
	    case KADM_NAME:
	    case KADM_INST:
		mask |= KADM5_PRINCIPAL;
	    case KADM_EXPDATE:
		mask |= KADM5_PRINC_EXPIRE_TIME;
	    case KADM_MAXLIFE:
		mask |= KADM5_MAX_LIFE;
#ifdef EXTENDED_KADM
	    case KADM_KVNO:
		mask |= KADM5_KEY_DATA;
	    case KADM_MODDATE:
		mask |= KADM5_MOD_TIME;
	    case KADM_MODNAME:
	    case KADM_MODINST:
		mask |= KADM5_MOD_NAME;
#endif
	    }
    }
    return mask;
}

static void
ent_to_values(krb5_context context,
	      kadm5_principal_ent_t ent,
	      int32_t mask,
	      Kadm_vals *vals)
{
    krb5_error_code ret;
    char realm[REALM_SZ];
    time_t exp = 0;

    memset(vals, 0, sizeof(*vals));
    if(mask & KADM5_PRINCIPAL) {
	ret = krb5_524_conv_principal(context, ent->principal, 
				      vals->name, vals->instance, realm);
	SET_FIELD(KADM_NAME, vals->fields);
	SET_FIELD(KADM_INST, vals->fields);
    }
    if(mask & KADM5_PRINC_EXPIRE_TIME) {
	if(ent->princ_expire_time != 0)
	    exp = ent->princ_expire_time;
    }
    if(mask & KADM5_PW_EXPIRATION) {
	if(ent->pw_expiration != 0 && (exp == 0 || exp > ent->pw_expiration))
	    exp = ent->pw_expiration;
    }
    if(exp) {
	vals->exp_date = exp;
	SET_FIELD(KADM_EXPDATE, vals->fields);
    }
    if(mask & KADM5_MAX_LIFE) {
	if(ent->max_life == 0)
	    vals->max_life = 255;
	else
	    vals->max_life = krb_time_to_life(0, ent->max_life);
	SET_FIELD(KADM_MAXLIFE, vals->fields);
    }
    if(mask & KADM5_KEY_DATA) {
	if(ent->n_key_data > 0) {
#ifdef EXTENDED_KADM
	vals->key_version = ent->key_data[0].key_data_kvno;
	SET_FIELD(KADM_KVNO, vals->fields);
#endif
	}
	/* XXX the key itself? */
    }
#ifdef EXTENDED_KADM
    if(mask & KADM5_MOD_TIME) {
	vals->mod_date = ent->mod_date;
	SET_FIELD(KADM_MODDATE, vals->fields);
    }
    if(mask & KADM5_MOD_NAME) {
	krb5_524_conv_principal(context, ent->mod_name, 
				vals->mod_name, vals->mod_instance, realm);
	SET_FIELD(KADM_MODNAME, vals->fields);
	SET_FIELD(KADM_MODINST, vals->fields);
    }
#endif
}

/*
 * convert the kadm4 values in `vals' to `ent' (and `mask')
 */

static krb5_error_code
values_to_ent(krb5_context context,
	      Kadm_vals *vals, 
	      kadm5_principal_ent_t ent,
	      int32_t *mask)
{
    krb5_error_code ret;
    *mask = 0;
    memset(ent, 0, sizeof(*ent));
    
    if(IS_FIELD(KADM_NAME, vals->fields)) {
	char *inst = NULL;
	if(IS_FIELD(KADM_INST, vals->fields))
	    inst = vals->instance;
	ret = krb5_425_conv_principal(context, 
				      vals->name,
				      inst,
				      NULL,
				      &ent->principal);
	if(ret)
	    return ret;
	*mask |= KADM5_PRINCIPAL;
    }
    if(IS_FIELD(KADM_EXPDATE, vals->fields)) {
	ent->princ_expire_time = vals->exp_date;
	*mask |= KADM5_PRINC_EXPIRE_TIME;
    }
    if(IS_FIELD(KADM_MAXLIFE, vals->fields)) {
	ent->max_life = krb_life_to_time(0, vals->max_life);
	*mask |= KADM5_MAX_LIFE;
    }
    
    if(IS_FIELD(KADM_DESKEY, vals->fields)) {
	int i;
	ent->key_data = calloc(3, sizeof(*ent->key_data));
	if(ent->key_data == NULL)
	    return ENOMEM;
	for(i = 0; i < 3; i++) {
	    u_int32_t key_low, key_high;

	    ent->key_data[i].key_data_ver = 2;
#ifdef EXTENDED_KADM
	    if(IS_FIELD(KADM_KVNO, vals->fields))
		ent->key_data[i].key_data_kvno = vals->key_version;
#endif
	    ent->key_data[i].key_data_type[0] = ETYPE_DES_CBC_MD5;
	    ent->key_data[i].key_data_length[0] = 8;
	    if((ent->key_data[i].key_data_contents[0] = malloc(8)) == NULL)
		return ENOMEM;

	    key_low  = ntohl(vals->key_low);
	    key_high = ntohl(vals->key_high);
	    memcpy(ent->key_data[i].key_data_contents[0],
		   &key_low, 4);
	    memcpy((char*)ent->key_data[i].key_data_contents[0] + 4,
		   &key_high, 4);
	    ent->key_data[i].key_data_type[1] = KRB5_PW_SALT;
	    ent->key_data[i].key_data_length[1] = 0;
	    ent->key_data[i].key_data_contents[1] = NULL;
	}
	ent->key_data[1].key_data_type[0] = ETYPE_DES_CBC_MD4;
	ent->key_data[2].key_data_type[0] = ETYPE_DES_CBC_CRC;
	ent->n_key_data = 3;
	*mask |= KADM5_KEY_DATA;
    }
    
#ifdef EXTENDED_KADM
    if(IS_FIELD(KADM_MODDATE, vals->fields)) {
	ent->mod_date = vals->mod_date;
	*mask |= KADM5_MOD_TIME;
    }
    if(IS_FIELD(KADM_MODNAME, vals->fields)) {
	char *inst = NULL;
	if(IS_FIELD(KADM_MODINST, vals->fields))
	    inst = vals->mod_instance;
	ret = krb5_425_conv_principal(context, 
				      vals->mod_name,
				      inst,
				      NULL,
				      &ent->mod_name);
	if(ret)
	    return ret;
	*mask |= KADM5_MOD_NAME;
    }
#endif
    return 0;
}

/*
 * Try to translate a KADM5 error code into a v4 kadmin one.
 */

static int
error_code(int ret)
{
    switch (ret) {
    case 0:
	return 0;
    case KADM5_FAILURE :
    case KADM5_AUTH_GET :
    case KADM5_AUTH_ADD :
    case KADM5_AUTH_MODIFY :
    case KADM5_AUTH_DELETE :
    case KADM5_AUTH_INSUFFICIENT :
	return KADM_UNAUTH;
    case KADM5_BAD_DB :
	return KADM_UK_RERROR;
    case KADM5_DUP :
	return KADM_INUSE;
    case KADM5_RPC_ERROR :
    case KADM5_NO_SRV :
	return KADM_NO_SERV;
    case KADM5_NOT_INIT :
	return KADM_NO_CONN;
    case KADM5_UNK_PRINC :
	return KADM_NOENTRY;
    case KADM5_PASS_Q_TOOSHORT :
#ifdef KADM_PASS_Q_TOOSHORT
	return KADM_PASS_Q_TOOSHORT;
#else
	return KADM_INSECURE_PW;
#endif
    case KADM5_PASS_Q_CLASS :
#ifdef KADM_PASS_Q_CLASS
	return KADM_PASS_Q_CLASS;
#else
	return KADM_INSECURE_PW;
#endif
    case KADM5_PASS_Q_DICT :
#ifdef KADM_PASS_Q_DICT
	return KADM_PASS_Q_DICT;
#else
	return KADM_INSECURE_PW;
#endif
    case KADM5_PASS_REUSE :
    case KADM5_PASS_TOOSOON :
    case KADM5_BAD_PASSWORD :
	return KADM_INSECURE_PW;
    case KADM5_PROTECT_PRINCIPAL :
	return KADM_IMMUTABLE;
    case KADM5_POLICY_REF :
    case KADM5_INIT :
    case KADM5_BAD_HIST_KEY :
    case KADM5_UNK_POLICY :
    case KADM5_BAD_MASK :
    case KADM5_BAD_CLASS :
    case KADM5_BAD_LENGTH :
    case KADM5_BAD_POLICY :
    case KADM5_BAD_PRINCIPAL :
    case KADM5_BAD_AUX_ATTR :
    case KADM5_BAD_HISTORY :
    case KADM5_BAD_MIN_PASS_LIFE :
    case KADM5_BAD_SERVER_HANDLE :
    case KADM5_BAD_STRUCT_VERSION :
    case KADM5_OLD_STRUCT_VERSION :
    case KADM5_NEW_STRUCT_VERSION :
    case KADM5_BAD_API_VERSION :
    case KADM5_OLD_LIB_API_VERSION :
    case KADM5_OLD_SERVER_API_VERSION :
    case KADM5_NEW_LIB_API_VERSION :
    case KADM5_NEW_SERVER_API_VERSION :
    case KADM5_SECURE_PRINC_MISSING :
    case KADM5_NO_RENAME_SALT :
    case KADM5_BAD_CLIENT_PARAMS :
    case KADM5_BAD_SERVER_PARAMS :
    case KADM5_AUTH_LIST :
    case KADM5_AUTH_CHANGEPW :
    case KADM5_BAD_TL_TYPE :
    case KADM5_MISSING_CONF_PARAMS :
    case KADM5_BAD_SERVER_NAME :
    default :
	return KADM_UNAUTH;	/* XXX */
    }
}

/*
 * server functions
 */

static int
kadm_ser_cpw(krb5_context context,
	     void *kadm_handle, 
	     krb5_principal principal, 
	     const char *principal_string,
	     krb5_storage *message,
	     krb5_storage *reply)
{
    char key[8];
    char *password = NULL;
    krb5_error_code ret;
    
    krb5_warnx(context, "v4-compat %s: CHPASS %s",
	       principal_string, principal_string); 

    ret = krb5_storage_read(message, key + 4, 4);
    ret = krb5_storage_read(message, key, 4);
    ret = krb5_ret_stringz(message, &password);
    
    if(password) {
	krb5_data pwd_data;
	const char *tmp;

	pwd_data.data   = password;
	pwd_data.length = strlen(password);

	tmp = kadm5_check_password_quality (context, principal, &pwd_data);

	if (tmp != NULL) {
	    krb5_store_stringz (reply, (char *)tmp);
	    ret = KADM5_PASS_Q_DICT;
	    goto fail;
	}
	ret = kadm5_chpass_principal(kadm_handle, principal, password);
    } else {
	krb5_key_data key_data[3];
	int i;
	for(i = 0; i < 3; i++) {
	    key_data[i].key_data_ver = 2;
	    key_data[i].key_data_kvno = 0;
	    /* key */
	    key_data[i].key_data_type[0] = ETYPE_DES_CBC_CRC;
	    key_data[i].key_data_length[0] = 8;
	    key_data[i].key_data_contents[0] = malloc(8);
	    memcpy(key_data[i].key_data_contents[0], &key, 8);
	    /* salt */
	    key_data[i].key_data_type[1] = KRB5_PW_SALT;
	    key_data[i].key_data_length[1] = 0;
	    key_data[i].key_data_contents[1] = NULL;
	}
	key_data[0].key_data_type[0] = ETYPE_DES_CBC_MD5;
	key_data[1].key_data_type[0] = ETYPE_DES_CBC_MD4;
	ret = kadm5_s_chpass_principal_with_key(kadm_handle, 
						principal, 3, key_data);
    }

    if(ret != 0) {
	krb5_store_stringz(reply, (char*)krb5_get_err_text(context, ret));
	goto fail;
    }
    return 0;
fail:
    krb5_warn(context, ret, "v4-compat CHPASS");
    return error_code(ret);
}

static int
kadm_ser_add(krb5_context context,
	     void *kadm_handle, 
	     krb5_principal principal, 
	     const char *principal_string,
	     krb5_storage *message,
	     krb5_storage *reply)
{
    int32_t mask;
    kadm5_principal_ent_rec ent, out;
    Kadm_vals values;
    krb5_error_code ret;
    char name[128];

    ret_vals(message, &values);

    ret = values_to_ent(context, &values, &ent, &mask);
    if(ret)
	goto fail;

    krb5_unparse_name_fixed(context, ent.principal, name, sizeof(name));
    krb5_warnx(context, "v4-compat %s: ADD %s",
	       principal_string, name);

    ret = _kadm5_acl_check_permission (kadm_handle, KADM5_PRIV_ADD,
				       ent.principal);
    if (ret)
	goto fail;

    ret = kadm5_s_create_principal_with_key(kadm_handle, &ent, mask);
    if(ret) {
	kadm5_free_principal_ent(kadm_handle, &ent);
	goto fail;
    }

    mask = KADM5_PRINCIPAL | KADM5_PRINC_EXPIRE_TIME | KADM5_MAX_LIFE |
	KADM5_KEY_DATA | KADM5_MOD_TIME | KADM5_MOD_NAME;
    
    kadm5_get_principal(kadm_handle, ent.principal, &out, mask);
    ent_to_values(context, &out, mask, &values);
    kadm5_free_principal_ent(kadm_handle, &ent);
    kadm5_free_principal_ent(kadm_handle, &out);
    store_vals(reply, &values);
    return 0;
fail:
    krb5_warn(context, ret, "v4-compat ADD");
    return error_code(ret);
}

static int
kadm_ser_get(krb5_context context,
	     void *kadm_handle, 
	     krb5_principal principal, 
	     const char *principal_string,
	     krb5_storage *message,
	     krb5_storage *reply)
{
    krb5_error_code ret;
    Kadm_vals values;
    kadm5_principal_ent_rec ent, out;
    int32_t mask;
    char flags[FLDSZ];
    char name[128];

    ret_vals(message, &values);
    /* XXX BRAIN DAMAGE! these flags are not stored in the same order
       as in the header */
    krb5_ret_int8(message, &flags[3]);
    krb5_ret_int8(message, &flags[2]);
    krb5_ret_int8(message, &flags[1]);
    krb5_ret_int8(message, &flags[0]);
    ret = values_to_ent(context, &values, &ent, &mask);
    if(ret)
	goto fail;

    krb5_unparse_name_fixed(context, ent.principal, name, sizeof(name));
    krb5_warnx(context, "v4-compat %s: GET %s",
	       principal_string, name);

    ret = _kadm5_acl_check_permission (kadm_handle, KADM5_PRIV_GET,
				       ent.principal);
    if (ret)
	goto fail;

    mask = flags_4_to_5(flags);

    ret = kadm5_get_principal(kadm_handle, ent.principal, &out, mask);
    kadm5_free_principal_ent(kadm_handle, &ent);

    if (ret)
	goto fail;
    
    ent_to_values(context, &out, mask, &values);

    kadm5_free_principal_ent(kadm_handle, &out);

    store_vals(reply, &values);
    return 0;
fail:
    krb5_warn(context, ret, "v4-compat GET");
    return error_code(ret);
}

static int
kadm_ser_mod(krb5_context context,
	     void *kadm_handle, 
	     krb5_principal principal, 
	     const char *principal_string,
	     krb5_storage *message,
	     krb5_storage *reply)
{
    Kadm_vals values1, values2;
    kadm5_principal_ent_rec ent, out;
    int32_t mask;
    krb5_error_code ret;
    char name[128];
    
    ret_vals(message, &values1);
    /* why are the old values sent? is the mask the same in the old and
       the new entry? */
    ret_vals(message, &values2);
    
    ret = values_to_ent(context, &values2, &ent, &mask);
    if(ret)
	goto fail;

    krb5_unparse_name_fixed(context, ent.principal, name, sizeof(name));
    krb5_warnx(context, "v4-compat %s: MOD %s",
	       principal_string, name);

    ret = _kadm5_acl_check_permission (kadm_handle, KADM5_PRIV_MODIFY,
				       ent.principal);
    if (ret)
	goto fail;

    ret = kadm5_s_modify_principal(kadm_handle, &ent, mask);
    if(ret) {
	kadm5_free_principal_ent(kadm_handle, &ent);
	krb5_warn(context, ret, "kadm5_s_modify_principal");
	goto fail;
    }

    ret = kadm5_get_principal(kadm_handle, ent.principal, &out, mask);
    if(ret) {
	kadm5_free_principal_ent(kadm_handle, &ent);
	krb5_warn(context, ret, "kadm5_s_modify_principal");
	goto fail;
    }

    ent_to_values(context, &out, mask, &values1);

    kadm5_free_principal_ent(kadm_handle, &ent);
    kadm5_free_principal_ent(kadm_handle, &out);
    
    store_vals(reply, &values1);
    return 0;
fail:
    krb5_warn(context, ret, "v4-compat MOD");
    return error_code(ret);
}

static int
kadm_ser_del(krb5_context context,
	     void *kadm_handle, 
	     krb5_principal principal, 
	     const char *principal_string,
	     krb5_storage *message,
	     krb5_storage *reply)
{
    Kadm_vals values;
    kadm5_principal_ent_rec ent;
    int32_t mask;
    krb5_error_code ret;
    char name[128];

    ret_vals(message, &values);

    ret = values_to_ent(context, &values, &ent, &mask);
    if(ret)
	goto fail;
    
    krb5_unparse_name_fixed(context, ent.principal, name, sizeof(name));
    krb5_warnx(context, "v4-compat %s: DEL %s",
	       principal_string, name);

    ret = _kadm5_acl_check_permission (kadm_handle, KADM5_PRIV_DELETE,
				       ent.principal);
    if (ret)
	goto fail;

    ret = kadm5_delete_principal(kadm_handle, ent.principal);

    kadm5_free_principal_ent(kadm_handle, &ent);

    if (ret)
	goto fail;

    return 0;
fail:
    krb5_warn(context, ret, "v4-compat ADD");
    return error_code(ret);
}

static int
dispatch(krb5_context context,
	 void *kadm_handle,
	 krb5_principal principal,
	 const char *principal_string,
	 krb5_data msg, 
	 krb5_data *reply)
{
    int retval;
    int8_t command;
    krb5_storage *sp_in, *sp_out;
    
    sp_in = krb5_storage_from_data(&msg);
    krb5_ret_int8(sp_in, &command);
    
    sp_out = krb5_storage_emem();
    krb5_storage_write(sp_out, KADM_VERSTR, KADM_VERSIZE);
    krb5_store_int32(sp_out, 0);

    switch(command) {
    case CHANGE_PW:
	retval = kadm_ser_cpw(context, kadm_handle, principal,
			      principal_string,
			      sp_in, sp_out);
	break;
    case ADD_ENT:
	retval = kadm_ser_add(context, kadm_handle, principal,
			      principal_string,
			      sp_in, sp_out);
	break;
    case GET_ENT:
	retval = kadm_ser_get(context, kadm_handle, principal,
			      principal_string,
			      sp_in, sp_out);
	break;
    case MOD_ENT:
	retval = kadm_ser_mod(context, kadm_handle, principal,
			      principal_string,
			      sp_in, sp_out);
	break;
    case DEL_ENT:
	retval = kadm_ser_del(context, kadm_handle, principal,
			      principal_string,
			      sp_in, sp_out);
	break;
    default:
	krb5_warnx(context, "v4-compat %s: unknown opcode: %d",
		   principal_string, command);
	retval = KADM_NO_OPCODE;
	break;
    }
    krb5_storage_free(sp_in);
    if(retval) {
	krb5_storage_seek(sp_out, KADM_VERSIZE, SEEK_SET);
	krb5_store_int32(sp_out, retval);
    }
    krb5_storage_to_data(sp_out, reply);
    krb5_storage_free(sp_out);
    return retval;
}

/*
 * Decode a v4 kadmin packet in `message' and create a reply in `reply'
 */

static void
decode_packet(krb5_context context,
	      krb5_keytab keytab,
	      struct sockaddr_in *admin_addr,
	      struct sockaddr_in *client_addr,
	      krb5_data message,
	      krb5_data *reply)
{
    int ret;
    KTEXT_ST authent;
    AUTH_DAT ad;
    MSG_DAT msg_dat;
    off_t off = 0;
    unsigned long rlen;
    char sname[] = "changepw", sinst[] = "kerberos";
    unsigned long checksum;
    des_key_schedule schedule;
    char *msg = message.data;
    void *kadm_handle;
    krb5_principal client;
    char *client_str;
    krb5_keytab_entry entry;
    
    if(message.length < KADM_VERSIZE
       || strncmp(msg, KADM_VERSTR, KADM_VERSIZE) != 0) {
	make_you_loose_packet (KADM_BAD_VER, reply);
	return;
    }

    off = KADM_VERSIZE;
    off += _krb5_get_int(msg + off, &rlen, 4);
    memset(&authent, 0, sizeof(authent));
    authent.length = message.length - rlen - KADM_VERSIZE - 4;

    if(authent.length >= MAX_KTXT_LEN) {
	krb5_warnx(context, "received bad rlen (%lu)", (unsigned long)rlen);
	make_you_loose_packet (KADM_LENGTH_ERROR, reply);
	return;
    }

    memcpy(authent.dat, (char*)msg + off, authent.length);
    off += authent.length;
    
    {
	krb5_principal principal;
	krb5_keyblock *key;

	ret = krb5_make_principal(context, &principal, NULL, 
				  "changepw", "kerberos", NULL);
	if (ret) {
	    krb5_warn (context, ret, "krb5_make_principal");
	    make_you_loose_packet (KADM_NOMEM, reply);
	    return;
	}
	ret = krb5_kt_get_entry (context, keytab, principal, 0,
				 ETYPE_DES_CBC_MD5, &entry);
	krb5_kt_close (context, keytab);
	if (ret) {
	    krb5_free_principal(context, principal);
	    make_you_loose_packet (KADM_NO_AUTH, reply);
	    return;
	}
	ret = krb5_copy_keyblock (context, &entry.keyblock,& key);
	krb5_kt_free_entry(context, &entry);
	krb5_free_principal(context, principal);
	if(ret) {
	    if(ret == KRB5_KT_NOTFOUND)
		make_you_loose_packet(KADM_NO_AUTH, reply);
	    else
		/* XXX */
		make_you_loose_packet(KADM_NO_AUTH, reply);
	    krb5_warn(context, ret, "krb5_kt_read_service_key");
	    return;
	}
	
	if(key->keyvalue.length != 8)
	    krb5_abortx(context, "key has wrong length (%lu)", 
			(unsigned long)key->keyvalue.length);
	krb_set_key(key->keyvalue.data, 0);
	krb5_free_keyblock(context, key);
    }
    
    ret = krb_rd_req(&authent, sname, sinst, 
		     client_addr->sin_addr.s_addr, &ad, NULL);

    if(ret) {
	make_you_loose_packet(ERROR_TABLE_BASE_krb + ret, reply);
	krb5_warnx(context, "krb_rd_req: %d", ret);
	return;
    }

    ret = krb5_425_conv_principal(context, ad.pname, ad.pinst, ad.prealm,
				  &client);
    if (ret) {
	krb5_warnx (context, "krb5_425_conv_principal: %d", ret);
	make_you_loose_packet (KADM_NOMEM, reply);
	return;
    }

    krb5_unparse_name(context, client, &client_str);

    ret = kadm5_init_with_password_ctx(context, 
				       client_str, 
				       NULL,
				       KADM5_ADMIN_SERVICE,
				       NULL, 0, 0, 
				       &kadm_handle);
    if (ret) {
	krb5_warn (context, ret, "kadm5_init_with_password_ctx");
	make_you_loose_packet (KADM_NOMEM, reply);
	goto out;
    }
    
    checksum = des_quad_cksum((void *)(msg + off), NULL, rlen, 0, &ad.session);
    if(checksum != ad.checksum) {
	krb5_warnx(context, "decode_packet: bad checksum");
	make_you_loose_packet (KADM_BAD_CHK, reply);
	goto out;
    }
    des_set_key(&ad.session, schedule);
    ret = krb_rd_priv(msg + off, rlen, schedule, &ad.session, 
		      client_addr, admin_addr, &msg_dat);
    if (ret) {
	make_you_loose_packet (ERROR_TABLE_BASE_krb + ret, reply);
	krb5_warnx(context, "krb_rd_priv: %d", ret);
	goto out;
    }

    {
	krb5_data d, r;
	int retval;

	d.data   = msg_dat.app_data;
	d.length = msg_dat.app_length;
	
	retval = dispatch(context, kadm_handle,
			  client, client_str, d, &r);
	krb5_data_alloc(reply, r.length + 26);
	reply->length = krb_mk_priv(r.data, reply->data, r.length, 
				    schedule, &ad.session, 
				    admin_addr, client_addr);
	if((ssize_t)reply->length < 0) {
	    make_you_loose_packet(KADM_NO_ENCRYPT, reply);
	    goto out;
	}
    }
out:
    krb5_free_principal(context, client);
    free(client_str);
}

void
handle_v4(krb5_context context,
	  krb5_keytab keytab,
	  int len,
	  int fd)
{
    int first = 1;
    struct sockaddr_in admin_addr, client_addr;
    socklen_t addr_len;
    krb5_data message, reply;
    ssize_t n;

    addr_len = sizeof(client_addr);
    if (getsockname(fd, (struct sockaddr*)&admin_addr, &addr_len) < 0)
	krb5_errx (context, 1, "getsockname");
    addr_len = sizeof(client_addr);
    if (getpeername(fd, (struct sockaddr*)&client_addr, &addr_len) < 0)
	krb5_errx (context, 1, "getpeername");

    while(1) {
	doing_useful_work = 0;
	if(term_flag)
	    exit(0);
	if(first) {
	    /* first time around, we have already read len, and two
               bytes of the version string */
	    krb5_data_alloc(&message, len);
	    memcpy(message.data, "KA", 2);
	    n = krb5_net_read(context, &fd, (char*)message.data + 2,
			      len - 2);
	    if (n == 0)
		exit (0);
	    if (n < 0)
		krb5_err (context, 1, errno, "krb5_net_read");
	    first = 0;
	} else {
	    char buf[2];
	    unsigned long tmp;
	    ssize_t n;

	    n = krb5_net_read(context, &fd, buf, sizeof(2));
	    if (n == 0)
		exit (0);
	    if (n < 0)
		krb5_err (context, 1, errno, "krb5_net_read");
	    _krb5_get_int(buf, &tmp, 2);
	    krb5_data_alloc(&message, tmp);
	    n = krb5_net_read(context, &fd, message.data, message.length);
	    if (n == 0)
		krb5_errx (context, 1, "EOF in krb5_net_read");
	    if (n < 0)
		krb5_err (context, 1, errno, "krb5_net_read");
	}
	doing_useful_work = 1;
	decode_packet(context, keytab, &admin_addr, &client_addr, 
		      message, &reply);
	krb5_data_free(&message);
	{
	    char buf[2];

	    _krb5_put_int(buf, reply.length, sizeof(buf));
	    n = krb5_net_write(context, &fd, buf, sizeof(buf));
	    if (n < 0)
		krb5_err (context, 1, errno, "krb5_net_write");
	    n = krb5_net_write(context, &fd, reply.data, reply.length);
	    if (n < 0)
		krb5_err (context, 1, errno, "krb5_net_write");
	    krb5_data_free(&reply);
	}
    }
}
