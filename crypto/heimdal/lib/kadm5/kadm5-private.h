/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

/* $Id: kadm5-private.h,v 1.3 2000/07/24 04:31:17 assar Exp $ */

#ifndef __kadm5_privatex_h__
#define __kadm5_privatex_h__

kadm5_ret_t _kadm5_privs_to_string (u_int32_t, char*, size_t);

kadm5_ret_t _kadm5_string_to_privs (const char*, u_int32_t*);

HDB *_kadm5_s_get_db (void *);

kadm5_ret_t
_kadm5_acl_check_permission __P((
	kadm5_server_context *context,
	unsigned op,
	krb5_const_principal princ));

kadm5_ret_t
_kadm5_acl_init __P((kadm5_server_context *context));

kadm5_ret_t
_kadm5_c_init_context __P((
	kadm5_client_context **ctx,
	kadm5_config_params *params,
	krb5_context context));

kadm5_ret_t
_kadm5_client_recv __P((
	kadm5_client_context *context,
	krb5_data *reply));

kadm5_ret_t
_kadm5_client_send __P((
	kadm5_client_context *context,
	krb5_storage *sp));

kadm5_ret_t
_kadm5_connect __P((void*));

kadm5_ret_t
_kadm5_error_code __P((kadm5_ret_t code));

kadm5_ret_t
_kadm5_s_init_context __P((
	kadm5_server_context **ctx,
	kadm5_config_params *params,
	krb5_context context));

kadm5_ret_t
_kadm5_set_keys __P((
	kadm5_server_context *context,
	hdb_entry *ent,
	const char *password));

kadm5_ret_t
_kadm5_set_keys2 __P((
	kadm5_server_context *context,
	hdb_entry *ent, 
	int16_t n_key_data, 
	krb5_key_data *key_data));

kadm5_ret_t
_kadm5_set_keys3 __P((
	kadm5_server_context *context,
	hdb_entry *ent, 
	int n_keys,
	krb5_keyblock *keyblocks));

kadm5_ret_t
_kadm5_set_keys_randomly __P((kadm5_server_context *context,
			      hdb_entry *ent,
			      krb5_keyblock **new_keys,
			      int *n_keys));

kadm5_ret_t
_kadm5_set_modifier __P((
	kadm5_server_context *context,
	hdb_entry *ent));

kadm5_ret_t
_kadm5_bump_pw_expire __P((kadm5_server_context *context,
			   hdb_entry *ent));

kadm5_ret_t
_kadm5_setup_entry __P((
	kadm5_server_context *context,
	hdb_entry *ent,
	u_int32_t mask,
	kadm5_principal_ent_t princ,
	u_int32_t princ_mask,
	kadm5_principal_ent_t def,
	u_int32_t def_mask));

kadm5_ret_t
kadm5_log_get_version_fd (int fd, u_int32_t *ver);

kadm5_ret_t
kadm5_log_get_version (kadm5_server_context *context, u_int32_t *ver);

kadm5_ret_t
kadm5_log_set_version (kadm5_server_context *context, u_int32_t vno);

kadm5_ret_t
kadm5_log_init (kadm5_server_context *context);

kadm5_ret_t
kadm5_log_reinit (kadm5_server_context *context);

kadm5_ret_t
kadm5_log_create (kadm5_server_context *context,
		  hdb_entry *ent);

kadm5_ret_t
kadm5_log_delete (kadm5_server_context *context,
		  krb5_principal princ);

kadm5_ret_t
kadm5_log_rename (kadm5_server_context *context,
		  krb5_principal source,
		  hdb_entry *ent);

kadm5_ret_t
kadm5_log_modify (kadm5_server_context *context,
		  hdb_entry *ent,
		  u_int32_t mask);

kadm5_ret_t
kadm5_log_nop (kadm5_server_context *context);

kadm5_ret_t
kadm5_log_end (kadm5_server_context *context);

kadm5_ret_t
kadm5_log_foreach (kadm5_server_context *context,
		   void (*func)(kadm5_server_context *server_context,
				u_int32_t ver,
				time_t timestamp,
				enum kadm_ops op,
				u_int32_t len,
				krb5_storage *sp));

kadm5_ret_t
kadm5_log_replay_create (kadm5_server_context *context,
			 u_int32_t ver,
			 u_int32_t len,
			 krb5_storage *sp);

kadm5_ret_t
kadm5_log_replay_delete (kadm5_server_context *context,
			 u_int32_t ver,
			 u_int32_t len,
			 krb5_storage *sp);

kadm5_ret_t
kadm5_log_replay_rename (kadm5_server_context *context,
			 u_int32_t ver,
			 u_int32_t len,
			 krb5_storage *sp);

kadm5_ret_t
kadm5_log_replay_modify (kadm5_server_context *context,
			 u_int32_t ver,
			 u_int32_t len,
			 krb5_storage *sp);

kadm5_ret_t
kadm5_log_replay_nop (kadm5_server_context *context,
		      u_int32_t ver,
		      u_int32_t len,
		      krb5_storage *sp);

kadm5_ret_t
kadm5_log_replay (kadm5_server_context *context,
		  enum kadm_ops op,
		  u_int32_t ver,
		  u_int32_t len,
		  krb5_storage *sp);

krb5_storage *
kadm5_log_goto_end (int fd);

kadm5_ret_t
kadm5_log_previous (krb5_storage *sp,
		    u_int32_t *ver,
		    time_t *timestamp,
		    enum kadm_ops *op,
		    u_int32_t *len);

kadm5_ret_t
kadm5_log_truncate (kadm5_server_context *server_context);

kadm5_ret_t
_kadm5_marshal_params __P((krb5_context context, 
			   kadm5_config_params *params, 
			   krb5_data *out));

kadm5_ret_t
_kadm5_unmarshal_params __P((krb5_context context,
			     krb5_data *in,
			     kadm5_config_params *params));

void
_kadm5_free_keys (kadm5_server_context *context,
		  int len, Key *keys);

void
_kadm5_init_keys (Key *keys, int len);

int
_kadm5_cmp_keys(Key *keys1, int len1, Key *keys2, int len2);

#endif /* __kadm5_privatex_h__ */
