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

RCSID("$Id: chpass_s.c,v 1.13.8.1 2003/12/30 15:59:58 lha Exp $");

static kadm5_ret_t
change(void *server_handle, 
       krb5_principal princ,
       char *password,
       int cond)
{
    kadm5_server_context *context = server_handle;
    hdb_entry ent;
    kadm5_ret_t ret;
    Key *keys;
    size_t num_keys;
    int cmp = 1;

    ent.principal = princ;
    ret = context->db->open(context->context, context->db, O_RDWR, 0);
    if(ret)
	return ret;
    ret = context->db->fetch(context->context, context->db, 
			     HDB_F_DECRYPT, &ent);
    if(ret == HDB_ERR_NOENTRY)
	goto out;

    num_keys = ent.keys.len;
    keys     = ent.keys.val;

    ent.keys.len = 0;
    ent.keys.val = NULL;

    ret = _kadm5_set_keys(context, &ent, password);
    if(ret) {
	_kadm5_free_keys (server_handle, num_keys, keys);
	goto out2;
    }
    if (cond)
	cmp = _kadm5_cmp_keys (ent.keys.val, ent.keys.len,
			       keys, num_keys);
    _kadm5_free_keys (server_handle, num_keys, keys);

    if (cmp == 0) {
	krb5_set_error_string(context->context, "Password reuse forbidden");
	ret = KADM5_PASS_REUSE;
 	goto out2;
    }
    ret = _kadm5_set_modifier(context, &ent);
    if(ret)
	goto out2;

    ret = _kadm5_bump_pw_expire(context, &ent);
    if (ret)
	goto out2;

    ret = hdb_seal_keys(context->context, context->db, &ent);
    if (ret)
	goto out2;

    kadm5_log_modify (context,
		      &ent,
		      KADM5_PRINCIPAL | KADM5_MOD_NAME | KADM5_MOD_TIME |
		      KADM5_KEY_DATA | KADM5_KVNO | KADM5_PW_EXPIRATION);
    
    ret = context->db->store(context->context, context->db, 
			     HDB_F_REPLACE, &ent);
out2:
    hdb_free_entry(context->context, &ent);
out:
    context->db->close(context->context, context->db);
    return _kadm5_error_code(ret);
}



/*
 * change the password of `princ' to `password' if it's not already that.
 */

kadm5_ret_t
kadm5_s_chpass_principal_cond(void *server_handle, 
			      krb5_principal princ,
			      char *password)
{
    return change (server_handle, princ, password, 1);
}

/*
 * change the password of `princ' to `password'
 */

kadm5_ret_t
kadm5_s_chpass_principal(void *server_handle, 
			 krb5_principal princ,
			 char *password)
{
    return change (server_handle, princ, password, 0);
}

/*
 * change keys for `princ' to `keys'
 */

kadm5_ret_t
kadm5_s_chpass_principal_with_key(void *server_handle, 
				  krb5_principal princ,
				  int n_key_data,
				  krb5_key_data *key_data)
{
    kadm5_server_context *context = server_handle;
    hdb_entry ent;
    kadm5_ret_t ret;
    ent.principal = princ;
    ret = context->db->open(context->context, context->db, O_RDWR, 0);
    if(ret)
	return ret;
    ret = context->db->fetch(context->context, context->db, 0, &ent);
    if(ret == HDB_ERR_NOENTRY)
	goto out;
    ret = _kadm5_set_keys2(context, &ent, n_key_data, key_data);
    if(ret)
	goto out2;
    ret = _kadm5_set_modifier(context, &ent);
    if(ret)
	goto out2;
    ret = _kadm5_bump_pw_expire(context, &ent);
    if (ret)
	goto out2;

    ret = hdb_seal_keys(context->context, context->db, &ent);
    if (ret)
	goto out2;

    kadm5_log_modify (context,
		      &ent,
		      KADM5_PRINCIPAL | KADM5_MOD_NAME | KADM5_MOD_TIME |
		      KADM5_KEY_DATA | KADM5_KVNO | KADM5_PW_EXPIRATION);
    
    ret = context->db->store(context->context, context->db, 
			     HDB_F_REPLACE, &ent);
out2:
    hdb_free_entry(context->context, &ent);
out:
    context->db->close(context->context, context->db);
    return _kadm5_error_code(ret);
}
