/* This is a generated file */
#ifndef __krb5_protos_h__
#define __krb5_protos_h__

#include <stdarg.h>

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

krb5_error_code
krb524_convert_creds_kdc (
	krb5_context /*context*/,
	krb5_creds */*in_cred*/,
	struct credentials */*v4creds*/);

krb5_error_code
krb524_convert_creds_kdc_ccache (
	krb5_context /*context*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*in_cred*/,
	struct credentials */*v4creds*/);

krb5_error_code
krb5_425_conv_principal (
	krb5_context /*context*/,
	const char */*name*/,
	const char */*instance*/,
	const char */*realm*/,
	krb5_principal */*princ*/);

krb5_error_code
krb5_425_conv_principal_ext (
	krb5_context /*context*/,
	const char */*name*/,
	const char */*instance*/,
	const char */*realm*/,
	krb5_boolean (*/*func*/)(krb5_context, krb5_principal),
	krb5_boolean /*resolve*/,
	krb5_principal */*princ*/);

krb5_error_code
krb5_524_conv_principal (
	krb5_context /*context*/,
	const krb5_principal /*principal*/,
	char */*name*/,
	char */*instance*/,
	char */*realm*/);

krb5_error_code
krb5_abort (
	krb5_context /*context*/,
	krb5_error_code /*code*/,
	const char */*fmt*/,
	...)
    __attribute__ ((noreturn, format (printf, 3, 4)));

krb5_error_code
krb5_abortx (
	krb5_context /*context*/,
	const char */*fmt*/,
	...)
    __attribute__ ((noreturn, format (printf, 2, 3)));

krb5_error_code
krb5_acl_match_file (
	krb5_context /*context*/,
	const char */*file*/,
	const char */*format*/,
	...);

krb5_error_code
krb5_acl_match_string (
	krb5_context /*context*/,
	const char */*string*/,
	const char */*format*/,
	...);

krb5_error_code
krb5_add_et_list (
	krb5_context /*context*/,
	void (*/*func*/)(struct et_list **));

krb5_error_code
krb5_add_extra_addresses (
	krb5_context /*context*/,
	krb5_addresses */*addresses*/);

krb5_error_code
krb5_add_ignore_addresses (
	krb5_context /*context*/,
	krb5_addresses */*addresses*/);

krb5_error_code
krb5_addlog_dest (
	krb5_context /*context*/,
	krb5_log_facility */*f*/,
	const char */*orig*/);

krb5_error_code
krb5_addlog_func (
	krb5_context /*context*/,
	krb5_log_facility */*fac*/,
	int /*min*/,
	int /*max*/,
	krb5_log_log_func_t /*log*/,
	krb5_log_close_func_t /*close*/,
	void */*data*/);

krb5_error_code
krb5_addr2sockaddr (
	krb5_context /*context*/,
	const krb5_address */*addr*/,
	struct sockaddr */*sa*/,
	krb5_socklen_t */*sa_size*/,
	int /*port*/);

krb5_boolean
krb5_address_compare (
	krb5_context /*context*/,
	const krb5_address */*addr1*/,
	const krb5_address */*addr2*/);

int
krb5_address_order (
	krb5_context /*context*/,
	const krb5_address */*addr1*/,
	const krb5_address */*addr2*/);

krb5_boolean
krb5_address_search (
	krb5_context /*context*/,
	const krb5_address */*addr*/,
	const krb5_addresses */*addrlist*/);

krb5_error_code
krb5_aname_to_localname (
	krb5_context /*context*/,
	krb5_const_principal /*aname*/,
	size_t /*lnsize*/,
	char */*lname*/);

krb5_error_code
krb5_anyaddr (
	krb5_context /*context*/,
	int /*af*/,
	struct sockaddr */*sa*/,
	krb5_socklen_t */*sa_size*/,
	int /*port*/);

void
krb5_appdefault_boolean (
	krb5_context /*context*/,
	const char */*appname*/,
	krb5_const_realm /*realm*/,
	const char */*option*/,
	krb5_boolean /*def_val*/,
	krb5_boolean */*ret_val*/);

void
krb5_appdefault_string (
	krb5_context /*context*/,
	const char */*appname*/,
	krb5_const_realm /*realm*/,
	const char */*option*/,
	const char */*def_val*/,
	char **/*ret_val*/);

void
krb5_appdefault_time (
	krb5_context /*context*/,
	const char */*appname*/,
	krb5_const_realm /*realm*/,
	const char */*option*/,
	time_t /*def_val*/,
	time_t */*ret_val*/);

krb5_error_code
krb5_append_addresses (
	krb5_context /*context*/,
	krb5_addresses */*dest*/,
	const krb5_addresses */*source*/);

krb5_error_code
krb5_auth_con_free (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/);

krb5_error_code
krb5_auth_con_genaddrs (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	int /*fd*/,
	int /*flags*/);

krb5_error_code
krb5_auth_con_generatelocalsubkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock */*key*/);

krb5_error_code
krb5_auth_con_getaddrs (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_address **/*local_addr*/,
	krb5_address **/*remote_addr*/);

krb5_error_code
krb5_auth_con_getauthenticator (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_authenticator */*authenticator*/);

krb5_error_code
krb5_auth_con_getcksumtype (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_cksumtype */*cksumtype*/);

krb5_error_code
krb5_auth_con_getflags (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	int32_t */*flags*/);

krb5_error_code
krb5_auth_con_getkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock **/*keyblock*/);

krb5_error_code
krb5_auth_con_getkeytype (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keytype */*keytype*/);

krb5_error_code
krb5_auth_con_getlocalseqnumber (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	int32_t */*seqnumber*/);

krb5_error_code
krb5_auth_con_getlocalsubkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock **/*keyblock*/);

krb5_error_code
krb5_auth_con_getrcache (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_rcache */*rcache*/);

krb5_error_code
krb5_auth_con_getremotesubkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock **/*keyblock*/);

krb5_error_code
krb5_auth_con_init (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/);

krb5_error_code
krb5_auth_con_setaddrs (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_address */*local_addr*/,
	krb5_address */*remote_addr*/);

krb5_error_code
krb5_auth_con_setaddrs_from_fd (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	void */*p_fd*/);

krb5_error_code
krb5_auth_con_setcksumtype (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_cksumtype /*cksumtype*/);

krb5_error_code
krb5_auth_con_setflags (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	int32_t /*flags*/);

krb5_error_code
krb5_auth_con_setkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock */*keyblock*/);

krb5_error_code
krb5_auth_con_setkeytype (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keytype /*keytype*/);

krb5_error_code
krb5_auth_con_setlocalseqnumber (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	int32_t /*seqnumber*/);

krb5_error_code
krb5_auth_con_setlocalsubkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock */*keyblock*/);

krb5_error_code
krb5_auth_con_setrcache (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_rcache /*rcache*/);

krb5_error_code
krb5_auth_con_setremoteseqnumber (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	int32_t /*seqnumber*/);

krb5_error_code
krb5_auth_con_setremotesubkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock */*keyblock*/);

krb5_error_code
krb5_auth_con_setuserkey (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_keyblock */*keyblock*/);

krb5_error_code
krb5_auth_getremoteseqnumber (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	int32_t */*seqnumber*/);

krb5_error_code
krb5_build_ap_req (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/,
	krb5_creds */*cred*/,
	krb5_flags /*ap_options*/,
	krb5_data /*authenticator*/,
	krb5_data */*retdata*/);

krb5_error_code
krb5_build_authenticator (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_enctype /*enctype*/,
	krb5_creds */*cred*/,
	Checksum */*cksum*/,
	Authenticator **/*auth_result*/,
	krb5_data */*result*/,
	krb5_key_usage /*usage*/);

krb5_error_code
krb5_build_principal (
	krb5_context /*context*/,
	krb5_principal */*principal*/,
	int /*rlen*/,
	krb5_const_realm /*realm*/,
	...);

krb5_error_code
krb5_build_principal_ext (
	krb5_context /*context*/,
	krb5_principal */*principal*/,
	int /*rlen*/,
	krb5_const_realm /*realm*/,
	...);

krb5_error_code
krb5_build_principal_va (
	krb5_context /*context*/,
	krb5_principal */*principal*/,
	int /*rlen*/,
	krb5_const_realm /*realm*/,
	va_list /*ap*/);

krb5_error_code
krb5_build_principal_va_ext (
	krb5_context /*context*/,
	krb5_principal */*principal*/,
	int /*rlen*/,
	krb5_const_realm /*realm*/,
	va_list /*ap*/);

krb5_error_code
krb5_cc_close (
	krb5_context /*context*/,
	krb5_ccache /*id*/);

krb5_error_code
krb5_cc_copy_cache (
	krb5_context /*context*/,
	const krb5_ccache /*from*/,
	krb5_ccache /*to*/);

krb5_error_code
krb5_cc_default (
	krb5_context /*context*/,
	krb5_ccache */*id*/);

const char*
krb5_cc_default_name (krb5_context /*context*/);

krb5_error_code
krb5_cc_destroy (
	krb5_context /*context*/,
	krb5_ccache /*id*/);

krb5_error_code
krb5_cc_end_seq_get (
	krb5_context /*context*/,
	const krb5_ccache /*id*/,
	krb5_cc_cursor */*cursor*/);

krb5_error_code
krb5_cc_gen_new (
	krb5_context /*context*/,
	const krb5_cc_ops */*ops*/,
	krb5_ccache */*id*/);

const char*
krb5_cc_get_name (
	krb5_context /*context*/,
	krb5_ccache /*id*/);

krb5_error_code
krb5_cc_get_principal (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_principal */*principal*/);

const char*
krb5_cc_get_type (
	krb5_context /*context*/,
	krb5_ccache /*id*/);

krb5_error_code
krb5_cc_get_version (
	krb5_context /*context*/,
	const krb5_ccache /*id*/);

krb5_error_code
krb5_cc_initialize (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_principal /*primary_principal*/);

krb5_error_code
krb5_cc_next_cred (
	krb5_context /*context*/,
	const krb5_ccache /*id*/,
	krb5_cc_cursor */*cursor*/,
	krb5_creds */*creds*/);

krb5_error_code
krb5_cc_register (
	krb5_context /*context*/,
	const krb5_cc_ops */*ops*/,
	krb5_boolean /*override*/);

krb5_error_code
krb5_cc_remove_cred (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_flags /*which*/,
	krb5_creds */*cred*/);

krb5_error_code
krb5_cc_resolve (
	krb5_context /*context*/,
	const char */*name*/,
	krb5_ccache */*id*/);

krb5_error_code
krb5_cc_retrieve_cred (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_flags /*whichfields*/,
	const krb5_creds */*mcreds*/,
	krb5_creds */*creds*/);

krb5_error_code
krb5_cc_set_flags (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_flags /*flags*/);

krb5_error_code
krb5_cc_start_seq_get (
	krb5_context /*context*/,
	const krb5_ccache /*id*/,
	krb5_cc_cursor */*cursor*/);

krb5_error_code
krb5_cc_store_cred (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_creds */*creds*/);

krb5_error_code
krb5_change_password (
	krb5_context /*context*/,
	krb5_creds */*creds*/,
	char */*newpw*/,
	int */*result_code*/,
	krb5_data */*result_code_string*/,
	krb5_data */*result_string*/);

krb5_error_code
krb5_check_transited_realms (
	krb5_context /*context*/,
	const char *const */*realms*/,
	int /*num_realms*/,
	int */*bad_realm*/);

krb5_boolean
krb5_checksum_is_collision_proof (
	krb5_context /*context*/,
	krb5_cksumtype /*type*/);

krb5_boolean
krb5_checksum_is_keyed (
	krb5_context /*context*/,
	krb5_cksumtype /*type*/);

krb5_error_code
krb5_checksumsize (
	krb5_context /*context*/,
	krb5_cksumtype /*type*/,
	size_t */*size*/);

void
krb5_clear_error_string (krb5_context /*context*/);

krb5_error_code
krb5_closelog (
	krb5_context /*context*/,
	krb5_log_facility */*fac*/);

krb5_boolean
krb5_compare_creds (
	krb5_context /*context*/,
	krb5_flags /*whichfields*/,
	const krb5_creds */*mcreds*/,
	const krb5_creds */*creds*/);

krb5_error_code
krb5_config_file_free (
	krb5_context /*context*/,
	krb5_config_section */*s*/);

void
krb5_config_free_strings (char **/*strings*/);

const void *
krb5_config_get (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*type*/,
	...);

krb5_boolean
krb5_config_get_bool (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	...);

krb5_boolean
krb5_config_get_bool_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	krb5_boolean /*def_value*/,
	...);

int
krb5_config_get_int (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	...);

int
krb5_config_get_int_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*def_value*/,
	...);

const krb5_config_binding *
krb5_config_get_list (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	...);

const void *
krb5_config_get_next (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	const krb5_config_binding **/*pointer*/,
	int /*type*/,
	...);

const char *
krb5_config_get_string (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	...);

const char *
krb5_config_get_string_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	const char */*def_value*/,
	...);

char**
krb5_config_get_strings (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	...);

int
krb5_config_get_time (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	...);

int
krb5_config_get_time_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*def_value*/,
	...);

krb5_error_code
krb5_config_parse_file (
	krb5_context /*context*/,
	const char */*fname*/,
	krb5_config_section **/*res*/);

krb5_error_code
krb5_config_parse_file_multi (
	krb5_context /*context*/,
	const char */*fname*/,
	krb5_config_section **/*res*/);

const void *
krb5_config_vget (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*type*/,
	va_list /*args*/);

krb5_boolean
krb5_config_vget_bool (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	va_list /*args*/);

krb5_boolean
krb5_config_vget_bool_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	krb5_boolean /*def_value*/,
	va_list /*args*/);

int
krb5_config_vget_int (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	va_list /*args*/);

int
krb5_config_vget_int_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*def_value*/,
	va_list /*args*/);

const krb5_config_binding *
krb5_config_vget_list (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	va_list /*args*/);

const void *
krb5_config_vget_next (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	const krb5_config_binding **/*pointer*/,
	int /*type*/,
	va_list /*args*/);

const char *
krb5_config_vget_string (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	va_list /*args*/);

const char *
krb5_config_vget_string_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	const char */*def_value*/,
	va_list /*args*/);

char **
krb5_config_vget_strings (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	va_list /*args*/);

int
krb5_config_vget_time (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	va_list /*args*/);

int
krb5_config_vget_time_default (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*def_value*/,
	va_list /*args*/);

krb5_error_code
krb5_copy_address (
	krb5_context /*context*/,
	const krb5_address */*inaddr*/,
	krb5_address */*outaddr*/);

krb5_error_code
krb5_copy_addresses (
	krb5_context /*context*/,
	const krb5_addresses */*inaddr*/,
	krb5_addresses */*outaddr*/);

krb5_error_code
krb5_copy_creds (
	krb5_context /*context*/,
	const krb5_creds */*incred*/,
	krb5_creds **/*outcred*/);

krb5_error_code
krb5_copy_creds_contents (
	krb5_context /*context*/,
	const krb5_creds */*incred*/,
	krb5_creds */*c*/);

krb5_error_code
krb5_copy_data (
	krb5_context /*context*/,
	const krb5_data */*indata*/,
	krb5_data **/*outdata*/);

krb5_error_code
krb5_copy_host_realm (
	krb5_context /*context*/,
	const krb5_realm */*from*/,
	krb5_realm **/*to*/);

krb5_error_code
krb5_copy_keyblock (
	krb5_context /*context*/,
	const krb5_keyblock */*inblock*/,
	krb5_keyblock **/*to*/);

krb5_error_code
krb5_copy_keyblock_contents (
	krb5_context /*context*/,
	const krb5_keyblock */*inblock*/,
	krb5_keyblock */*to*/);

krb5_error_code
krb5_copy_principal (
	krb5_context /*context*/,
	krb5_const_principal /*inprinc*/,
	krb5_principal */*outprinc*/);

krb5_error_code
krb5_copy_ticket (
	krb5_context /*context*/,
	const krb5_ticket */*from*/,
	krb5_ticket **/*to*/);

krb5_error_code
krb5_create_checksum (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	krb5_key_usage /*usage*/,
	int /*type*/,
	void */*data*/,
	size_t /*len*/,
	Checksum */*result*/);

krb5_error_code
krb5_crypto_destroy (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/);

krb5_error_code
krb5_crypto_getblocksize (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	size_t */*blocksize*/);

krb5_error_code
krb5_crypto_init (
	krb5_context /*context*/,
	const krb5_keyblock */*key*/,
	krb5_enctype /*etype*/,
	krb5_crypto */*crypto*/);

krb5_error_code
krb5_data_alloc (
	krb5_data */*p*/,
	int /*len*/);

krb5_error_code
krb5_data_copy (
	krb5_data */*p*/,
	const void */*data*/,
	size_t /*len*/);

void
krb5_data_free (krb5_data */*p*/);

krb5_error_code
krb5_data_realloc (
	krb5_data */*p*/,
	int /*len*/);

void
krb5_data_zero (krb5_data */*p*/);

krb5_error_code
krb5_decode_Authenticator (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	Authenticator */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_decode_ETYPE_INFO (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	ETYPE_INFO */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_decode_EncAPRepPart (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	EncAPRepPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_decode_EncASRepPart (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	EncASRepPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_decode_EncKrbCredPart (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	EncKrbCredPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_decode_EncTGSRepPart (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	EncTGSRepPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_decode_EncTicketPart (
	krb5_context /*context*/,
	const void */*data*/,
	size_t /*length*/,
	EncTicketPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_decode_ap_req (
	krb5_context /*context*/,
	const krb5_data */*inbuf*/,
	krb5_ap_req */*ap_req*/);

krb5_error_code
krb5_decrypt (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	unsigned /*usage*/,
	void */*data*/,
	size_t /*len*/,
	krb5_data */*result*/);

krb5_error_code
krb5_decrypt_EncryptedData (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	unsigned /*usage*/,
	const EncryptedData */*e*/,
	krb5_data */*result*/);

krb5_error_code
krb5_decrypt_ivec (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	unsigned /*usage*/,
	void */*data*/,
	size_t /*len*/,
	krb5_data */*result*/,
	void */*ivec*/);

krb5_error_code
krb5_decrypt_ticket (
	krb5_context /*context*/,
	Ticket */*ticket*/,
	krb5_keyblock */*key*/,
	EncTicketPart */*out*/,
	krb5_flags /*flags*/);

krb5_error_code
krb5_derive_key (
	krb5_context /*context*/,
	const krb5_keyblock */*key*/,
	krb5_enctype /*etype*/,
	const void */*constant*/,
	size_t /*constant_len*/,
	krb5_keyblock **/*derived_key*/);

krb5_error_code
krb5_domain_x500_decode (
	krb5_context /*context*/,
	krb5_data /*tr*/,
	char ***/*realms*/,
	int */*num_realms*/,
	const char */*client_realm*/,
	const char */*server_realm*/);

krb5_error_code
krb5_domain_x500_encode (
	char **/*realms*/,
	int /*num_realms*/,
	krb5_data */*encoding*/);

krb5_error_code
krb5_eai_to_heim_errno (
	int /*eai_errno*/,
	int /*system_error*/);

krb5_error_code
krb5_encode_Authenticator (
	krb5_context /*context*/,
	void */*data*/,
	size_t /*length*/,
	Authenticator */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_encode_ETYPE_INFO (
	krb5_context /*context*/,
	void */*data*/,
	size_t /*length*/,
	ETYPE_INFO */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_encode_EncAPRepPart (
	krb5_context /*context*/,
	void */*data*/,
	size_t /*length*/,
	EncAPRepPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_encode_EncASRepPart (
	krb5_context /*context*/,
	void */*data*/,
	size_t /*length*/,
	EncASRepPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_encode_EncKrbCredPart (
	krb5_context /*context*/,
	void */*data*/,
	size_t /*length*/,
	EncKrbCredPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_encode_EncTGSRepPart (
	krb5_context /*context*/,
	void */*data*/,
	size_t /*length*/,
	EncTGSRepPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_encode_EncTicketPart (
	krb5_context /*context*/,
	void */*data*/,
	size_t /*length*/,
	EncTicketPart */*t*/,
	size_t */*len*/);

krb5_error_code
krb5_encrypt (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	unsigned /*usage*/,
	void */*data*/,
	size_t /*len*/,
	krb5_data */*result*/);

krb5_error_code
krb5_encrypt_EncryptedData (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	unsigned /*usage*/,
	void */*data*/,
	size_t /*len*/,
	int /*kvno*/,
	EncryptedData */*result*/);

krb5_error_code
krb5_encrypt_ivec (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	unsigned /*usage*/,
	void */*data*/,
	size_t /*len*/,
	krb5_data */*result*/,
	void */*ivec*/);

krb5_error_code
krb5_enctype_to_keytype (
	krb5_context /*context*/,
	krb5_enctype /*etype*/,
	krb5_keytype */*keytype*/);

krb5_error_code
krb5_enctype_to_string (
	krb5_context /*context*/,
	krb5_enctype /*etype*/,
	char **/*string*/);

krb5_error_code
krb5_enctype_valid (
	krb5_context /*context*/,
	krb5_enctype /*etype*/);

krb5_boolean
krb5_enctypes_compatible_keys (
	krb5_context /*context*/,
	krb5_enctype /*etype1*/,
	krb5_enctype /*etype2*/);

krb5_error_code
krb5_err (
	krb5_context /*context*/,
	int /*eval*/,
	krb5_error_code /*code*/,
	const char */*fmt*/,
	...)
    __attribute__ ((noreturn, format (printf, 4, 5)));

krb5_error_code
krb5_error_from_rd_error (
	krb5_context /*context*/,
	const krb5_error */*error*/,
	const krb5_creds */*creds*/);

krb5_error_code
krb5_errx (
	krb5_context /*context*/,
	int /*eval*/,
	const char */*fmt*/,
	...)
    __attribute__ ((noreturn, format (printf, 3, 4)));

krb5_error_code
krb5_expand_hostname (
	krb5_context /*context*/,
	const char */*orig_hostname*/,
	char **/*new_hostname*/);

krb5_error_code
krb5_expand_hostname_realms (
	krb5_context /*context*/,
	const char */*orig_hostname*/,
	char **/*new_hostname*/,
	char ***/*realms*/);

PA_DATA *
krb5_find_padata (
	PA_DATA */*val*/,
	unsigned /*len*/,
	int /*type*/,
	int */*index*/);

krb5_error_code
krb5_format_time (
	krb5_context /*context*/,
	time_t /*t*/,
	char */*s*/,
	size_t /*len*/,
	krb5_boolean /*include_time*/);

krb5_error_code
krb5_free_address (
	krb5_context /*context*/,
	krb5_address */*address*/);

krb5_error_code
krb5_free_addresses (
	krb5_context /*context*/,
	krb5_addresses */*addresses*/);

void
krb5_free_ap_rep_enc_part (
	krb5_context /*context*/,
	krb5_ap_rep_enc_part */*val*/);

void
krb5_free_authenticator (
	krb5_context /*context*/,
	krb5_authenticator */*authenticator*/);

void
krb5_free_config_files (char **/*filenames*/);

void
krb5_free_context (krb5_context /*context*/);

krb5_error_code
krb5_free_cred_contents (
	krb5_context /*context*/,
	krb5_creds */*c*/);

krb5_error_code
krb5_free_creds (
	krb5_context /*context*/,
	krb5_creds */*c*/);

krb5_error_code
krb5_free_creds_contents (
	krb5_context /*context*/,
	krb5_creds */*c*/);

void
krb5_free_data (
	krb5_context /*context*/,
	krb5_data */*p*/);

void
krb5_free_error (
	krb5_context /*context*/,
	krb5_error */*error*/);

void
krb5_free_error_contents (
	krb5_context /*context*/,
	krb5_error */*error*/);

void
krb5_free_error_string (
	krb5_context /*context*/,
	char */*str*/);

krb5_error_code
krb5_free_host_realm (
	krb5_context /*context*/,
	krb5_realm */*realmlist*/);

krb5_error_code
krb5_free_kdc_rep (
	krb5_context /*context*/,
	krb5_kdc_rep */*rep*/);

void
krb5_free_keyblock (
	krb5_context /*context*/,
	krb5_keyblock */*keyblock*/);

void
krb5_free_keyblock_contents (
	krb5_context /*context*/,
	krb5_keyblock */*keyblock*/);

krb5_error_code
krb5_free_krbhst (
	krb5_context /*context*/,
	char **/*hostlist*/);

void
krb5_free_principal (
	krb5_context /*context*/,
	krb5_principal /*p*/);

krb5_error_code
krb5_free_salt (
	krb5_context /*context*/,
	krb5_salt /*salt*/);

krb5_error_code
krb5_free_ticket (
	krb5_context /*context*/,
	krb5_ticket */*ticket*/);

krb5_error_code
krb5_fwd_tgt_creds (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	const char */*hostname*/,
	krb5_principal /*client*/,
	krb5_principal /*server*/,
	krb5_ccache /*ccache*/,
	int /*forwardable*/,
	krb5_data */*out_data*/);

void
krb5_generate_random_block (
	void */*buf*/,
	size_t /*len*/);

krb5_error_code
krb5_generate_random_keyblock (
	krb5_context /*context*/,
	krb5_enctype /*type*/,
	krb5_keyblock */*key*/);

krb5_error_code
krb5_generate_seq_number (
	krb5_context /*context*/,
	const krb5_keyblock */*key*/,
	u_int32_t */*seqno*/);

krb5_error_code
krb5_generate_subkey (
	krb5_context /*context*/,
	const krb5_keyblock */*key*/,
	krb5_keyblock **/*subkey*/);

krb5_error_code
krb5_get_all_client_addrs (
	krb5_context /*context*/,
	krb5_addresses */*res*/);

krb5_error_code
krb5_get_all_server_addrs (
	krb5_context /*context*/,
	krb5_addresses */*res*/);

krb5_error_code
krb5_get_cred_from_kdc (
	krb5_context /*context*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*in_creds*/,
	krb5_creds **/*out_creds*/,
	krb5_creds ***/*ret_tgts*/);

krb5_error_code
krb5_get_cred_from_kdc_opt (
	krb5_context /*context*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*in_creds*/,
	krb5_creds **/*out_creds*/,
	krb5_creds ***/*ret_tgts*/,
	krb5_flags /*flags*/);

krb5_error_code
krb5_get_credentials (
	krb5_context /*context*/,
	krb5_flags /*options*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*in_creds*/,
	krb5_creds **/*out_creds*/);

krb5_error_code
krb5_get_credentials_with_flags (
	krb5_context /*context*/,
	krb5_flags /*options*/,
	krb5_kdc_flags /*flags*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*in_creds*/,
	krb5_creds **/*out_creds*/);

krb5_error_code
krb5_get_default_config_files (char ***/*pfilenames*/);

krb5_error_code
krb5_get_default_in_tkt_etypes (
	krb5_context /*context*/,
	krb5_enctype **/*etypes*/);

krb5_error_code
krb5_get_default_principal (
	krb5_context /*context*/,
	krb5_principal */*princ*/);

krb5_error_code
krb5_get_default_realm (
	krb5_context /*context*/,
	krb5_realm */*realm*/);

krb5_error_code
krb5_get_default_realms (
	krb5_context /*context*/,
	krb5_realm **/*realms*/);

const char *
krb5_get_err_text (
	krb5_context /*context*/,
	krb5_error_code /*code*/);

char*
krb5_get_error_string (krb5_context /*context*/);

krb5_error_code
krb5_get_extra_addresses (
	krb5_context /*context*/,
	krb5_addresses */*addresses*/);

krb5_error_code
krb5_get_fcache_version (
	krb5_context /*context*/,
	int */*version*/);

krb5_error_code
krb5_get_forwarded_creds (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_ccache /*ccache*/,
	krb5_flags /*flags*/,
	const char */*hostname*/,
	krb5_creds */*in_creds*/,
	krb5_data */*out_data*/);

krb5_error_code
krb5_get_host_realm (
	krb5_context /*context*/,
	const char */*host*/,
	krb5_realm **/*realms*/);

krb5_error_code
krb5_get_host_realm_int (
	krb5_context /*context*/,
	const char */*host*/,
	krb5_boolean /*use_dns*/,
	krb5_realm **/*realms*/);

krb5_error_code
krb5_get_ignore_addresses (
	krb5_context /*context*/,
	krb5_addresses */*addresses*/);

krb5_error_code
krb5_get_in_cred (
	krb5_context /*context*/,
	krb5_flags /*options*/,
	const krb5_addresses */*addrs*/,
	const krb5_enctype */*etypes*/,
	const krb5_preauthtype */*ptypes*/,
	const krb5_preauthdata */*preauth*/,
	krb5_key_proc /*key_proc*/,
	krb5_const_pointer /*keyseed*/,
	krb5_decrypt_proc /*decrypt_proc*/,
	krb5_const_pointer /*decryptarg*/,
	krb5_creds */*creds*/,
	krb5_kdc_rep */*ret_as_reply*/);

krb5_error_code
krb5_get_in_tkt (
	krb5_context /*context*/,
	krb5_flags /*options*/,
	const krb5_addresses */*addrs*/,
	const krb5_enctype */*etypes*/,
	const krb5_preauthtype */*ptypes*/,
	krb5_key_proc /*key_proc*/,
	krb5_const_pointer /*keyseed*/,
	krb5_decrypt_proc /*decrypt_proc*/,
	krb5_const_pointer /*decryptarg*/,
	krb5_creds */*creds*/,
	krb5_ccache /*ccache*/,
	krb5_kdc_rep */*ret_as_reply*/);

krb5_error_code
krb5_get_in_tkt_with_keytab (
	krb5_context /*context*/,
	krb5_flags /*options*/,
	krb5_addresses */*addrs*/,
	const krb5_enctype */*etypes*/,
	const krb5_preauthtype */*pre_auth_types*/,
	krb5_keytab /*keytab*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*creds*/,
	krb5_kdc_rep */*ret_as_reply*/);

krb5_error_code
krb5_get_in_tkt_with_password (
	krb5_context /*context*/,
	krb5_flags /*options*/,
	krb5_addresses */*addrs*/,
	const krb5_enctype */*etypes*/,
	const krb5_preauthtype */*pre_auth_types*/,
	const char */*password*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*creds*/,
	krb5_kdc_rep */*ret_as_reply*/);

krb5_error_code
krb5_get_in_tkt_with_skey (
	krb5_context /*context*/,
	krb5_flags /*options*/,
	krb5_addresses */*addrs*/,
	const krb5_enctype */*etypes*/,
	const krb5_preauthtype */*pre_auth_types*/,
	const krb5_keyblock */*key*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*creds*/,
	krb5_kdc_rep */*ret_as_reply*/);

krb5_error_code
krb5_get_init_creds_keytab (
	krb5_context /*context*/,
	krb5_creds */*creds*/,
	krb5_principal /*client*/,
	krb5_keytab /*keytab*/,
	krb5_deltat /*start_time*/,
	const char */*in_tkt_service*/,
	krb5_get_init_creds_opt */*options*/);

void
krb5_get_init_creds_opt_init (krb5_get_init_creds_opt */*opt*/);

void
krb5_get_init_creds_opt_set_address_list (
	krb5_get_init_creds_opt */*opt*/,
	krb5_addresses */*addresses*/);

void
krb5_get_init_creds_opt_set_anonymous (
	krb5_get_init_creds_opt */*opt*/,
	int /*anonymous*/);

void
krb5_get_init_creds_opt_set_default_flags (
	krb5_context /*context*/,
	const char */*appname*/,
	krb5_const_realm /*realm*/,
	krb5_get_init_creds_opt */*opt*/);

void
krb5_get_init_creds_opt_set_etype_list (
	krb5_get_init_creds_opt */*opt*/,
	krb5_enctype */*etype_list*/,
	int /*etype_list_length*/);

void
krb5_get_init_creds_opt_set_forwardable (
	krb5_get_init_creds_opt */*opt*/,
	int /*forwardable*/);

void
krb5_get_init_creds_opt_set_preauth_list (
	krb5_get_init_creds_opt */*opt*/,
	krb5_preauthtype */*preauth_list*/,
	int /*preauth_list_length*/);

void
krb5_get_init_creds_opt_set_proxiable (
	krb5_get_init_creds_opt */*opt*/,
	int /*proxiable*/);

void
krb5_get_init_creds_opt_set_renew_life (
	krb5_get_init_creds_opt */*opt*/,
	krb5_deltat /*renew_life*/);

void
krb5_get_init_creds_opt_set_salt (
	krb5_get_init_creds_opt */*opt*/,
	krb5_data */*salt*/);

void
krb5_get_init_creds_opt_set_tkt_life (
	krb5_get_init_creds_opt */*opt*/,
	krb5_deltat /*tkt_life*/);

krb5_error_code
krb5_get_init_creds_password (
	krb5_context /*context*/,
	krb5_creds */*creds*/,
	krb5_principal /*client*/,
	const char */*password*/,
	krb5_prompter_fct /*prompter*/,
	void */*data*/,
	krb5_deltat /*start_time*/,
	const char */*in_tkt_service*/,
	krb5_get_init_creds_opt */*options*/);

krb5_error_code
krb5_get_kdc_cred (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_kdc_flags /*flags*/,
	krb5_addresses */*addresses*/,
	Ticket */*second_ticket*/,
	krb5_creds */*in_creds*/,
	krb5_creds **out_creds );

krb5_error_code
krb5_get_krb524hst (
	krb5_context /*context*/,
	const krb5_realm */*realm*/,
	char ***/*hostlist*/);

krb5_error_code
krb5_get_krb_admin_hst (
	krb5_context /*context*/,
	const krb5_realm */*realm*/,
	char ***/*hostlist*/);

krb5_error_code
krb5_get_krb_changepw_hst (
	krb5_context /*context*/,
	const krb5_realm */*realm*/,
	char ***/*hostlist*/);

krb5_error_code
krb5_get_krbhst (
	krb5_context /*context*/,
	const krb5_realm */*realm*/,
	char ***/*hostlist*/);

krb5_error_code
krb5_get_pw_salt (
	krb5_context /*context*/,
	krb5_const_principal /*principal*/,
	krb5_salt */*salt*/);

krb5_error_code
krb5_get_server_rcache (
	krb5_context /*context*/,
	const krb5_data */*piece*/,
	krb5_rcache */*id*/);

krb5_boolean
krb5_get_use_admin_kdc (krb5_context /*context*/);

size_t
krb5_get_wrapped_length (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	size_t /*data_len*/);

int
krb5_getportbyname (
	krb5_context /*context*/,
	const char */*service*/,
	const char */*proto*/,
	int /*default_port*/);

krb5_error_code
krb5_h_addr2addr (
	krb5_context /*context*/,
	int /*af*/,
	const char */*haddr*/,
	krb5_address */*addr*/);

krb5_error_code
krb5_h_addr2sockaddr (
	krb5_context /*context*/,
	int /*af*/,
	const char */*addr*/,
	struct sockaddr */*sa*/,
	krb5_socklen_t */*sa_size*/,
	int /*port*/);

krb5_error_code
krb5_h_errno_to_heim_errno (int /*eai_errno*/);

krb5_boolean
krb5_have_error_string (krb5_context /*context*/);

krb5_error_code
krb5_init_context (krb5_context */*context*/);

void
krb5_init_ets (krb5_context /*context*/);

krb5_error_code
krb5_init_etype (
	krb5_context /*context*/,
	unsigned */*len*/,
	krb5_enctype **/*val*/,
	const krb5_enctype */*etypes*/);

krb5_error_code
krb5_initlog (
	krb5_context /*context*/,
	const char */*program*/,
	krb5_log_facility **/*fac*/);

krb5_error_code
krb5_keyblock_key_proc (
	krb5_context /*context*/,
	krb5_keytype /*type*/,
	krb5_data */*salt*/,
	krb5_const_pointer /*keyseed*/,
	krb5_keyblock **/*key*/);

krb5_error_code
krb5_keytab_key_proc (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/,
	krb5_salt /*salt*/,
	krb5_const_pointer /*keyseed*/,
	krb5_keyblock **/*key*/);

krb5_error_code
krb5_keytype_to_enctypes (
	krb5_context /*context*/,
	krb5_keytype /*keytype*/,
	unsigned */*len*/,
	krb5_enctype **/*val*/);

krb5_error_code
krb5_keytype_to_enctypes_default (
	krb5_context /*context*/,
	krb5_keytype /*keytype*/,
	unsigned */*len*/,
	krb5_enctype **/*val*/);

krb5_error_code
krb5_keytype_to_string (
	krb5_context /*context*/,
	krb5_keytype /*keytype*/,
	char **/*string*/);

krb5_error_code
krb5_krbhst_format_string (
	krb5_context /*context*/,
	const krb5_krbhst_info */*host*/,
	char */*hostname*/,
	size_t /*hostlen*/);

void
krb5_krbhst_free (
	krb5_context /*context*/,
	krb5_krbhst_handle /*handle*/);

krb5_error_code
krb5_krbhst_get_addrinfo (
	krb5_context /*context*/,
	krb5_krbhst_info */*host*/,
	struct addrinfo **/*ai*/);

krb5_error_code
krb5_krbhst_init (
	krb5_context /*context*/,
	const char */*realm*/,
	unsigned int /*type*/,
	krb5_krbhst_handle */*handle*/);

krb5_error_code
krb5_krbhst_next (
	krb5_context /*context*/,
	krb5_krbhst_handle /*handle*/,
	krb5_krbhst_info **/*host*/);

krb5_error_code
krb5_krbhst_next_as_string (
	krb5_context /*context*/,
	krb5_krbhst_handle /*handle*/,
	char */*hostname*/,
	size_t /*hostlen*/);

void
krb5_krbhst_reset (
	krb5_context /*context*/,
	krb5_krbhst_handle /*handle*/);

krb5_error_code
krb5_kt_add_entry (
	krb5_context /*context*/,
	krb5_keytab /*id*/,
	krb5_keytab_entry */*entry*/);

krb5_error_code
krb5_kt_close (
	krb5_context /*context*/,
	krb5_keytab /*id*/);

krb5_boolean
krb5_kt_compare (
	krb5_context /*context*/,
	krb5_keytab_entry */*entry*/,
	krb5_const_principal /*principal*/,
	krb5_kvno /*vno*/,
	krb5_enctype /*enctype*/);

krb5_error_code
krb5_kt_copy_entry_contents (
	krb5_context /*context*/,
	const krb5_keytab_entry */*in*/,
	krb5_keytab_entry */*out*/);

krb5_error_code
krb5_kt_default (
	krb5_context /*context*/,
	krb5_keytab */*id*/);

krb5_error_code
krb5_kt_default_modify_name (
	krb5_context /*context*/,
	char */*name*/,
	size_t /*namesize*/);

krb5_error_code
krb5_kt_default_name (
	krb5_context /*context*/,
	char */*name*/,
	size_t /*namesize*/);

krb5_error_code
krb5_kt_end_seq_get (
	krb5_context /*context*/,
	krb5_keytab /*id*/,
	krb5_kt_cursor */*cursor*/);

krb5_error_code
krb5_kt_free_entry (
	krb5_context /*context*/,
	krb5_keytab_entry */*entry*/);

krb5_error_code
krb5_kt_get_entry (
	krb5_context /*context*/,
	krb5_keytab /*id*/,
	krb5_const_principal /*principal*/,
	krb5_kvno /*kvno*/,
	krb5_enctype /*enctype*/,
	krb5_keytab_entry */*entry*/);

krb5_error_code
krb5_kt_get_name (
	krb5_context /*context*/,
	krb5_keytab /*keytab*/,
	char */*name*/,
	size_t /*namesize*/);

krb5_error_code
krb5_kt_next_entry (
	krb5_context /*context*/,
	krb5_keytab /*id*/,
	krb5_keytab_entry */*entry*/,
	krb5_kt_cursor */*cursor*/);

krb5_error_code
krb5_kt_read_service_key (
	krb5_context /*context*/,
	krb5_pointer /*keyprocarg*/,
	krb5_principal /*principal*/,
	krb5_kvno /*vno*/,
	krb5_enctype /*enctype*/,
	krb5_keyblock **/*key*/);

krb5_error_code
krb5_kt_register (
	krb5_context /*context*/,
	const krb5_kt_ops */*ops*/);

krb5_error_code
krb5_kt_remove_entry (
	krb5_context /*context*/,
	krb5_keytab /*id*/,
	krb5_keytab_entry */*entry*/);

krb5_error_code
krb5_kt_resolve (
	krb5_context /*context*/,
	const char */*name*/,
	krb5_keytab */*id*/);

krb5_error_code
krb5_kt_start_seq_get (
	krb5_context /*context*/,
	krb5_keytab /*id*/,
	krb5_kt_cursor */*cursor*/);

krb5_boolean
krb5_kuserok (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	const char */*luser*/);

krb5_error_code
krb5_log (
	krb5_context /*context*/,
	krb5_log_facility */*fac*/,
	int /*level*/,
	const char */*fmt*/,
	...)
    __attribute__((format (printf, 4, 5)));

krb5_error_code
krb5_log_msg (
	krb5_context /*context*/,
	krb5_log_facility */*fac*/,
	int /*level*/,
	char **/*reply*/,
	const char */*fmt*/,
	...)
    __attribute__((format (printf, 5, 6)));

krb5_error_code
krb5_make_addrport (
	krb5_context /*context*/,
	krb5_address **/*res*/,
	const krb5_address */*addr*/,
	int16_t /*port*/);

krb5_error_code
krb5_make_principal (
	krb5_context /*context*/,
	krb5_principal */*principal*/,
	krb5_const_realm /*realm*/,
	...);

size_t
krb5_max_sockaddr_size (void);

krb5_error_code
krb5_mk_error (
	krb5_context /*context*/,
	krb5_error_code /*error_code*/,
	const char */*e_text*/,
	const krb5_data */*e_data*/,
	const krb5_principal /*client*/,
	const krb5_principal /*server*/,
	time_t */*client_time*/,
	int */*client_usec*/,
	krb5_data */*reply*/);

krb5_error_code
krb5_mk_priv (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	const krb5_data */*userdata*/,
	krb5_data */*outbuf*/,
	void */*outdata*/);

krb5_error_code
krb5_mk_rep (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_data */*outbuf*/);

krb5_error_code
krb5_mk_req (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_flags /*ap_req_options*/,
	const char */*service*/,
	const char */*hostname*/,
	krb5_data */*in_data*/,
	krb5_ccache /*ccache*/,
	krb5_data */*outbuf*/);

krb5_error_code
krb5_mk_req_exact (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_flags /*ap_req_options*/,
	const krb5_principal /*server*/,
	krb5_data */*in_data*/,
	krb5_ccache /*ccache*/,
	krb5_data */*outbuf*/);

krb5_error_code
krb5_mk_req_extended (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_flags /*ap_req_options*/,
	krb5_data */*in_data*/,
	krb5_creds */*in_creds*/,
	krb5_data */*outbuf*/);

krb5_error_code
krb5_mk_req_internal (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_flags /*ap_req_options*/,
	krb5_data */*in_data*/,
	krb5_creds */*in_creds*/,
	krb5_data */*outbuf*/,
	krb5_key_usage /*checksum_usage*/,
	krb5_key_usage /*encrypt_usage*/);

krb5_error_code
krb5_mk_safe (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	const krb5_data */*userdata*/,
	krb5_data */*outbuf*/,
	void */*outdata*/);

krb5_ssize_t
krb5_net_read (
	krb5_context /*context*/,
	void */*p_fd*/,
	void */*buf*/,
	size_t /*len*/);

krb5_ssize_t
krb5_net_write (
	krb5_context /*context*/,
	void */*p_fd*/,
	const void */*buf*/,
	size_t /*len*/);

krb5_error_code
krb5_openlog (
	krb5_context /*context*/,
	const char */*program*/,
	krb5_log_facility **/*fac*/);

krb5_error_code
krb5_parse_address (
	krb5_context /*context*/,
	const char */*string*/,
	krb5_addresses */*addresses*/);

krb5_error_code
krb5_parse_name (
	krb5_context /*context*/,
	const char */*name*/,
	krb5_principal */*principal*/);

const char *
krb5_passwd_result_to_string (
	krb5_context /*context*/,
	int /*result*/);

krb5_error_code
krb5_password_key_proc (
	krb5_context /*context*/,
	krb5_enctype /*type*/,
	krb5_salt /*salt*/,
	krb5_const_pointer /*keyseed*/,
	krb5_keyblock **/*key*/);

krb5_realm*
krb5_princ_realm (
	krb5_context /*context*/,
	krb5_principal /*principal*/);

void
krb5_princ_set_realm (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	krb5_realm */*realm*/);

krb5_error_code
krb5_principal2principalname (
	PrincipalName */*p*/,
	const krb5_principal /*from*/);

krb5_boolean
krb5_principal_compare (
	krb5_context /*context*/,
	krb5_const_principal /*princ1*/,
	krb5_const_principal /*princ2*/);

krb5_boolean
krb5_principal_compare_any_realm (
	krb5_context /*context*/,
	krb5_const_principal /*princ1*/,
	krb5_const_principal /*princ2*/);

const char *
krb5_principal_get_comp_string (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	unsigned int /*component*/);

const char *
krb5_principal_get_realm (
	krb5_context /*context*/,
	krb5_principal /*principal*/);

int
krb5_principal_get_type (
	krb5_context /*context*/,
	krb5_principal /*principal*/);

krb5_boolean
krb5_principal_match (
	krb5_context /*context*/,
	krb5_const_principal /*princ*/,
	krb5_const_principal /*pattern*/);

krb5_error_code
krb5_print_address (
	const krb5_address */*addr*/,
	char */*str*/,
	size_t /*len*/,
	size_t */*ret_len*/);

int
krb5_program_setup (
	krb5_context */*context*/,
	int /*argc*/,
	char **/*argv*/,
	struct getargs */*args*/,
	int /*num_args*/,
	void (*/*usage*/)(int, struct getargs*, int));

int
krb5_prompter_posix (
	krb5_context /*context*/,
	void */*data*/,
	const char */*name*/,
	const char */*banner*/,
	int /*num_prompts*/,
	krb5_prompt prompts[]);

krb5_error_code
krb5_rc_close (
	krb5_context /*context*/,
	krb5_rcache /*id*/);

krb5_error_code
krb5_rc_default (
	krb5_context /*context*/,
	krb5_rcache */*id*/);

const char *
krb5_rc_default_name (krb5_context /*context*/);

const char *
krb5_rc_default_type (krb5_context /*context*/);

krb5_error_code
krb5_rc_destroy (
	krb5_context /*context*/,
	krb5_rcache /*id*/);

krb5_error_code
krb5_rc_expunge (
	krb5_context /*context*/,
	krb5_rcache /*id*/);

krb5_error_code
krb5_rc_get_lifespan (
	krb5_context /*context*/,
	krb5_rcache /*id*/,
	krb5_deltat */*auth_lifespan*/);

const char*
krb5_rc_get_name (
	krb5_context /*context*/,
	krb5_rcache /*id*/);

const char*
krb5_rc_get_type (
	krb5_context /*context*/,
	krb5_rcache /*id*/);

krb5_error_code
krb5_rc_initialize (
	krb5_context /*context*/,
	krb5_rcache /*id*/,
	krb5_deltat /*auth_lifespan*/);

krb5_error_code
krb5_rc_recover (
	krb5_context /*context*/,
	krb5_rcache /*id*/);

krb5_error_code
krb5_rc_resolve (
	krb5_context /*context*/,
	krb5_rcache /*id*/,
	const char */*name*/);

krb5_error_code
krb5_rc_resolve_full (
	krb5_context /*context*/,
	krb5_rcache */*id*/,
	const char */*string_name*/);

krb5_error_code
krb5_rc_resolve_type (
	krb5_context /*context*/,
	krb5_rcache */*id*/,
	const char */*type*/);

krb5_error_code
krb5_rc_store (
	krb5_context /*context*/,
	krb5_rcache /*id*/,
	krb5_donot_replay */*rep*/);

krb5_error_code
krb5_rd_cred (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_data */*in_data*/,
	krb5_creds ***/*ret_creds*/,
	krb5_replay_data */*out_data*/);

krb5_error_code
krb5_rd_cred2 (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_ccache /*ccache*/,
	krb5_data */*in_data*/);

krb5_error_code
krb5_rd_error (
	krb5_context /*context*/,
	krb5_data */*msg*/,
	KRB_ERROR */*result*/);

krb5_error_code
krb5_rd_priv (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	const krb5_data */*inbuf*/,
	krb5_data */*outbuf*/,
	void */*outdata*/);

krb5_error_code
krb5_rd_rep (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	const krb5_data */*inbuf*/,
	krb5_ap_rep_enc_part **/*repl*/);

krb5_error_code
krb5_rd_req (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_data */*inbuf*/,
	krb5_const_principal /*server*/,
	krb5_keytab /*keytab*/,
	krb5_flags */*ap_req_options*/,
	krb5_ticket **/*ticket*/);

krb5_error_code
krb5_rd_req_with_keyblock (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_data */*inbuf*/,
	krb5_const_principal /*server*/,
	krb5_keyblock */*keyblock*/,
	krb5_flags */*ap_req_options*/,
	krb5_ticket **/*ticket*/);

krb5_error_code
krb5_rd_safe (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	const krb5_data */*inbuf*/,
	krb5_data */*outbuf*/,
	void */*outdata*/);

krb5_error_code
krb5_read_message (
	krb5_context /*context*/,
	krb5_pointer /*p_fd*/,
	krb5_data */*data*/);

krb5_error_code
krb5_read_priv_message (
	krb5_context /*context*/,
	krb5_auth_context /*ac*/,
	krb5_pointer /*p_fd*/,
	krb5_data */*data*/);

krb5_error_code
krb5_read_safe_message (
	krb5_context /*context*/,
	krb5_auth_context /*ac*/,
	krb5_pointer /*p_fd*/,
	krb5_data */*data*/);

krb5_boolean
krb5_realm_compare (
	krb5_context /*context*/,
	krb5_const_principal /*princ1*/,
	krb5_const_principal /*princ2*/);

krb5_error_code
krb5_recvauth (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	krb5_pointer /*p_fd*/,
	const char */*appl_version*/,
	krb5_principal /*server*/,
	int32_t /*flags*/,
	krb5_keytab /*keytab*/,
	krb5_ticket **/*ticket*/);

krb5_error_code
krb5_recvauth_match_version (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	krb5_pointer /*p_fd*/,
	krb5_boolean (*/*match_appl_version*/)(const void *, const char*),
	const void */*match_data*/,
	krb5_principal /*server*/,
	int32_t /*flags*/,
	krb5_keytab /*keytab*/,
	krb5_ticket **/*ticket*/);

krb5_error_code
krb5_ret_address (
	krb5_storage */*sp*/,
	krb5_address */*adr*/);

krb5_error_code
krb5_ret_addrs (
	krb5_storage */*sp*/,
	krb5_addresses */*adr*/);

krb5_error_code
krb5_ret_authdata (
	krb5_storage */*sp*/,
	krb5_authdata */*auth*/);

krb5_error_code
krb5_ret_creds (
	krb5_storage */*sp*/,
	krb5_creds */*creds*/);

krb5_error_code
krb5_ret_data (
	krb5_storage */*sp*/,
	krb5_data */*data*/);

krb5_error_code
krb5_ret_int16 (
	krb5_storage */*sp*/,
	int16_t */*value*/);

krb5_error_code
krb5_ret_int32 (
	krb5_storage */*sp*/,
	int32_t */*value*/);

krb5_error_code
krb5_ret_int8 (
	krb5_storage */*sp*/,
	int8_t */*value*/);

krb5_error_code
krb5_ret_keyblock (
	krb5_storage */*sp*/,
	krb5_keyblock */*p*/);

krb5_error_code
krb5_ret_principal (
	krb5_storage */*sp*/,
	krb5_principal */*princ*/);

krb5_error_code
krb5_ret_string (
	krb5_storage */*sp*/,
	char **/*string*/);

krb5_error_code
krb5_ret_stringz (
	krb5_storage */*sp*/,
	char **/*string*/);

krb5_error_code
krb5_ret_times (
	krb5_storage */*sp*/,
	krb5_times */*times*/);

krb5_error_code
krb5_salttype_to_string (
	krb5_context /*context*/,
	krb5_enctype /*etype*/,
	krb5_salttype /*stype*/,
	char **/*string*/);

krb5_error_code
krb5_sendauth (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	krb5_pointer /*p_fd*/,
	const char */*appl_version*/,
	krb5_principal /*client*/,
	krb5_principal /*server*/,
	krb5_flags /*ap_req_options*/,
	krb5_data */*in_data*/,
	krb5_creds */*in_creds*/,
	krb5_ccache /*ccache*/,
	krb5_error **/*ret_error*/,
	krb5_ap_rep_enc_part **/*rep_result*/,
	krb5_creds **/*out_creds*/);

krb5_error_code
krb5_sendto (
	krb5_context /*context*/,
	const krb5_data */*send_data*/,
	krb5_krbhst_handle /*handle*/,
	krb5_data */*receive*/);

krb5_error_code
krb5_sendto_kdc (
	krb5_context /*context*/,
	const krb5_data */*send_data*/,
	const krb5_realm */*realm*/,
	krb5_data */*receive*/);

krb5_error_code
krb5_sendto_kdc2 (
	krb5_context /*context*/,
	const krb5_data */*send_data*/,
	const krb5_realm */*realm*/,
	krb5_data */*receive*/,
	krb5_boolean /*master*/);

krb5_error_code
krb5_set_config_files (
	krb5_context /*context*/,
	char **/*filenames*/);

krb5_error_code
krb5_set_default_in_tkt_etypes (
	krb5_context /*context*/,
	const krb5_enctype */*etypes*/);

krb5_error_code
krb5_set_default_realm (
	krb5_context /*context*/,
	const char */*realm*/);

krb5_error_code
krb5_set_error_string (
	krb5_context /*context*/,
	const char */*fmt*/,
	...)
    __attribute__((format (printf, 2, 3)));

krb5_error_code
krb5_set_extra_addresses (
	krb5_context /*context*/,
	const krb5_addresses */*addresses*/);

krb5_error_code
krb5_set_fcache_version (
	krb5_context /*context*/,
	int /*version*/);

krb5_error_code
krb5_set_ignore_addresses (
	krb5_context /*context*/,
	const krb5_addresses */*addresses*/);

void
krb5_set_use_admin_kdc (
	krb5_context /*context*/,
	krb5_boolean /*flag*/);

krb5_error_code
krb5_set_warn_dest (
	krb5_context /*context*/,
	krb5_log_facility */*fac*/);

krb5_error_code
krb5_sname_to_principal (
	krb5_context /*context*/,
	const char */*hostname*/,
	const char */*sname*/,
	int32_t /*type*/,
	krb5_principal */*ret_princ*/);

krb5_error_code
krb5_sock_to_principal (
	krb5_context /*context*/,
	int /*sock*/,
	const char */*sname*/,
	int32_t /*type*/,
	krb5_principal */*ret_princ*/);

krb5_error_code
krb5_sockaddr2address (
	krb5_context /*context*/,
	const struct sockaddr */*sa*/,
	krb5_address */*addr*/);

krb5_error_code
krb5_sockaddr2port (
	krb5_context /*context*/,
	const struct sockaddr */*sa*/,
	int16_t */*port*/);

krb5_boolean
krb5_sockaddr_uninteresting (const struct sockaddr */*sa*/);

void
krb5_std_usage (
	int /*code*/,
	struct getargs */*args*/,
	int /*num_args*/);

void
krb5_storage_clear_flags (
	krb5_storage */*sp*/,
	krb5_flags /*flags*/);

krb5_storage *
krb5_storage_emem (void);

krb5_error_code
krb5_storage_free (krb5_storage */*sp*/);

krb5_storage *
krb5_storage_from_data (krb5_data */*data*/);

krb5_storage *
krb5_storage_from_fd (int /*fd*/);

krb5_storage *
krb5_storage_from_mem (
	void */*buf*/,
	size_t /*len*/);

krb5_flags
krb5_storage_get_byteorder (
	krb5_storage */*sp*/,
	krb5_flags /*byteorder*/);

krb5_boolean
krb5_storage_is_flags (
	krb5_storage */*sp*/,
	krb5_flags /*flags*/);

krb5_ssize_t
krb5_storage_read (
	krb5_storage */*sp*/,
	void */*buf*/,
	size_t /*len*/);

off_t
krb5_storage_seek (
	krb5_storage */*sp*/,
	off_t /*offset*/,
	int /*whence*/);

void
krb5_storage_set_byteorder (
	krb5_storage */*sp*/,
	krb5_flags /*byteorder*/);

void
krb5_storage_set_eof_code (
	krb5_storage */*sp*/,
	int /*code*/);

void
krb5_storage_set_flags (
	krb5_storage */*sp*/,
	krb5_flags /*flags*/);

krb5_error_code
krb5_storage_to_data (
	krb5_storage */*sp*/,
	krb5_data */*data*/);

krb5_ssize_t
krb5_storage_write (
	krb5_storage */*sp*/,
	const void */*buf*/,
	size_t /*len*/);

krb5_error_code
krb5_store_address (
	krb5_storage */*sp*/,
	krb5_address /*p*/);

krb5_error_code
krb5_store_addrs (
	krb5_storage */*sp*/,
	krb5_addresses /*p*/);

krb5_error_code
krb5_store_authdata (
	krb5_storage */*sp*/,
	krb5_authdata /*auth*/);

krb5_error_code
krb5_store_creds (
	krb5_storage */*sp*/,
	krb5_creds */*creds*/);

krb5_error_code
krb5_store_data (
	krb5_storage */*sp*/,
	krb5_data /*data*/);

krb5_error_code
krb5_store_int16 (
	krb5_storage */*sp*/,
	int16_t /*value*/);

krb5_error_code
krb5_store_int32 (
	krb5_storage */*sp*/,
	int32_t /*value*/);

krb5_error_code
krb5_store_int8 (
	krb5_storage */*sp*/,
	int8_t /*value*/);

krb5_error_code
krb5_store_keyblock (
	krb5_storage */*sp*/,
	krb5_keyblock /*p*/);

krb5_error_code
krb5_store_principal (
	krb5_storage */*sp*/,
	krb5_principal /*p*/);

krb5_error_code
krb5_store_string (
	krb5_storage */*sp*/,
	const char */*s*/);

krb5_error_code
krb5_store_stringz (
	krb5_storage */*sp*/,
	const char */*s*/);

krb5_error_code
krb5_store_times (
	krb5_storage */*sp*/,
	krb5_times /*times*/);

krb5_error_code
krb5_string_to_deltat (
	const char */*string*/,
	krb5_deltat */*deltat*/);

krb5_error_code
krb5_string_to_enctype (
	krb5_context /*context*/,
	const char */*string*/,
	krb5_enctype */*etype*/);

krb5_error_code
krb5_string_to_key (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/,
	const char */*password*/,
	krb5_principal /*principal*/,
	krb5_keyblock */*key*/);

krb5_error_code
krb5_string_to_key_data (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/,
	krb5_data /*password*/,
	krb5_principal /*principal*/,
	krb5_keyblock */*key*/);

krb5_error_code
krb5_string_to_key_data_salt (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/,
	krb5_data /*password*/,
	krb5_salt /*salt*/,
	krb5_keyblock */*key*/);

krb5_error_code
krb5_string_to_key_derived (
	krb5_context /*context*/,
	const void */*str*/,
	size_t /*len*/,
	krb5_enctype /*etype*/,
	krb5_keyblock */*key*/);

krb5_error_code
krb5_string_to_key_salt (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/,
	const char */*password*/,
	krb5_salt /*salt*/,
	krb5_keyblock */*key*/);

krb5_error_code
krb5_string_to_keytype (
	krb5_context /*context*/,
	const char */*string*/,
	krb5_keytype */*keytype*/);

krb5_error_code
krb5_string_to_salttype (
	krb5_context /*context*/,
	krb5_enctype /*etype*/,
	const char */*string*/,
	krb5_salttype */*salttype*/);

krb5_error_code
krb5_timeofday (
	krb5_context /*context*/,
	krb5_timestamp */*timeret*/);

krb5_error_code
krb5_unparse_name (
	krb5_context /*context*/,
	krb5_const_principal /*principal*/,
	char **/*name*/);

krb5_error_code
krb5_unparse_name_fixed (
	krb5_context /*context*/,
	krb5_const_principal /*principal*/,
	char */*name*/,
	size_t /*len*/);

krb5_error_code
krb5_unparse_name_fixed_short (
	krb5_context /*context*/,
	krb5_const_principal /*principal*/,
	char */*name*/,
	size_t /*len*/);

krb5_error_code
krb5_unparse_name_short (
	krb5_context /*context*/,
	krb5_const_principal /*principal*/,
	char **/*name*/);

krb5_error_code
krb5_us_timeofday (
	krb5_context /*context*/,
	int32_t */*sec*/,
	int32_t */*usec*/);

krb5_error_code
krb5_vabort (
	krb5_context /*context*/,
	krb5_error_code /*code*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__ ((noreturn, format (printf, 3, 0)));

krb5_error_code
krb5_vabortx (
	krb5_context /*context*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__ ((noreturn, format (printf, 2, 0)));

krb5_error_code
krb5_verify_ap_req (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	krb5_ap_req */*ap_req*/,
	krb5_const_principal /*server*/,
	krb5_keyblock */*keyblock*/,
	krb5_flags /*flags*/,
	krb5_flags */*ap_req_options*/,
	krb5_ticket **/*ticket*/);

krb5_error_code
krb5_verify_ap_req2 (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	krb5_ap_req */*ap_req*/,
	krb5_const_principal /*server*/,
	krb5_keyblock */*keyblock*/,
	krb5_flags /*flags*/,
	krb5_flags */*ap_req_options*/,
	krb5_ticket **/*ticket*/,
	krb5_key_usage /*usage*/);

krb5_error_code
krb5_verify_authenticator_checksum (
	krb5_context /*context*/,
	krb5_auth_context /*ac*/,
	void */*data*/,
	size_t /*len*/);

krb5_error_code
krb5_verify_checksum (
	krb5_context /*context*/,
	krb5_crypto /*crypto*/,
	krb5_key_usage /*usage*/,
	void */*data*/,
	size_t /*len*/,
	Checksum */*cksum*/);

krb5_error_code
krb5_verify_init_creds (
	krb5_context /*context*/,
	krb5_creds */*creds*/,
	krb5_principal /*ap_req_server*/,
	krb5_keytab /*ap_req_keytab*/,
	krb5_ccache */*ccache*/,
	krb5_verify_init_creds_opt */*options*/);

void
krb5_verify_init_creds_opt_init (krb5_verify_init_creds_opt */*options*/);

void
krb5_verify_init_creds_opt_set_ap_req_nofail (
	krb5_verify_init_creds_opt */*options*/,
	int /*ap_req_nofail*/);

void
krb5_verify_opt_init (krb5_verify_opt */*opt*/);

void
krb5_verify_opt_set_ccache (
	krb5_verify_opt */*opt*/,
	krb5_ccache /*ccache*/);

void
krb5_verify_opt_set_flags (
	krb5_verify_opt */*opt*/,
	unsigned int /*flags*/);

void
krb5_verify_opt_set_keytab (
	krb5_verify_opt */*opt*/,
	krb5_keytab /*keytab*/);

void
krb5_verify_opt_set_secure (
	krb5_verify_opt */*opt*/,
	krb5_boolean /*secure*/);

void
krb5_verify_opt_set_service (
	krb5_verify_opt */*opt*/,
	const char */*service*/);

krb5_error_code
krb5_verify_user (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	krb5_ccache /*ccache*/,
	const char */*password*/,
	krb5_boolean /*secure*/,
	const char */*service*/);

krb5_error_code
krb5_verify_user_lrealm (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	krb5_ccache /*ccache*/,
	const char */*password*/,
	krb5_boolean /*secure*/,
	const char */*service*/);

krb5_error_code
krb5_verify_user_opt (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	const char */*password*/,
	krb5_verify_opt */*opt*/);

krb5_error_code
krb5_verr (
	krb5_context /*context*/,
	int /*eval*/,
	krb5_error_code /*code*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__ ((noreturn, format (printf, 4, 0)));

krb5_error_code
krb5_verrx (
	krb5_context /*context*/,
	int /*eval*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__ ((noreturn, format (printf, 3, 0)));

krb5_error_code
krb5_vlog (
	krb5_context /*context*/,
	krb5_log_facility */*fac*/,
	int /*level*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__((format (printf, 4, 0)));

krb5_error_code
krb5_vlog_msg (
	krb5_context /*context*/,
	krb5_log_facility */*fac*/,
	char **/*reply*/,
	int /*level*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__((format (printf, 5, 0)));

krb5_error_code
krb5_vset_error_string (
	krb5_context /*context*/,
	const char */*fmt*/,
	va_list /*args*/)
    __attribute__ ((format (printf, 2, 0)));

krb5_error_code
krb5_vwarn (
	krb5_context /*context*/,
	krb5_error_code /*code*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__ ((format (printf, 3, 0)));

krb5_error_code
krb5_vwarnx (
	krb5_context /*context*/,
	const char */*fmt*/,
	va_list /*ap*/)
    __attribute__ ((format (printf, 2, 0)));

krb5_error_code
krb5_warn (
	krb5_context /*context*/,
	krb5_error_code /*code*/,
	const char */*fmt*/,
	...)
    __attribute__ ((format (printf, 3, 4)));

krb5_error_code
krb5_warnx (
	krb5_context /*context*/,
	const char */*fmt*/,
	...)
    __attribute__ ((format (printf, 2, 3)));

krb5_error_code
krb5_write_message (
	krb5_context /*context*/,
	krb5_pointer /*p_fd*/,
	krb5_data */*data*/);

krb5_error_code
krb5_write_priv_message (
	krb5_context /*context*/,
	krb5_auth_context /*ac*/,
	krb5_pointer /*p_fd*/,
	krb5_data */*data*/);

krb5_error_code
krb5_write_safe_message (
	krb5_context /*context*/,
	krb5_auth_context /*ac*/,
	krb5_pointer /*p_fd*/,
	krb5_data */*data*/);

krb5_error_code
krb5_xfree (void */*ptr*/);

krb5_error_code
principalname2krb5_principal (
	krb5_principal */*principal*/,
	const PrincipalName /*from*/,
	const Realm /*realm*/);

#endif /* __krb5_protos_h__ */
