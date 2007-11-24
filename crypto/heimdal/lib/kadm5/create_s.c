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

#include "kadm5_locl.h"

RCSID("$Id: create_s.c,v 1.19 2001/01/30 01:24:28 assar Exp $");

static kadm5_ret_t
get_default(kadm5_server_context *context, krb5_principal princ, 
	    kadm5_principal_ent_t def)
{
    kadm5_ret_t ret;
    krb5_principal def_principal;
    krb5_realm *realm = krb5_princ_realm(context->context, princ);

    ret = krb5_make_principal(context->context, &def_principal, 
			      *realm, "default", NULL);
    if (ret)
	return ret;
    ret = kadm5_s_get_principal(context, def_principal, def, 
				KADM5_PRINCIPAL_NORMAL_MASK);
    krb5_free_principal (context->context, def_principal);
    return ret;
}

static kadm5_ret_t
create_principal(kadm5_server_context *context,
		 kadm5_principal_ent_t princ,
		 u_int32_t mask,
		 hdb_entry *ent,
		 u_int32_t required_mask,
		 u_int32_t forbidden_mask)
{
    kadm5_ret_t ret;
    kadm5_principal_ent_rec defrec, *defent;
    u_int32_t def_mask;
    
    if((mask & required_mask) != required_mask)
	return KADM5_BAD_MASK;
    if((mask & forbidden_mask))
	return KADM5_BAD_MASK;
    if((mask & KADM5_POLICY) && strcmp(princ->policy, "default"))
	/* XXX no real policies for now */
	return KADM5_UNK_POLICY;
    memset(ent, 0, sizeof(*ent));
    ret  = krb5_copy_principal(context->context, princ->principal, 
			       &ent->principal);
    if(ret)
	return ret;
    
    defent = &defrec;
    ret = get_default(context, princ->principal, defent);
    if(ret) {
	defent   = NULL;
	def_mask = 0;
    } else {
	def_mask = KADM5_ATTRIBUTES | KADM5_MAX_LIFE | KADM5_MAX_RLIFE;
    }

    ret = _kadm5_setup_entry(context,
			     ent, mask | def_mask,
			     princ, mask,
			     defent, def_mask);
    if(defent)
	kadm5_free_principal_ent(context, defent);
    
    ent->created_by.time = time(NULL);
    ret = krb5_copy_principal(context->context, context->caller, 
			      &ent->created_by.principal);

    return ret;
}

kadm5_ret_t
kadm5_s_create_principal_with_key(void *server_handle,
				  kadm5_principal_ent_t princ,
				  u_int32_t mask)
{
    kadm5_ret_t ret;
    hdb_entry ent;
    kadm5_server_context *context = server_handle;

    ret = create_principal(context, princ, mask, &ent,
			   KADM5_PRINCIPAL | KADM5_KEY_DATA,
			   KADM5_LAST_PWD_CHANGE | KADM5_MOD_TIME 
			   | KADM5_MOD_NAME | KADM5_MKVNO 
			   | KADM5_AUX_ATTRIBUTES 
			   | KADM5_POLICY_CLR | KADM5_LAST_SUCCESS 
			   | KADM5_LAST_FAILED | KADM5_FAIL_AUTH_COUNT);
    if(ret)
	goto out;

    ret = _kadm5_set_keys2(context, &ent, princ->n_key_data, princ->key_data);
    if(ret)
	goto out;
    
    ret = hdb_seal_keys(context->context, context->db, &ent);
    if (ret)
	goto out;
    
    kadm5_log_create (context, &ent);

    ret = context->db->open(context->context, context->db, O_RDWR, 0);
    if(ret)
	goto out;
    ret = context->db->store(context->context, context->db, 0, &ent);
    context->db->close(context->context, context->db);
out:
    hdb_free_entry(context->context, &ent);
    return _kadm5_error_code(ret);
}
				  

kadm5_ret_t
kadm5_s_create_principal(void *server_handle,
			 kadm5_principal_ent_t princ, 
			 u_int32_t mask,
			 char *password)
{
    kadm5_ret_t ret;
    hdb_entry ent;
    kadm5_server_context *context = server_handle;

    ret = create_principal(context, princ, mask, &ent,
			   KADM5_PRINCIPAL,
			   KADM5_LAST_PWD_CHANGE | KADM5_MOD_TIME 
			   | KADM5_MOD_NAME | KADM5_MKVNO 
			   | KADM5_AUX_ATTRIBUTES | KADM5_KEY_DATA
			   | KADM5_POLICY_CLR | KADM5_LAST_SUCCESS 
			   | KADM5_LAST_FAILED | KADM5_FAIL_AUTH_COUNT);
    if(ret)
	goto out;

    /* XXX this should be fixed */
    ent.keys.len = 4;
    ent.keys.val = calloc(ent.keys.len, sizeof(*ent.keys.val));
    ent.keys.val[0].key.keytype = ETYPE_DES_CBC_CRC;
    /* flag as version 4 compatible salt; ignored by _kadm5_set_keys
       if we don't want to be compatible */
    ent.keys.val[0].salt = calloc(1, sizeof(*ent.keys.val[0].salt));
    ent.keys.val[0].salt->type = hdb_pw_salt;
    ent.keys.val[1].key.keytype = ETYPE_DES_CBC_MD4;
    ent.keys.val[1].salt = calloc(1, sizeof(*ent.keys.val[1].salt));
    ent.keys.val[1].salt->type = hdb_pw_salt;
    ent.keys.val[2].key.keytype = ETYPE_DES_CBC_MD5;
    ent.keys.val[2].salt = calloc(1, sizeof(*ent.keys.val[2].salt));
    ent.keys.val[2].salt->type = hdb_pw_salt;
    ent.keys.val[3].key.keytype = ETYPE_DES3_CBC_SHA1;
    ret = _kadm5_set_keys(context, &ent, password);
    if (ret)
	goto out;

    ret = hdb_seal_keys(context->context, context->db, &ent);
    if (ret)
	goto out;
    
    kadm5_log_create (context, &ent);

    ret = context->db->open(context->context, context->db, O_RDWR, 0);
    if(ret)
	goto out;
    ret = context->db->store(context->context, context->db, 0, &ent);
    context->db->close(context->context, context->db);
out:
    hdb_free_entry(context->context, &ent);
    return _kadm5_error_code(ret);
}

