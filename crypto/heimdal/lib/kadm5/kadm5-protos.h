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

/* $Id: kadm5-protos.h,v 1.2 2000/07/22 05:52:01 assar Exp $ */

#ifndef __kadm5_protos_h__
#define __kadm5_protos_h__

kadm5_ret_t
kadm5_c_chpass_principal __P((
	void *server_handle,
	krb5_principal princ,
	char *password));

kadm5_ret_t
kadm5_c_chpass_principal_with_key __P((
	void *server_handle,
	krb5_principal princ,
	int n_key_data,
	krb5_key_data *key_data));

kadm5_ret_t
kadm5_c_create_principal __P((
	void *server_handle,
	kadm5_principal_ent_t princ,
	u_int32_t mask,
	char *password));

kadm5_ret_t
kadm5_c_delete_principal __P((
	void *server_handle,
	krb5_principal princ));

kadm5_ret_t
kadm5_c_destroy __P((void *server_handle));

kadm5_ret_t
kadm5_c_flush __P((void *server_handle));

kadm5_ret_t
kadm5_c_get_principal __P((
	void *server_handle,
	krb5_principal princ,
	kadm5_principal_ent_t out,
	u_int32_t mask));

kadm5_ret_t
kadm5_c_get_principals __P((
	void *server_handle,
	const char *exp,
	char ***princs,
	int *count));

kadm5_ret_t
kadm5_c_get_privs __P((
	void *server_handle,
	u_int32_t *privs));

kadm5_ret_t
kadm5_c_init_with_creds __P((
	const char *client_name,
	krb5_ccache ccache,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_c_init_with_creds_ctx __P((
	krb5_context context,
	const char *client_name,
	krb5_ccache ccache,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_c_init_with_password __P((
	const char *client_name,
	const char *password,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_c_init_with_password_ctx __P((
	krb5_context context,
	const char *client_name,
	const char *password,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_c_init_with_skey __P((
	const char *client_name,
	const char *keytab,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_c_init_with_skey_ctx __P((
	krb5_context context,
	const char *client_name,
	const char *keytab,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_c_modify_principal __P((
	void *server_handle,
	kadm5_principal_ent_t princ,
	u_int32_t mask));

kadm5_ret_t
kadm5_c_randkey_principal __P((
	void *server_handle,
	krb5_principal princ,
	krb5_keyblock **new_keys,
	int *n_keys));

kadm5_ret_t
kadm5_c_rename_principal __P((
	void *server_handle,
	krb5_principal source,
	krb5_principal target));

kadm5_ret_t
kadm5_chpass_principal __P((
	void *server_handle,
	krb5_principal princ,
	char *password));

kadm5_ret_t
kadm5_chpass_principal_with_key __P((
	void *server_handle,
	krb5_principal princ,
	int n_key_data,
	krb5_key_data *key_data));

kadm5_ret_t
kadm5_create_principal __P((
	void *server_handle,
	kadm5_principal_ent_t princ,
	u_int32_t mask,
	char *password));

kadm5_ret_t
kadm5_delete_principal __P((
	void *server_handle,
	krb5_principal princ));

kadm5_ret_t
kadm5_destroy __P((void *server_handle));

kadm5_ret_t
kadm5_flush __P((void *server_handle));

void
kadm5_free_key_data __P((
	void *server_handle,
	int16_t *n_key_data,
	krb5_key_data *key_data));

void
kadm5_free_name_list __P((
	void *server_handle,
	char **names,
	int *count));

void
kadm5_free_principal_ent __P((
	void *server_handle,
	kadm5_principal_ent_t princ));

kadm5_ret_t
kadm5_get_principal __P((
	void *server_handle,
	krb5_principal princ,
	kadm5_principal_ent_t out,
	u_int32_t mask));

kadm5_ret_t
kadm5_get_principals __P((
	void *server_handle,
	const char *exp,
	char ***princs,
	int *count));

kadm5_ret_t
kadm5_get_privs __P((
	void *server_handle,
	u_int32_t *privs));

kadm5_ret_t
kadm5_init_with_creds __P((
	const char *client_name,
	krb5_ccache ccache,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_init_with_creds_ctx __P((
	krb5_context context,
	const char *client_name,
	krb5_ccache ccache,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_init_with_password __P((
	const char *client_name,
	const char *password,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_init_with_password_ctx __P((
	krb5_context context,
	const char *client_name,
	const char *password,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_init_with_skey __P((
	const char *client_name,
	const char *keytab,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_init_with_skey_ctx __P((
	krb5_context context,
	const char *client_name,
	const char *keytab,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_modify_principal __P((
	void *server_handle,
	kadm5_principal_ent_t princ,
	u_int32_t mask));

kadm5_ret_t
kadm5_randkey_principal __P((
	void *server_handle,
	krb5_principal princ,
	krb5_keyblock **new_keys,
	int *n_keys));

kadm5_ret_t
kadm5_rename_principal __P((
	void *server_handle,
	krb5_principal source,
	krb5_principal target));

kadm5_ret_t
kadm5_ret_key_data __P((
	krb5_storage *sp,
	krb5_key_data *key));

kadm5_ret_t
kadm5_ret_principal_ent __P((
	krb5_storage *sp,
	kadm5_principal_ent_t princ));

kadm5_ret_t
kadm5_ret_principal_ent_mask __P((
	krb5_storage *sp,
	kadm5_principal_ent_t princ,
	u_int32_t *mask));

kadm5_ret_t
kadm5_ret_tl_data __P((
	krb5_storage *sp,
	krb5_tl_data *tl));

kadm5_ret_t
kadm5_s_chpass_principal __P((
	void *server_handle,
	krb5_principal princ,
	char *password));

kadm5_ret_t
kadm5_s_chpass_principal_cond __P((
	void *server_handle,
	krb5_principal princ,
	char *password));

kadm5_ret_t
kadm5_s_chpass_principal_with_key __P((
	void *server_handle,
	krb5_principal princ,
	int n_key_data,
	krb5_key_data *key_data));

kadm5_ret_t
kadm5_s_create_principal __P((
	void *server_handle,
	kadm5_principal_ent_t princ,
	u_int32_t mask,
	char *password));

kadm5_ret_t
kadm5_s_create_principal_with_key __P((
	void *server_handle,
	kadm5_principal_ent_t princ,
	u_int32_t mask));

kadm5_ret_t
kadm5_s_delete_principal __P((
	void *server_handle,
	krb5_principal princ));

kadm5_ret_t
kadm5_s_destroy __P((void *server_handle));

kadm5_ret_t
kadm5_s_flush __P((void *server_handle));

kadm5_ret_t
kadm5_s_get_principal __P((
	void *server_handle,
	krb5_principal princ,
	kadm5_principal_ent_t out,
	u_int32_t mask));

kadm5_ret_t
kadm5_s_get_principals __P((
	void *server_handle,
	const char *exp,
	char ***princs,
	int *count));

kadm5_ret_t
kadm5_s_get_privs __P((
	void *server_handle,
	u_int32_t *privs));

kadm5_ret_t
kadm5_s_init_with_creds __P((
	const char *client_name,
	krb5_ccache ccache,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_s_init_with_creds_ctx __P((
	krb5_context context,
	const char *client_name,
	krb5_ccache ccache,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_s_init_with_password __P((
	const char *client_name,
	const char *password,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_s_init_with_password_ctx __P((
	krb5_context context,
	const char *client_name,
	const char *password,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_s_init_with_skey __P((
	const char *client_name,
	const char *keytab,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_s_init_with_skey_ctx __P((
	krb5_context context,
	const char *client_name,
	const char *keytab,
	const char *service_name,
	kadm5_config_params *realm_params,
	unsigned long struct_version,
	unsigned long api_version,
	void **server_handle));

kadm5_ret_t
kadm5_s_modify_principal __P((
	void *server_handle,
	kadm5_principal_ent_t princ,
	u_int32_t mask));

kadm5_ret_t
kadm5_s_randkey_principal __P((
	void *server_handle,
	krb5_principal princ,
	krb5_keyblock **new_keys,
	int *n_keys));

kadm5_ret_t
kadm5_s_rename_principal __P((
	void *server_handle,
	krb5_principal source,
	krb5_principal target));

kadm5_ret_t
kadm5_store_key_data __P((
	krb5_storage *sp,
	krb5_key_data *key));

kadm5_ret_t
kadm5_store_principal_ent __P((
	krb5_storage *sp,
	kadm5_principal_ent_t princ));

kadm5_ret_t
kadm5_store_principal_ent_mask __P((
	krb5_storage *sp,
	kadm5_principal_ent_t princ,
	u_int32_t mask));

kadm5_ret_t
kadm5_store_tl_data __P((
	krb5_storage *sp,
	krb5_tl_data *tl));

void
kadm5_setup_passwd_quality_check(krb5_context context,
				 const char *check_library,
				 const char *check_function);

const char *
kadm5_check_password_quality (krb5_context context,
			      krb5_principal principal,
			      krb5_data *pwd_data);

#endif /* __kadm5_protos_h__ */
