/* This is a generated file */
#ifndef __gsskrb5_private_h__
#define __gsskrb5_private_h__

#include <stdarg.h>

gssapi_mech_interface
__gss_krb5_initialize (void);

OM_uint32
__gsskrb5_ccache_lifetime (
	OM_uint32 */*minor_status*/,
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_principal /*principal*/,
	OM_uint32 */*lifetime*/);

OM_uint32
_gss_DES3_get_mic_compat (
	OM_uint32 */*minor_status*/,
	gsskrb5_ctx /*ctx*/,
	krb5_context /*context*/);

OM_uint32
_gssapi_decapsulate (
	 OM_uint32 */*minor_status*/,
	gss_buffer_t /*input_token_buffer*/,
	krb5_data */*out_data*/,
	const gss_OID mech );

void
_gssapi_encap_length (
	size_t /*data_len*/,
	size_t */*len*/,
	size_t */*total_len*/,
	const gss_OID /*mech*/);

OM_uint32
_gssapi_encapsulate (
	 OM_uint32 */*minor_status*/,
	const krb5_data */*in_data*/,
	gss_buffer_t /*output_token*/,
	const gss_OID mech );

OM_uint32
_gssapi_get_mic_arcfour (
	OM_uint32 * /*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	gss_qop_t /*qop_req*/,
	const gss_buffer_t /*message_buffer*/,
	gss_buffer_t /*message_token*/,
	krb5_keyblock */*key*/);

void *
_gssapi_make_mech_header (
	void */*ptr*/,
	size_t /*len*/,
	const gss_OID /*mech*/);

OM_uint32
_gssapi_mic_cfx (
	OM_uint32 */*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	gss_qop_t /*qop_req*/,
	const gss_buffer_t /*message_buffer*/,
	gss_buffer_t /*message_token*/,
	krb5_keyblock */*key*/);

OM_uint32
_gssapi_msg_order_check (
	struct gss_msg_order */*o*/,
	OM_uint32 /*seq_num*/);

OM_uint32
_gssapi_msg_order_create (
	OM_uint32 */*minor_status*/,
	struct gss_msg_order **/*o*/,
	OM_uint32 /*flags*/,
	OM_uint32 /*seq_num*/,
	OM_uint32 /*jitter_window*/,
	int /*use_64*/);

OM_uint32
_gssapi_msg_order_destroy (struct gss_msg_order **/*m*/);

krb5_error_code
_gssapi_msg_order_export (
	krb5_storage */*sp*/,
	struct gss_msg_order */*o*/);

OM_uint32
_gssapi_msg_order_f (OM_uint32 /*flags*/);

OM_uint32
_gssapi_msg_order_import (
	OM_uint32 */*minor_status*/,
	krb5_storage */*sp*/,
	struct gss_msg_order **/*o*/);

OM_uint32
_gssapi_unwrap_arcfour (
	OM_uint32 */*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	const gss_buffer_t /*input_message_buffer*/,
	gss_buffer_t /*output_message_buffer*/,
	int */*conf_state*/,
	gss_qop_t */*qop_state*/,
	krb5_keyblock */*key*/);

OM_uint32
_gssapi_unwrap_cfx (
	OM_uint32 */*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	const gss_buffer_t /*input_message_buffer*/,
	gss_buffer_t /*output_message_buffer*/,
	int */*conf_state*/,
	gss_qop_t */*qop_state*/,
	krb5_keyblock */*key*/);

OM_uint32
_gssapi_verify_mech_header (
	u_char **/*str*/,
	size_t /*total_len*/,
	gss_OID /*mech*/);

OM_uint32
_gssapi_verify_mic_arcfour (
	OM_uint32 * /*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	const gss_buffer_t /*message_buffer*/,
	const gss_buffer_t /*token_buffer*/,
	gss_qop_t * /*qop_state*/,
	krb5_keyblock */*key*/,
	char */*type*/);

OM_uint32
_gssapi_verify_mic_cfx (
	OM_uint32 */*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	const gss_buffer_t /*message_buffer*/,
	const gss_buffer_t /*token_buffer*/,
	gss_qop_t */*qop_state*/,
	krb5_keyblock */*key*/);

OM_uint32
_gssapi_verify_pad (
	gss_buffer_t /*wrapped_token*/,
	size_t /*datalen*/,
	size_t */*padlen*/);

OM_uint32
_gssapi_wrap_arcfour (
	OM_uint32 * /*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	const gss_buffer_t /*input_message_buffer*/,
	int * /*conf_state*/,
	gss_buffer_t /*output_message_buffer*/,
	krb5_keyblock */*key*/);

OM_uint32
_gssapi_wrap_cfx (
	OM_uint32 */*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	const gss_buffer_t /*input_message_buffer*/,
	int */*conf_state*/,
	gss_buffer_t /*output_message_buffer*/,
	krb5_keyblock */*key*/);

OM_uint32
_gssapi_wrap_size_arcfour (
	OM_uint32 */*minor_status*/,
	const gsskrb5_ctx /*ctx*/,
	krb5_context /*context*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	OM_uint32 /*req_output_size*/,
	OM_uint32 */*max_input_size*/,
	krb5_keyblock */*key*/);

OM_uint32
_gssapi_wrap_size_cfx (
	OM_uint32 */*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	OM_uint32 /*req_output_size*/,
	OM_uint32 */*max_input_size*/,
	krb5_keyblock */*key*/);

OM_uint32
_gsskrb5_accept_sec_context (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t * /*context_handle*/,
	const gss_cred_id_t /*acceptor_cred_handle*/,
	const gss_buffer_t /*input_token_buffer*/,
	const gss_channel_bindings_t /*input_chan_bindings*/,
	gss_name_t * /*src_name*/,
	gss_OID * /*mech_type*/,
	gss_buffer_t /*output_token*/,
	OM_uint32 * /*ret_flags*/,
	OM_uint32 * /*time_rec*/,
	gss_cred_id_t * /*delegated_cred_handle*/);

OM_uint32
_gsskrb5_acquire_cred (
	OM_uint32 * /*minor_status*/,
	const gss_name_t /*desired_name*/,
	OM_uint32 /*time_req*/,
	const gss_OID_set /*desired_mechs*/,
	gss_cred_usage_t /*cred_usage*/,
	gss_cred_id_t * /*output_cred_handle*/,
	gss_OID_set * /*actual_mechs*/,
	OM_uint32 * time_rec );

OM_uint32
_gsskrb5_add_cred (
	 OM_uint32 */*minor_status*/,
	const gss_cred_id_t /*input_cred_handle*/,
	const gss_name_t /*desired_name*/,
	const gss_OID /*desired_mech*/,
	gss_cred_usage_t /*cred_usage*/,
	OM_uint32 /*initiator_time_req*/,
	OM_uint32 /*acceptor_time_req*/,
	gss_cred_id_t */*output_cred_handle*/,
	gss_OID_set */*actual_mechs*/,
	OM_uint32 */*initiator_time_rec*/,
	OM_uint32 */*acceptor_time_rec*/);

OM_uint32
_gsskrb5_canonicalize_name (
	 OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	const gss_OID /*mech_type*/,
	gss_name_t * output_name );

void
_gsskrb5_clear_status (void);

OM_uint32
_gsskrb5_compare_name (
	OM_uint32 * /*minor_status*/,
	const gss_name_t /*name1*/,
	const gss_name_t /*name2*/,
	int * name_equal );

OM_uint32
_gsskrb5_context_time (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	OM_uint32 * time_rec );

OM_uint32
_gsskrb5_create_8003_checksum (
	 OM_uint32 */*minor_status*/,
	const gss_channel_bindings_t /*input_chan_bindings*/,
	OM_uint32 /*flags*/,
	const krb5_data */*fwd_data*/,
	Checksum */*result*/);

OM_uint32
_gsskrb5_create_ctx (
	 OM_uint32 * /*minor_status*/,
	gss_ctx_id_t * /*context_handle*/,
	krb5_context /*context*/,
	const gss_channel_bindings_t /*input_chan_bindings*/,
	enum gss_ctx_id_t_state /*state*/);

OM_uint32
_gsskrb5_decapsulate (
	OM_uint32 */*minor_status*/,
	gss_buffer_t /*input_token_buffer*/,
	krb5_data */*out_data*/,
	const void */*type*/,
	gss_OID /*oid*/);

krb5_error_code
_gsskrb5_decode_be_om_uint32 (
	const void */*ptr*/,
	OM_uint32 */*n*/);

krb5_error_code
_gsskrb5_decode_om_uint32 (
	const void */*ptr*/,
	OM_uint32 */*n*/);

OM_uint32
_gsskrb5_delete_sec_context (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t * /*context_handle*/,
	gss_buffer_t /*output_token*/);

OM_uint32
_gsskrb5_display_name (
	OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	gss_buffer_t /*output_name_buffer*/,
	gss_OID * output_name_type );

OM_uint32
_gsskrb5_display_status (
	OM_uint32 */*minor_status*/,
	OM_uint32 /*status_value*/,
	int /*status_type*/,
	const gss_OID /*mech_type*/,
	OM_uint32 */*message_context*/,
	gss_buffer_t /*status_string*/);

OM_uint32
_gsskrb5_duplicate_name (
	 OM_uint32 * /*minor_status*/,
	const gss_name_t /*src_name*/,
	gss_name_t * dest_name );

void
_gsskrb5_encap_length (
	size_t /*data_len*/,
	size_t */*len*/,
	size_t */*total_len*/,
	const gss_OID /*mech*/);

OM_uint32
_gsskrb5_encapsulate (
	 OM_uint32 */*minor_status*/,
	const krb5_data */*in_data*/,
	gss_buffer_t /*output_token*/,
	const void */*type*/,
	const gss_OID mech );

krb5_error_code
_gsskrb5_encode_be_om_uint32 (
	OM_uint32 /*n*/,
	u_char */*p*/);

krb5_error_code
_gsskrb5_encode_om_uint32 (
	OM_uint32 /*n*/,
	u_char */*p*/);

OM_uint32
_gsskrb5_export_name (
	OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	gss_buffer_t exported_name );

OM_uint32
_gsskrb5_export_sec_context (
	 OM_uint32 * /*minor_status*/,
	gss_ctx_id_t * /*context_handle*/,
	gss_buffer_t interprocess_token );

ssize_t
_gsskrb5_get_mech (
	const u_char */*ptr*/,
	size_t /*total_len*/,
	const u_char **/*mech_ret*/);

OM_uint32
_gsskrb5_get_mic (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	gss_qop_t /*qop_req*/,
	const gss_buffer_t /*message_buffer*/,
	gss_buffer_t message_token );

OM_uint32
_gsskrb5_get_tkt_flags (
	OM_uint32 */*minor_status*/,
	gsskrb5_ctx /*ctx*/,
	OM_uint32 */*tkt_flags*/);

OM_uint32
_gsskrb5_import_cred (
	OM_uint32 */*minor_status*/,
	krb5_ccache /*id*/,
	krb5_principal /*keytab_principal*/,
	krb5_keytab /*keytab*/,
	gss_cred_id_t */*cred*/);

OM_uint32
_gsskrb5_import_name (
	OM_uint32 * /*minor_status*/,
	const gss_buffer_t /*input_name_buffer*/,
	const gss_OID /*input_name_type*/,
	gss_name_t * output_name );

OM_uint32
_gsskrb5_import_sec_context (
	 OM_uint32 * /*minor_status*/,
	const gss_buffer_t /*interprocess_token*/,
	gss_ctx_id_t * context_handle );

OM_uint32
_gsskrb5_indicate_mechs (
	OM_uint32 * /*minor_status*/,
	gss_OID_set * mech_set );

krb5_error_code
_gsskrb5_init (krb5_context */*context*/);

OM_uint32
_gsskrb5_init_sec_context (
	OM_uint32 * /*minor_status*/,
	const gss_cred_id_t /*initiator_cred_handle*/,
	gss_ctx_id_t * /*context_handle*/,
	const gss_name_t /*target_name*/,
	const gss_OID /*mech_type*/,
	OM_uint32 /*req_flags*/,
	OM_uint32 /*time_req*/,
	const gss_channel_bindings_t /*input_chan_bindings*/,
	const gss_buffer_t /*input_token*/,
	gss_OID * /*actual_mech_type*/,
	gss_buffer_t /*output_token*/,
	OM_uint32 * /*ret_flags*/,
	OM_uint32 * time_rec );

OM_uint32
_gsskrb5_inquire_context (
	 OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	gss_name_t * /*src_name*/,
	gss_name_t * /*targ_name*/,
	OM_uint32 * /*lifetime_rec*/,
	gss_OID * /*mech_type*/,
	OM_uint32 * /*ctx_flags*/,
	int * /*locally_initiated*/,
	int * open_context );

OM_uint32
_gsskrb5_inquire_cred (
	OM_uint32 * /*minor_status*/,
	const gss_cred_id_t /*cred_handle*/,
	gss_name_t * /*output_name*/,
	OM_uint32 * /*lifetime*/,
	gss_cred_usage_t * /*cred_usage*/,
	gss_OID_set * mechanisms );

OM_uint32
_gsskrb5_inquire_cred_by_mech (
	 OM_uint32 * /*minor_status*/,
	const gss_cred_id_t /*cred_handle*/,
	const gss_OID /*mech_type*/,
	gss_name_t * /*name*/,
	OM_uint32 * /*initiator_lifetime*/,
	OM_uint32 * /*acceptor_lifetime*/,
	gss_cred_usage_t * cred_usage );

OM_uint32
_gsskrb5_inquire_cred_by_oid (
	OM_uint32 * /*minor_status*/,
	const gss_cred_id_t /*cred_handle*/,
	const gss_OID /*desired_object*/,
	gss_buffer_set_t */*data_set*/);

OM_uint32
_gsskrb5_inquire_mechs_for_name (
	 OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	gss_OID_set * mech_types );

OM_uint32
_gsskrb5_inquire_names_for_mech (
	 OM_uint32 * /*minor_status*/,
	const gss_OID /*mechanism*/,
	gss_OID_set * name_types );

OM_uint32
_gsskrb5_inquire_sec_context_by_oid (
	OM_uint32 */*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	const gss_OID /*desired_object*/,
	gss_buffer_set_t */*data_set*/);

OM_uint32
_gsskrb5_krb5_ccache_name (
	OM_uint32 */*minor_status*/,
	const char */*name*/,
	const char **/*out_name*/);

OM_uint32
_gsskrb5_lifetime_left (
	OM_uint32 */*minor_status*/,
	krb5_context /*context*/,
	OM_uint32 /*lifetime*/,
	OM_uint32 */*lifetime_rec*/);

void *
_gsskrb5_make_header (
	void */*ptr*/,
	size_t /*len*/,
	const void */*type*/,
	const gss_OID /*mech*/);

OM_uint32
_gsskrb5_process_context_token (
	 OM_uint32 */*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	const gss_buffer_t token_buffer );

OM_uint32
_gsskrb5_pseudo_random (
	OM_uint32 */*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	int /*prf_key*/,
	const gss_buffer_t /*prf_in*/,
	ssize_t /*desired_output_len*/,
	gss_buffer_t /*prf_out*/);

OM_uint32
_gsskrb5_register_acceptor_identity (const char */*identity*/);

OM_uint32
_gsskrb5_release_buffer (
	OM_uint32 * /*minor_status*/,
	gss_buffer_t buffer );

OM_uint32
_gsskrb5_release_cred (
	OM_uint32 * /*minor_status*/,
	gss_cred_id_t * cred_handle );

OM_uint32
_gsskrb5_release_name (
	OM_uint32 * /*minor_status*/,
	gss_name_t * input_name );

OM_uint32
_gsskrb5_seal (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	int /*conf_req_flag*/,
	int /*qop_req*/,
	gss_buffer_t /*input_message_buffer*/,
	int * /*conf_state*/,
	gss_buffer_t output_message_buffer );

OM_uint32
_gsskrb5_set_cred_option (
	OM_uint32 */*minor_status*/,
	gss_cred_id_t */*cred_handle*/,
	const gss_OID /*desired_object*/,
	const gss_buffer_t /*value*/);

OM_uint32
_gsskrb5_set_sec_context_option (
	OM_uint32 */*minor_status*/,
	gss_ctx_id_t */*context_handle*/,
	const gss_OID /*desired_object*/,
	const gss_buffer_t /*value*/);

void
_gsskrb5_set_status (
	const char */*fmt*/,
	...);

OM_uint32
_gsskrb5_sign (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	int /*qop_req*/,
	gss_buffer_t /*message_buffer*/,
	gss_buffer_t message_token );

OM_uint32
_gsskrb5_unseal (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	gss_buffer_t /*input_message_buffer*/,
	gss_buffer_t /*output_message_buffer*/,
	int * /*conf_state*/,
	int * qop_state );

OM_uint32
_gsskrb5_unwrap (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	const gss_buffer_t /*input_message_buffer*/,
	gss_buffer_t /*output_message_buffer*/,
	int * /*conf_state*/,
	gss_qop_t * qop_state );

OM_uint32
_gsskrb5_verify (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	gss_buffer_t /*message_buffer*/,
	gss_buffer_t /*token_buffer*/,
	int * qop_state );

OM_uint32
_gsskrb5_verify_8003_checksum (
	 OM_uint32 */*minor_status*/,
	const gss_channel_bindings_t /*input_chan_bindings*/,
	const Checksum */*cksum*/,
	OM_uint32 */*flags*/,
	krb5_data */*fwd_data*/);

OM_uint32
_gsskrb5_verify_header (
	u_char **/*str*/,
	size_t /*total_len*/,
	const void */*type*/,
	gss_OID /*oid*/);

OM_uint32
_gsskrb5_verify_mic (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	const gss_buffer_t /*message_buffer*/,
	const gss_buffer_t /*token_buffer*/,
	gss_qop_t * qop_state );

OM_uint32
_gsskrb5_verify_mic_internal (
	OM_uint32 * /*minor_status*/,
	const gsskrb5_ctx /*context_handle*/,
	krb5_context /*context*/,
	const gss_buffer_t /*message_buffer*/,
	const gss_buffer_t /*token_buffer*/,
	gss_qop_t * /*qop_state*/,
	char * type );

OM_uint32
_gsskrb5_wrap (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	const gss_buffer_t /*input_message_buffer*/,
	int * /*conf_state*/,
	gss_buffer_t output_message_buffer );

OM_uint32
_gsskrb5_wrap_size_limit (
	 OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	OM_uint32 /*req_output_size*/,
	OM_uint32 * max_input_size );

krb5_error_code
_gsskrb5cfx_max_wrap_length_cfx (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	int /*conf_req_flag*/,
	size_t /*input_length*/,
	OM_uint32 */*output_length*/);

krb5_error_code
_gsskrb5cfx_wrap_length_cfx (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	int /*conf_req_flag*/,
	size_t /*input_length*/,
	size_t */*output_length*/,
	size_t */*cksumsize*/,
	uint16_t */*padlength*/);

krb5_error_code
_gsskrb5i_address_to_krb5addr (
	krb5_context /*context*/,
	OM_uint32 /*gss_addr_type*/,
	gss_buffer_desc */*gss_addr*/,
	int16_t /*port*/,
	krb5_address */*address*/);

krb5_error_code
_gsskrb5i_get_acceptor_subkey (
	const gsskrb5_ctx /*ctx*/,
	krb5_context /*context*/,
	krb5_keyblock **/*key*/);

krb5_error_code
_gsskrb5i_get_initiator_subkey (
	const gsskrb5_ctx /*ctx*/,
	krb5_context /*context*/,
	krb5_keyblock **/*key*/);

OM_uint32
_gsskrb5i_get_token_key (
	const gsskrb5_ctx /*ctx*/,
	krb5_context /*context*/,
	krb5_keyblock **/*key*/);

void
_gsskrb5i_is_cfx (
	gsskrb5_ctx /*ctx*/,
	int */*is_cfx*/);

#endif /* __gsskrb5_private_h__ */
