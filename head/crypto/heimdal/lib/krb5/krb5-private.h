/* This is a generated file */
#ifndef __krb5_private_h__
#define __krb5_private_h__

#include <stdarg.h>

void KRB5_LIB_FUNCTION
_krb5_aes_cts_encrypt (
	const unsigned char */*in*/,
	unsigned char */*out*/,
	size_t /*len*/,
	const AES_KEY */*key*/,
	unsigned char */*ivec*/,
	const int /*encryptp*/);

krb5_error_code
_krb5_cc_allocate (
	krb5_context /*context*/,
	const krb5_cc_ops */*ops*/,
	krb5_ccache */*id*/);

void
_krb5_crc_init_table (void);

uint32_t
_krb5_crc_update (
	const char */*p*/,
	size_t /*len*/,
	uint32_t /*res*/);

krb5_error_code
_krb5_dh_group_ok (
	krb5_context /*context*/,
	unsigned long /*bits*/,
	heim_integer */*p*/,
	heim_integer */*g*/,
	heim_integer */*q*/,
	struct krb5_dh_moduli **/*moduli*/,
	char **/*name*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_enctype_to_oid (
	krb5_context /*context*/,
	krb5_enctype /*etype*/,
	heim_oid */*oid*/);

krb5_error_code
_krb5_expand_default_cc_name (
	krb5_context /*context*/,
	const char */*str*/,
	char **/*res*/);

int
_krb5_extract_ticket (
	krb5_context /*context*/,
	krb5_kdc_rep */*rep*/,
	krb5_creds */*creds*/,
	krb5_keyblock */*key*/,
	krb5_const_pointer /*keyseed*/,
	krb5_key_usage /*key_usage*/,
	krb5_addresses */*addrs*/,
	unsigned /*nonce*/,
	unsigned /*flags*/,
	krb5_decrypt_proc /*decrypt_proc*/,
	krb5_const_pointer /*decryptarg*/);

void
_krb5_free_krbhst_info (krb5_krbhst_info */*hi*/);

void
_krb5_free_moduli (struct krb5_dh_moduli **/*moduli*/);

krb5_error_code
_krb5_get_default_principal_local (
	krb5_context /*context*/,
	krb5_principal */*princ*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_get_host_realm_int (
	krb5_context /*context*/,
	const char */*host*/,
	krb5_boolean /*use_dns*/,
	krb5_realm **/*realms*/);

krb5_error_code
_krb5_get_init_creds_opt_copy (
	krb5_context /*context*/,
	const krb5_get_init_creds_opt */*in*/,
	krb5_get_init_creds_opt **/*out*/);

void KRB5_LIB_FUNCTION
_krb5_get_init_creds_opt_free_krb5_error (krb5_get_init_creds_opt */*opt*/);

void KRB5_LIB_FUNCTION
_krb5_get_init_creds_opt_free_pkinit (krb5_get_init_creds_opt */*opt*/);

void KRB5_LIB_FUNCTION
_krb5_get_init_creds_opt_set_krb5_error (
	krb5_context /*context*/,
	krb5_get_init_creds_opt */*opt*/,
	const KRB_ERROR */*error*/);

krb5_ssize_t KRB5_LIB_FUNCTION
_krb5_get_int (
	void */*buffer*/,
	unsigned long */*value*/,
	size_t /*size*/);

krb5_error_code
_krb5_get_krbtgt (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_realm /*realm*/,
	krb5_creds **/*cred*/);

krb5_error_code
_krb5_kcm_chmod (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	uint16_t /*mode*/);

krb5_error_code
_krb5_kcm_chown (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	uint32_t /*uid*/,
	uint32_t /*gid*/);

krb5_error_code
_krb5_kcm_get_initial_ticket (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_principal /*server*/,
	krb5_keyblock */*key*/);

krb5_error_code
_krb5_kcm_get_ticket (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_kdc_flags /*flags*/,
	krb5_enctype /*enctype*/,
	krb5_principal /*server*/);

krb5_boolean
_krb5_kcm_is_running (krb5_context /*context*/);

krb5_error_code
_krb5_kcm_noop (
	krb5_context /*context*/,
	krb5_ccache /*id*/);

krb5_error_code
_krb5_kdc_retry (
	krb5_context /*context*/,
	krb5_sendto_ctx /*ctx*/,
	void */*data*/,
	const krb5_data */*reply*/,
	int */*action*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_cr_err_reply (
	krb5_context /*context*/,
	const char */*name*/,
	const char */*inst*/,
	const char */*realm*/,
	uint32_t /*time_ws*/,
	uint32_t /*e*/,
	const char */*e_string*/,
	krb5_data */*data*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_create_auth_reply (
	krb5_context /*context*/,
	const char */*pname*/,
	const char */*pinst*/,
	const char */*prealm*/,
	int32_t /*time_ws*/,
	int /*n*/,
	uint32_t /*x_date*/,
	unsigned char /*kvno*/,
	const krb5_data */*cipher*/,
	krb5_data */*data*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_create_ciph (
	krb5_context /*context*/,
	const krb5_keyblock */*session*/,
	const char */*service*/,
	const char */*instance*/,
	const char */*realm*/,
	uint32_t /*life*/,
	unsigned char /*kvno*/,
	const krb5_data */*ticket*/,
	uint32_t /*kdc_time*/,
	const krb5_keyblock */*key*/,
	krb5_data */*enc_data*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_create_ticket (
	krb5_context /*context*/,
	unsigned char /*flags*/,
	const char */*pname*/,
	const char */*pinstance*/,
	const char */*prealm*/,
	int32_t /*paddress*/,
	const krb5_keyblock */*session*/,
	int16_t /*life*/,
	int32_t /*life_sec*/,
	const char */*sname*/,
	const char */*sinstance*/,
	const krb5_keyblock */*key*/,
	krb5_data */*enc_data*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_decomp_ticket (
	krb5_context /*context*/,
	const krb5_data */*enc_ticket*/,
	const krb5_keyblock */*key*/,
	const char */*local_realm*/,
	char **/*sname*/,
	char **/*sinstance*/,
	struct _krb5_krb_auth_data */*ad*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_dest_tkt (
	krb5_context /*context*/,
	const char */*tkfile*/);

void KRB5_LIB_FUNCTION
_krb5_krb_free_auth_data (
	krb5_context /*context*/,
	struct _krb5_krb_auth_data */*ad*/);

time_t KRB5_LIB_FUNCTION
_krb5_krb_life_to_time (
	int /*start*/,
	int /*life_*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_rd_req (
	krb5_context /*context*/,
	krb5_data */*authent*/,
	const char */*service*/,
	const char */*instance*/,
	const char */*local_realm*/,
	int32_t /*from_addr*/,
	const krb5_keyblock */*key*/,
	struct _krb5_krb_auth_data */*ad*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_tf_setup (
	krb5_context /*context*/,
	struct credentials */*v4creds*/,
	const char */*tkfile*/,
	int /*append*/);

int KRB5_LIB_FUNCTION
_krb5_krb_time_to_life (
	time_t /*start*/,
	time_t /*end*/);

krb5_error_code
_krb5_krbhost_info_move (
	krb5_context /*context*/,
	krb5_krbhst_info */*from*/,
	krb5_krbhst_info **/*to*/);

krb5_error_code
_krb5_mk_req_internal (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_flags /*ap_req_options*/,
	krb5_data */*in_data*/,
	krb5_creds */*in_creds*/,
	krb5_data */*outbuf*/,
	krb5_key_usage /*checksum_usage*/,
	krb5_key_usage /*encrypt_usage*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_n_fold (
	const void */*str*/,
	size_t /*len*/,
	void */*key*/,
	size_t /*size*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_oid_to_enctype (
	krb5_context /*context*/,
	const heim_oid */*oid*/,
	krb5_enctype */*etype*/);

krb5_error_code
_krb5_pac_sign (
	krb5_context /*context*/,
	krb5_pac /*p*/,
	time_t /*authtime*/,
	krb5_principal /*principal*/,
	const krb5_keyblock */*server_key*/,
	const krb5_keyblock */*priv_key*/,
	krb5_data */*data*/);

krb5_error_code
_krb5_parse_moduli (
	krb5_context /*context*/,
	const char */*file*/,
	struct krb5_dh_moduli ***/*moduli*/);

krb5_error_code
_krb5_parse_moduli_line (
	krb5_context /*context*/,
	const char */*file*/,
	int /*lineno*/,
	char */*p*/,
	struct krb5_dh_moduli **/*m*/);

void KRB5_LIB_FUNCTION
_krb5_pk_allow_proxy_certificate (
	struct krb5_pk_identity */*id*/,
	int /*boolean*/);

void KRB5_LIB_FUNCTION
_krb5_pk_cert_free (struct krb5_pk_cert */*cert*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_load_id (
	krb5_context /*context*/,
	struct krb5_pk_identity **/*ret_id*/,
	const char */*user_id*/,
	const char */*anchor_id*/,
	char * const */*chain_list*/,
	char * const */*revoke_list*/,
	krb5_prompter_fct /*prompter*/,
	void */*prompter_data*/,
	char */*password*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_mk_ContentInfo (
	krb5_context /*context*/,
	const krb5_data */*buf*/,
	const heim_oid */*oid*/,
	struct ContentInfo */*content_info*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_mk_padata (
	krb5_context /*context*/,
	void */*c*/,
	const KDC_REQ_BODY */*req_body*/,
	unsigned /*nonce*/,
	METHOD_DATA */*md*/);

krb5_error_code
_krb5_pk_octetstring2key (
	krb5_context /*context*/,
	krb5_enctype /*type*/,
	const void */*dhdata*/,
	size_t /*dhsize*/,
	const heim_octet_string */*c_n*/,
	const heim_octet_string */*k_n*/,
	krb5_keyblock */*key*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_rd_pa_reply (
	krb5_context /*context*/,
	const char */*realm*/,
	void */*c*/,
	krb5_enctype /*etype*/,
	const krb5_krbhst_info */*hi*/,
	unsigned /*nonce*/,
	const krb5_data */*req_buffer*/,
	PA_DATA */*pa*/,
	krb5_keyblock **/*key*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_pk_verify_sign (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	struct krb5_pk_identity */*id*/,
	heim_oid */*contentType*/,
	krb5_data */*content*/,
	struct krb5_pk_cert **/*signer*/);

krb5_error_code
_krb5_plugin_find (
	krb5_context /*context*/,
	enum krb5_plugin_type /*type*/,
	const char */*name*/,
	struct krb5_plugin **/*list*/);

void
_krb5_plugin_free (struct krb5_plugin */*list*/);

struct krb5_plugin *
_krb5_plugin_get_next (struct krb5_plugin */*p*/);

void *
_krb5_plugin_get_symbol (struct krb5_plugin */*p*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_principal2principalname (
	PrincipalName */*p*/,
	const krb5_principal /*from*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_principalname2krb5_principal (
	krb5_context /*context*/,
	krb5_principal */*principal*/,
	const PrincipalName /*from*/,
	const Realm /*realm*/);

krb5_ssize_t KRB5_LIB_FUNCTION
_krb5_put_int (
	void */*buffer*/,
	unsigned long /*value*/,
	size_t /*size*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_rd_req_out_ctx_alloc (
	krb5_context /*context*/,
	krb5_rd_req_out_ctx */*ctx*/);

krb5_error_code KRB5_LIB_FUNCTION
_krb5_s4u2self_to_checksumdata (
	krb5_context /*context*/,
	const PA_S4U2Self */*self*/,
	krb5_data */*data*/);

int
_krb5_send_and_recv_tcp (
	int /*fd*/,
	time_t /*tmout*/,
	const krb5_data */*req*/,
	krb5_data */*rep*/);

int
_krb5_xlock (
	krb5_context /*context*/,
	int /*fd*/,
	krb5_boolean /*exclusive*/,
	const char */*filename*/);

int
_krb5_xunlock (
	krb5_context /*context*/,
	int /*fd*/);

#endif /* __krb5_private_h__ */
