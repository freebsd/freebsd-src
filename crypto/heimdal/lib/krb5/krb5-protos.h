/* This is a generated file */
#ifndef __krb5_protos_h__
#define __krb5_protos_h__

#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

krb5_error_code
krb524_convert_creds_kdc __P((
	krb5_context context,
	krb5_creds *in_cred,
	struct credentials *v4creds));

krb5_error_code
krb524_convert_creds_kdc_ccache __P((
	krb5_context context,
	krb5_ccache ccache,
	krb5_creds *in_cred,
	struct credentials *v4creds));

krb5_error_code
krb5_425_conv_principal __P((
	krb5_context context,
	const char *name,
	const char *instance,
	const char *realm,
	krb5_principal *princ));

krb5_error_code
krb5_425_conv_principal_ext __P((
	krb5_context context,
	const char *name,
	const char *instance,
	const char *realm,
	krb5_boolean (*func)(krb5_context, krb5_principal),
	krb5_boolean resolve,
	krb5_principal *princ));

krb5_error_code
krb5_524_conv_principal __P((
	krb5_context context,
	const krb5_principal principal,
	char *name,
	char *instance,
	char *realm));

krb5_error_code
krb5_abort __P((
	krb5_context context,
	krb5_error_code code,
	const char *fmt,
	...))
    __attribute__ ((noreturn, format (printf, 3, 4)));

krb5_error_code
krb5_abortx __P((
	krb5_context context,
	const char *fmt,
	...))
    __attribute__ ((noreturn, format (printf, 2, 3)));

krb5_error_code
krb5_acl_match_file __P((
	krb5_context context,
	const char *file,
	const char *format,
	...));

krb5_error_code
krb5_acl_match_string __P((
	krb5_context context,
	const char *acl_string,
	const char *format,
	...));

krb5_error_code
krb5_add_et_list __P((
	krb5_context context,
	void (*func)(struct et_list **)));

krb5_error_code
krb5_add_extra_addresses __P((
	krb5_context context,
	krb5_addresses *addresses));

krb5_error_code
krb5_add_ignore_addresses __P((
	krb5_context context,
	krb5_addresses *addresses));

krb5_error_code
krb5_addlog_dest __P((
	krb5_context context,
	krb5_log_facility *f,
	const char *orig));

krb5_error_code
krb5_addlog_func __P((
	krb5_context context,
	krb5_log_facility *fac,
	int min,
	int max,
	krb5_log_log_func_t log,
	krb5_log_close_func_t close,
	void *data));

krb5_error_code
krb5_addr2sockaddr __P((
	krb5_context context,
	const krb5_address *addr,
	struct sockaddr *sa,
	int *sa_size,
	int port));

krb5_boolean
krb5_address_compare __P((
	krb5_context context,
	const krb5_address *addr1,
	const krb5_address *addr2));

int
krb5_address_order __P((
	krb5_context context,
	const krb5_address *addr1,
	const krb5_address *addr2));

krb5_boolean
krb5_address_search __P((
	krb5_context context,
	const krb5_address *addr,
	const krb5_addresses *addrlist));

krb5_error_code
krb5_aname_to_localname __P((
	krb5_context context,
	krb5_const_principal aname,
	size_t lnsize,
	char *lname));

krb5_error_code
krb5_anyaddr __P((
	krb5_context context,
	int af,
	struct sockaddr *sa,
	int *sa_size,
	int port));

void
krb5_appdefault_boolean __P((
	krb5_context context,
	const char *appname,
	krb5_const_realm realm,
	const char *option,
	krb5_boolean def_val,
	krb5_boolean *ret_val));

void
krb5_appdefault_string __P((
	krb5_context context,
	const char *appname,
	krb5_const_realm realm,
	const char *option,
	const char *def_val,
	char **ret_val));

void
krb5_appdefault_time __P((
	krb5_context context,
	const char *appname,
	krb5_const_realm realm,
	const char *option,
	time_t def_val,
	time_t *ret_val));

krb5_error_code
krb5_append_addresses __P((
	krb5_context context,
	krb5_addresses *dest,
	const krb5_addresses *source));

krb5_error_code
krb5_auth_con_free __P((
	krb5_context context,
	krb5_auth_context auth_context));

krb5_error_code
krb5_auth_con_genaddrs __P((
	krb5_context context,
	krb5_auth_context auth_context,
	int fd,
	int flags));

krb5_error_code
krb5_auth_con_getaddrs __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_address **local_addr,
	krb5_address **remote_addr));

krb5_error_code
krb5_auth_con_getauthenticator __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_authenticator *authenticator));

krb5_error_code
krb5_auth_con_getcksumtype __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_cksumtype *cksumtype));

krb5_error_code
krb5_auth_con_getflags __P((
	krb5_context context,
	krb5_auth_context auth_context,
	int32_t *flags));

krb5_error_code
krb5_auth_con_getkey __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keyblock **keyblock));

krb5_error_code
krb5_auth_con_getkeytype __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keytype *keytype));

krb5_error_code
krb5_auth_con_getlocalseqnumber __P((
	krb5_context context,
	krb5_auth_context auth_context,
	int32_t *seqnumber));

krb5_error_code
krb5_auth_con_getlocalsubkey __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keyblock **keyblock));

krb5_error_code
krb5_auth_con_getrcache __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_rcache *rcache));

krb5_error_code
krb5_auth_con_getremotesubkey __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keyblock **keyblock));

krb5_error_code
krb5_auth_con_init __P((
	krb5_context context,
	krb5_auth_context *auth_context));

krb5_error_code
krb5_auth_con_setaddrs __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_address *local_addr,
	krb5_address *remote_addr));

krb5_error_code
krb5_auth_con_setaddrs_from_fd __P((
	krb5_context context,
	krb5_auth_context auth_context,
	void *p_fd));

krb5_error_code
krb5_auth_con_setcksumtype __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_cksumtype cksumtype));

krb5_error_code
krb5_auth_con_setflags __P((
	krb5_context context,
	krb5_auth_context auth_context,
	int32_t flags));

krb5_error_code
krb5_auth_con_setkey __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keyblock *keyblock));

krb5_error_code
krb5_auth_con_setkeytype __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keytype keytype));

krb5_error_code
krb5_auth_con_setlocalseqnumber __P((
	krb5_context context,
	krb5_auth_context auth_context,
	int32_t seqnumber));

krb5_error_code
krb5_auth_con_setlocalsubkey __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keyblock *keyblock));

krb5_error_code
krb5_auth_con_setrcache __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_rcache rcache));

krb5_error_code
krb5_auth_con_setremoteseqnumber __P((
	krb5_context context,
	krb5_auth_context auth_context,
	int32_t seqnumber));

krb5_error_code
krb5_auth_con_setremotesubkey __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keyblock *keyblock));

krb5_error_code
krb5_auth_con_setuserkey __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_keyblock *keyblock));

krb5_error_code
krb5_auth_getremoteseqnumber __P((
	krb5_context context,
	krb5_auth_context auth_context,
	int32_t *seqnumber));

krb5_error_code
krb5_build_ap_req __P((
	krb5_context context,
	krb5_enctype enctype,
	krb5_creds *cred,
	krb5_flags ap_options,
	krb5_data authenticator,
	krb5_data *retdata));

krb5_error_code
krb5_build_authenticator __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_enctype enctype,
	krb5_creds *cred,
	Checksum *cksum,
	Authenticator **auth_result,
	krb5_data *result,
	krb5_key_usage usage));

krb5_error_code
krb5_build_principal __P((
	krb5_context context,
	krb5_principal *principal,
	int rlen,
	krb5_const_realm realm,
	...));

krb5_error_code
krb5_build_principal_ext __P((
	krb5_context context,
	krb5_principal *principal,
	int rlen,
	krb5_const_realm realm,
	...));

krb5_error_code
krb5_build_principal_va __P((
	krb5_context context,
	krb5_principal *principal,
	int rlen,
	krb5_const_realm realm,
	va_list ap));

krb5_error_code
krb5_build_principal_va_ext __P((
	krb5_context context,
	krb5_principal *principal,
	int rlen,
	krb5_const_realm realm,
	va_list ap));

krb5_error_code
krb5_cc_close __P((
	krb5_context context,
	krb5_ccache id));

krb5_error_code
krb5_cc_copy_cache __P((
	krb5_context context,
	const krb5_ccache from,
	krb5_ccache to));

krb5_error_code
krb5_cc_default __P((
	krb5_context context,
	krb5_ccache *id));

const char*
krb5_cc_default_name __P((krb5_context context));

krb5_error_code
krb5_cc_destroy __P((
	krb5_context context,
	krb5_ccache id));

krb5_error_code
krb5_cc_end_seq_get __P((
	krb5_context context,
	const krb5_ccache id,
	krb5_cc_cursor *cursor));

krb5_error_code
krb5_cc_gen_new __P((
	krb5_context context,
	const krb5_cc_ops *ops,
	krb5_ccache *id));

const char*
krb5_cc_get_name __P((
	krb5_context context,
	krb5_ccache id));

krb5_error_code
krb5_cc_get_principal __P((
	krb5_context context,
	krb5_ccache id,
	krb5_principal *principal));

const char*
krb5_cc_get_type __P((
	krb5_context context,
	krb5_ccache id));

krb5_error_code
krb5_cc_get_version __P((
	krb5_context context,
	const krb5_ccache id));

krb5_error_code
krb5_cc_initialize __P((
	krb5_context context,
	krb5_ccache id,
	krb5_principal primary_principal));

krb5_error_code
krb5_cc_next_cred __P((
	krb5_context context,
	const krb5_ccache id,
	krb5_cc_cursor *cursor,
	krb5_creds *creds));

krb5_error_code
krb5_cc_register __P((
	krb5_context context,
	const krb5_cc_ops *ops,
	krb5_boolean override));

krb5_error_code
krb5_cc_remove_cred __P((
	krb5_context context,
	krb5_ccache id,
	krb5_flags which,
	krb5_creds *cred));

krb5_error_code
krb5_cc_resolve __P((
	krb5_context context,
	const char *name,
	krb5_ccache *id));

krb5_error_code
krb5_cc_retrieve_cred __P((
	krb5_context context,
	krb5_ccache id,
	krb5_flags whichfields,
	const krb5_creds *mcreds,
	krb5_creds *creds));

krb5_error_code
krb5_cc_set_flags __P((
	krb5_context context,
	krb5_ccache id,
	krb5_flags flags));

krb5_error_code
krb5_cc_start_seq_get __P((
	krb5_context context,
	const krb5_ccache id,
	krb5_cc_cursor *cursor));

krb5_error_code
krb5_cc_store_cred __P((
	krb5_context context,
	krb5_ccache id,
	krb5_creds *creds));

krb5_error_code
krb5_change_password __P((
	krb5_context context,
	krb5_creds *creds,
	char *newpw,
	int *result_code,
	krb5_data *result_code_string,
	krb5_data *result_string));

krb5_error_code
krb5_check_transited_realms __P((
	krb5_context context,
	const char *const *realms,
	int num_realms,
	int *bad_realm));

krb5_boolean
krb5_checksum_is_collision_proof __P((
	krb5_context context,
	krb5_cksumtype type));

krb5_boolean
krb5_checksum_is_keyed __P((
	krb5_context context,
	krb5_cksumtype type));

krb5_error_code
krb5_checksumsize __P((
	krb5_context context,
	krb5_cksumtype type,
	size_t *size));

void
krb5_clear_error_string __P((krb5_context context));

krb5_error_code
krb5_closelog __P((
	krb5_context context,
	krb5_log_facility *fac));

krb5_boolean
krb5_compare_creds __P((
	krb5_context context,
	krb5_flags whichfields,
	const krb5_creds *mcreds,
	const krb5_creds *creds));

krb5_error_code
krb5_config_file_free __P((
	krb5_context context,
	krb5_config_section *s));

void
krb5_config_free_strings __P((char **strings));

const void *
krb5_config_get __P((
	krb5_context context,
	krb5_config_section *c,
	int type,
	...));

krb5_boolean
krb5_config_get_bool __P((
	krb5_context context,
	krb5_config_section *c,
	...));

krb5_boolean
krb5_config_get_bool_default __P((
	krb5_context context,
	krb5_config_section *c,
	krb5_boolean def_value,
	...));

int
krb5_config_get_int __P((
	krb5_context context,
	krb5_config_section *c,
	...));

int
krb5_config_get_int_default __P((
	krb5_context context,
	krb5_config_section *c,
	int def_value,
	...));

const krb5_config_binding *
krb5_config_get_list __P((
	krb5_context context,
	krb5_config_section *c,
	...));

const void *
krb5_config_get_next __P((
	krb5_context context,
	krb5_config_section *c,
	krb5_config_binding **pointer,
	int type,
	...));

const char *
krb5_config_get_string __P((
	krb5_context context,
	krb5_config_section *c,
	...));

const char *
krb5_config_get_string_default __P((
	krb5_context context,
	krb5_config_section *c,
	const char *def_value,
	...));

char**
krb5_config_get_strings __P((
	krb5_context context,
	krb5_config_section *c,
	...));

int
krb5_config_get_time __P((
	krb5_context context,
	krb5_config_section *c,
	...));

int
krb5_config_get_time_default __P((
	krb5_context context,
	krb5_config_section *c,
	int def_value,
	...));

krb5_error_code
krb5_config_parse_file __P((
	krb5_context context,
	const char *fname,
	krb5_config_section **res));

const void *
krb5_config_vget __P((
	krb5_context context,
	krb5_config_section *c,
	int type,
	va_list args));

krb5_boolean
krb5_config_vget_bool __P((
	krb5_context context,
	krb5_config_section *c,
	va_list args));

krb5_boolean
krb5_config_vget_bool_default __P((
	krb5_context context,
	krb5_config_section *c,
	krb5_boolean def_value,
	va_list args));

int
krb5_config_vget_int __P((
	krb5_context context,
	krb5_config_section *c,
	va_list args));

int
krb5_config_vget_int_default __P((
	krb5_context context,
	krb5_config_section *c,
	int def_value,
	va_list args));

const krb5_config_binding *
krb5_config_vget_list __P((
	krb5_context context,
	krb5_config_section *c,
	va_list args));

const void *
krb5_config_vget_next __P((
	krb5_context context,
	krb5_config_section *c,
	krb5_config_binding **pointer,
	int type,
	va_list args));

const char *
krb5_config_vget_string __P((
	krb5_context context,
	krb5_config_section *c,
	va_list args));

const char *
krb5_config_vget_string_default __P((
	krb5_context context,
	krb5_config_section *c,
	const char *def_value,
	va_list args));

char **
krb5_config_vget_strings __P((
	krb5_context context,
	krb5_config_section *c,
	va_list args));

int
krb5_config_vget_time __P((
	krb5_context context,
	krb5_config_section *c,
	va_list args));

int
krb5_config_vget_time_default __P((
	krb5_context context,
	krb5_config_section *c,
	int def_value,
	va_list args));

krb5_error_code
krb5_copy_address __P((
	krb5_context context,
	const krb5_address *inaddr,
	krb5_address *outaddr));

krb5_error_code
krb5_copy_addresses __P((
	krb5_context context,
	const krb5_addresses *inaddr,
	krb5_addresses *outaddr));

krb5_error_code
krb5_copy_creds __P((
	krb5_context context,
	const krb5_creds *incred,
	krb5_creds **outcred));

krb5_error_code
krb5_copy_creds_contents __P((
	krb5_context context,
	const krb5_creds *incred,
	krb5_creds *c));

krb5_error_code
krb5_copy_data __P((
	krb5_context context,
	const krb5_data *indata,
	krb5_data **outdata));

krb5_error_code
krb5_copy_host_realm __P((
	krb5_context context,
	const krb5_realm *from,
	krb5_realm **to));

krb5_error_code
krb5_copy_keyblock __P((
	krb5_context context,
	const krb5_keyblock *inblock,
	krb5_keyblock **to));

krb5_error_code
krb5_copy_keyblock_contents __P((
	krb5_context context,
	const krb5_keyblock *inblock,
	krb5_keyblock *to));

krb5_error_code
krb5_copy_principal __P((
	krb5_context context,
	krb5_const_principal inprinc,
	krb5_principal *outprinc));

krb5_error_code
krb5_copy_ticket __P((
	krb5_context context,
	const krb5_ticket *from,
	krb5_ticket **to));

krb5_error_code
krb5_create_checksum __P((
	krb5_context context,
	krb5_crypto crypto,
	krb5_key_usage usage,
	int type,
	void *data,
	size_t len,
	Checksum *result));

krb5_error_code
krb5_crypto_destroy __P((
	krb5_context context,
	krb5_crypto crypto));

krb5_error_code
krb5_crypto_init __P((
	krb5_context context,
	const krb5_keyblock *key,
	krb5_enctype etype,
	krb5_crypto *crypto));

krb5_error_code
krb5_data_alloc __P((
	krb5_data *p,
	int len));

krb5_error_code
krb5_data_copy __P((
	krb5_data *p,
	const void *data,
	size_t len));

void
krb5_data_free __P((krb5_data *p));

krb5_error_code
krb5_data_realloc __P((
	krb5_data *p,
	int len));

void
krb5_data_zero __P((krb5_data *p));

krb5_error_code
krb5_decode_Authenticator __P((
	krb5_context context,
	const void *data,
	size_t length,
	Authenticator *t,
	size_t *len));

krb5_error_code
krb5_decode_ETYPE_INFO __P((
	krb5_context context,
	const void *data,
	size_t length,
	ETYPE_INFO *t,
	size_t *len));

krb5_error_code
krb5_decode_EncAPRepPart __P((
	krb5_context context,
	const void *data,
	size_t length,
	EncAPRepPart *t,
	size_t *len));

krb5_error_code
krb5_decode_EncASRepPart __P((
	krb5_context context,
	const void *data,
	size_t length,
	EncASRepPart *t,
	size_t *len));

krb5_error_code
krb5_decode_EncKrbCredPart __P((
	krb5_context context,
	const void *data,
	size_t length,
	EncKrbCredPart *t,
	size_t *len));

krb5_error_code
krb5_decode_EncTGSRepPart __P((
	krb5_context context,
	const void *data,
	size_t length,
	EncTGSRepPart *t,
	size_t *len));

krb5_error_code
krb5_decode_EncTicketPart __P((
	krb5_context context,
	const void *data,
	size_t length,
	EncTicketPart *t,
	size_t *len));

krb5_error_code
krb5_decode_ap_req __P((
	krb5_context context,
	const krb5_data *inbuf,
	krb5_ap_req *ap_req));

krb5_error_code
krb5_decrypt __P((
	krb5_context context,
	krb5_crypto crypto,
	unsigned usage,
	void *data,
	size_t len,
	krb5_data *result));

krb5_error_code
krb5_decrypt_EncryptedData __P((
	krb5_context context,
	krb5_crypto crypto,
	unsigned usage,
	const EncryptedData *e,
	krb5_data *result));

krb5_error_code
krb5_decrypt_ivec __P((
	krb5_context context,
	krb5_crypto crypto,
	unsigned usage,
	void *data,
	size_t len,
	krb5_data *result,
	void *ivec));

krb5_error_code
krb5_decrypt_ticket __P((
	krb5_context context,
	Ticket *ticket,
	krb5_keyblock *key,
	EncTicketPart *out,
	krb5_flags flags));

krb5_error_code
krb5_derive_key __P((
	krb5_context context,
	const krb5_keyblock *key,
	krb5_enctype etype,
	const void *constant,
	size_t constant_len,
	krb5_keyblock **derived_key));

krb5_error_code
krb5_domain_x500_decode __P((
	krb5_context context,
	krb5_data tr,
	char ***realms,
	int *num_realms,
	const char *client_realm,
	const char *server_realm));

krb5_error_code
krb5_domain_x500_encode __P((
	char **realms,
	int num_realms,
	krb5_data *encoding));

krb5_error_code
krb5_eai_to_heim_errno __P((
	int eai_errno,
	int system_error));

krb5_error_code
krb5_encode_Authenticator __P((
	krb5_context context,
	void *data,
	size_t length,
	Authenticator *t,
	size_t *len));

krb5_error_code
krb5_encode_ETYPE_INFO __P((
	krb5_context context,
	void *data,
	size_t length,
	ETYPE_INFO *t,
	size_t *len));

krb5_error_code
krb5_encode_EncAPRepPart __P((
	krb5_context context,
	void *data,
	size_t length,
	EncAPRepPart *t,
	size_t *len));

krb5_error_code
krb5_encode_EncASRepPart __P((
	krb5_context context,
	void *data,
	size_t length,
	EncASRepPart *t,
	size_t *len));

krb5_error_code
krb5_encode_EncKrbCredPart __P((
	krb5_context context,
	void *data,
	size_t length,
	EncKrbCredPart *t,
	size_t *len));

krb5_error_code
krb5_encode_EncTGSRepPart __P((
	krb5_context context,
	void *data,
	size_t length,
	EncTGSRepPart *t,
	size_t *len));

krb5_error_code
krb5_encode_EncTicketPart __P((
	krb5_context context,
	void *data,
	size_t length,
	EncTicketPart *t,
	size_t *len));

krb5_error_code
krb5_encrypt __P((
	krb5_context context,
	krb5_crypto crypto,
	unsigned usage,
	void *data,
	size_t len,
	krb5_data *result));

krb5_error_code
krb5_encrypt_EncryptedData __P((
	krb5_context context,
	krb5_crypto crypto,
	unsigned usage,
	void *data,
	size_t len,
	int kvno,
	EncryptedData *result));

krb5_error_code
krb5_encrypt_ivec __P((
	krb5_context context,
	krb5_crypto crypto,
	unsigned usage,
	void *data,
	size_t len,
	krb5_data *result,
	void *ivec));

krb5_error_code
krb5_enctype_to_keytype __P((
	krb5_context context,
	krb5_enctype etype,
	krb5_keytype *keytype));

krb5_error_code
krb5_enctype_to_string __P((
	krb5_context context,
	krb5_enctype etype,
	char **string));

krb5_error_code
krb5_enctype_valid __P((
	krb5_context context,
	krb5_enctype etype));

krb5_boolean
krb5_enctypes_compatible_keys __P((
	krb5_context context,
	krb5_enctype etype1,
	krb5_enctype etype2));

krb5_error_code
krb5_err __P((
	krb5_context context,
	int eval,
	krb5_error_code code,
	const char *fmt,
	...))
    __attribute__ ((noreturn, format (printf, 4, 5)));

krb5_error_code
krb5_error_from_rd_error __P((
	krb5_context context,
	const krb5_error *error,
	const krb5_creds *creds));

krb5_error_code
krb5_errx __P((
	krb5_context context,
	int eval,
	const char *fmt,
	...))
    __attribute__ ((noreturn, format (printf, 3, 4)));

krb5_error_code
krb5_expand_hostname __P((
	krb5_context context,
	const char *orig_hostname,
	char **new_hostname));

krb5_error_code
krb5_expand_hostname_realms __P((
	krb5_context context,
	const char *orig_hostname,
	char **new_hostname,
	char ***realms));

PA_DATA *
krb5_find_padata __P((
	PA_DATA *val,
	unsigned len,
	int type,
	int *index));

krb5_error_code
krb5_format_time __P((
	krb5_context context,
	time_t t,
	char *s,
	size_t len,
	krb5_boolean include_time));

krb5_error_code
krb5_free_address __P((
	krb5_context context,
	krb5_address *address));

krb5_error_code
krb5_free_addresses __P((
	krb5_context context,
	krb5_addresses *addresses));

void
krb5_free_ap_rep_enc_part __P((
	krb5_context context,
	krb5_ap_rep_enc_part *val));

void
krb5_free_authenticator __P((
	krb5_context context,
	krb5_authenticator *authenticator));

void
krb5_free_context __P((krb5_context context));

krb5_error_code
krb5_free_cred_contents __P((
	krb5_context context,
	krb5_creds *c));

krb5_error_code
krb5_free_creds __P((
	krb5_context context,
	krb5_creds *c));

krb5_error_code
krb5_free_creds_contents __P((
	krb5_context context,
	krb5_creds *c));

void
krb5_free_data __P((
	krb5_context context,
	krb5_data *p));

void
krb5_free_error __P((
	krb5_context context,
	krb5_error *error));

void
krb5_free_error_contents __P((
	krb5_context context,
	krb5_error *error));

void
krb5_free_error_string __P((
	krb5_context context,
	char *str));

krb5_error_code
krb5_free_host_realm __P((
	krb5_context context,
	krb5_realm *realmlist));

krb5_error_code
krb5_free_kdc_rep __P((
	krb5_context context,
	krb5_kdc_rep *rep));

void
krb5_free_keyblock __P((
	krb5_context context,
	krb5_keyblock *keyblock));

void
krb5_free_keyblock_contents __P((
	krb5_context context,
	krb5_keyblock *keyblock));

krb5_error_code
krb5_free_krbhst __P((
	krb5_context context,
	char **hostlist));

void
krb5_free_principal __P((
	krb5_context context,
	krb5_principal p));

krb5_error_code
krb5_free_salt __P((
	krb5_context context,
	krb5_salt salt));

krb5_error_code
krb5_free_ticket __P((
	krb5_context context,
	krb5_ticket *ticket));

krb5_error_code
krb5_fwd_tgt_creds __P((
	krb5_context context,
	krb5_auth_context auth_context,
	const char *hostname,
	krb5_principal client,
	krb5_principal server,
	krb5_ccache ccache,
	int forwardable,
	krb5_data *out_data));

void
krb5_generate_random_block __P((
	void *buf,
	size_t len));

krb5_error_code
krb5_generate_random_keyblock __P((
	krb5_context context,
	krb5_enctype type,
	krb5_keyblock *key));

krb5_error_code
krb5_generate_seq_number __P((
	krb5_context context,
	const krb5_keyblock *key,
	u_int32_t *seqno));

krb5_error_code
krb5_generate_subkey __P((
	krb5_context context,
	const krb5_keyblock *key,
	krb5_keyblock **subkey));

krb5_error_code
krb5_get_all_client_addrs __P((
	krb5_context context,
	krb5_addresses *res));

krb5_error_code
krb5_get_all_server_addrs __P((
	krb5_context context,
	krb5_addresses *res));

krb5_error_code
krb5_get_cred_from_kdc __P((
	krb5_context context,
	krb5_ccache ccache,
	krb5_creds *in_creds,
	krb5_creds **out_creds,
	krb5_creds ***ret_tgts));

krb5_error_code
krb5_get_cred_from_kdc_opt __P((
	krb5_context context,
	krb5_ccache ccache,
	krb5_creds *in_creds,
	krb5_creds **out_creds,
	krb5_creds ***ret_tgts,
	krb5_flags flags));

krb5_error_code
krb5_get_credentials __P((
	krb5_context context,
	krb5_flags options,
	krb5_ccache ccache,
	krb5_creds *in_creds,
	krb5_creds **out_creds));

krb5_error_code
krb5_get_credentials_with_flags __P((
	krb5_context context,
	krb5_flags options,
	krb5_kdc_flags flags,
	krb5_ccache ccache,
	krb5_creds *in_creds,
	krb5_creds **out_creds));

krb5_error_code
krb5_get_default_in_tkt_etypes __P((
	krb5_context context,
	krb5_enctype **etypes));

krb5_error_code
krb5_get_default_principal __P((
	krb5_context context,
	krb5_principal *princ));

krb5_error_code
krb5_get_default_realm __P((
	krb5_context context,
	krb5_realm *realm));

krb5_error_code
krb5_get_default_realms __P((
	krb5_context context,
	krb5_realm **realms));

const char *
krb5_get_err_text __P((
	krb5_context context,
	krb5_error_code code));

char*
krb5_get_error_string __P((krb5_context context));

krb5_error_code
krb5_get_extra_addresses __P((
	krb5_context context,
	krb5_addresses *addresses));

krb5_error_code
krb5_get_fcache_version __P((
	krb5_context context,
	int *version));

krb5_error_code
krb5_get_forwarded_creds __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_ccache ccache,
	krb5_flags flags,
	const char *hostname,
	krb5_creds *in_creds,
	krb5_data *out_data));

krb5_error_code
krb5_get_host_realm __P((
	krb5_context context,
	const char *host,
	krb5_realm **realms));

krb5_error_code
krb5_get_host_realm_int __P((
	krb5_context context,
	const char *host,
	krb5_boolean use_dns,
	krb5_realm **realms));

krb5_error_code
krb5_get_ignore_addresses __P((
	krb5_context context,
	krb5_addresses *addresses));

krb5_error_code
krb5_get_in_cred __P((
	krb5_context context,
	krb5_flags options,
	const krb5_addresses *addrs,
	const krb5_enctype *etypes,
	const krb5_preauthtype *ptypes,
	const krb5_preauthdata *preauth,
	krb5_key_proc key_proc,
	krb5_const_pointer keyseed,
	krb5_decrypt_proc decrypt_proc,
	krb5_const_pointer decryptarg,
	krb5_creds *creds,
	krb5_kdc_rep *ret_as_reply));

krb5_error_code
krb5_get_in_tkt __P((
	krb5_context context,
	krb5_flags options,
	const krb5_addresses *addrs,
	const krb5_enctype *etypes,
	const krb5_preauthtype *ptypes,
	krb5_key_proc key_proc,
	krb5_const_pointer keyseed,
	krb5_decrypt_proc decrypt_proc,
	krb5_const_pointer decryptarg,
	krb5_creds *creds,
	krb5_ccache ccache,
	krb5_kdc_rep *ret_as_reply));

krb5_error_code
krb5_get_in_tkt_with_keytab __P((
	krb5_context context,
	krb5_flags options,
	krb5_addresses *addrs,
	const krb5_enctype *etypes,
	const krb5_preauthtype *pre_auth_types,
	krb5_keytab keytab,
	krb5_ccache ccache,
	krb5_creds *creds,
	krb5_kdc_rep *ret_as_reply));

krb5_error_code
krb5_get_in_tkt_with_password __P((
	krb5_context context,
	krb5_flags options,
	krb5_addresses *addrs,
	const krb5_enctype *etypes,
	const krb5_preauthtype *pre_auth_types,
	const char *password,
	krb5_ccache ccache,
	krb5_creds *creds,
	krb5_kdc_rep *ret_as_reply));

krb5_error_code
krb5_get_in_tkt_with_skey __P((
	krb5_context context,
	krb5_flags options,
	krb5_addresses *addrs,
	const krb5_enctype *etypes,
	const krb5_preauthtype *pre_auth_types,
	const krb5_keyblock *key,
	krb5_ccache ccache,
	krb5_creds *creds,
	krb5_kdc_rep *ret_as_reply));

krb5_error_code
krb5_get_init_creds_keytab __P((
	krb5_context context,
	krb5_creds *creds,
	krb5_principal client,
	krb5_keytab keytab,
	krb5_deltat start_time,
	const char *in_tkt_service,
	krb5_get_init_creds_opt *options));

void
krb5_get_init_creds_opt_init __P((krb5_get_init_creds_opt *opt));

void
krb5_get_init_creds_opt_set_address_list __P((
	krb5_get_init_creds_opt *opt,
	krb5_addresses *addresses));

void
krb5_get_init_creds_opt_set_anonymous __P((
	krb5_get_init_creds_opt *opt,
	int anonymous));

void
krb5_get_init_creds_opt_set_default_flags __P((
	krb5_context context,
	const char *appname,
	krb5_const_realm realm,
	krb5_get_init_creds_opt *opt));

void
krb5_get_init_creds_opt_set_etype_list __P((
	krb5_get_init_creds_opt *opt,
	krb5_enctype *etype_list,
	int etype_list_length));

void
krb5_get_init_creds_opt_set_forwardable __P((
	krb5_get_init_creds_opt *opt,
	int forwardable));

void
krb5_get_init_creds_opt_set_preauth_list __P((
	krb5_get_init_creds_opt *opt,
	krb5_preauthtype *preauth_list,
	int preauth_list_length));

void
krb5_get_init_creds_opt_set_proxiable __P((
	krb5_get_init_creds_opt *opt,
	int proxiable));

void
krb5_get_init_creds_opt_set_renew_life __P((
	krb5_get_init_creds_opt *opt,
	krb5_deltat renew_life));

void
krb5_get_init_creds_opt_set_salt __P((
	krb5_get_init_creds_opt *opt,
	krb5_data *salt));

void
krb5_get_init_creds_opt_set_tkt_life __P((
	krb5_get_init_creds_opt *opt,
	krb5_deltat tkt_life));

krb5_error_code
krb5_get_init_creds_password __P((
	krb5_context context,
	krb5_creds *creds,
	krb5_principal client,
	const char *password,
	krb5_prompter_fct prompter,
	void *data,
	krb5_deltat start_time,
	const char *in_tkt_service,
	krb5_get_init_creds_opt *options));

krb5_error_code
krb5_get_kdc_cred __P((
	krb5_context context,
	krb5_ccache id,
	krb5_kdc_flags flags,
	krb5_addresses *addresses,
	Ticket *second_ticket,
	krb5_creds *in_creds,
	krb5_creds **out_creds ));

krb5_error_code
krb5_get_krb524hst __P((
	krb5_context context,
	const krb5_realm *realm,
	char ***hostlist));

krb5_error_code
krb5_get_krb_admin_hst __P((
	krb5_context context,
	const krb5_realm *realm,
	char ***hostlist));

krb5_error_code
krb5_get_krb_changepw_hst __P((
	krb5_context context,
	const krb5_realm *realm,
	char ***hostlist));

krb5_error_code
krb5_get_krbhst __P((
	krb5_context context,
	const krb5_realm *realm,
	char ***hostlist));

krb5_error_code
krb5_get_pw_salt __P((
	krb5_context context,
	krb5_const_principal principal,
	krb5_salt *salt));

krb5_error_code
krb5_get_server_rcache __P((
	krb5_context context,
	const krb5_data *piece,
	krb5_rcache *id));

krb5_boolean
krb5_get_use_admin_kdc __P((krb5_context context));

size_t
krb5_get_wrapped_length __P((
	krb5_context context,
	krb5_crypto crypto,
	size_t data_len));

int
krb5_getportbyname __P((
	krb5_context context,
	const char *service,
	const char *proto,
	int default_port));

krb5_error_code
krb5_h_addr2addr __P((
	krb5_context context,
	int af,
	const char *haddr,
	krb5_address *addr));

krb5_error_code
krb5_h_addr2sockaddr __P((
	krb5_context context,
	int af,
	const char *addr,
	struct sockaddr *sa,
	int *sa_size,
	int port));

krb5_error_code
krb5_h_errno_to_heim_errno __P((int eai_errno));

krb5_boolean
krb5_have_error_string __P((krb5_context context));

krb5_error_code
krb5_init_context __P((krb5_context *context));

void
krb5_init_ets __P((krb5_context context));

krb5_error_code
krb5_init_etype __P((
	krb5_context context,
	unsigned *len,
	krb5_enctype **val,
	const krb5_enctype *etypes));

krb5_error_code
krb5_initlog __P((
	krb5_context context,
	const char *program,
	krb5_log_facility **fac));

krb5_error_code
krb5_keyblock_key_proc __P((
	krb5_context context,
	krb5_keytype type,
	krb5_data *salt,
	krb5_const_pointer keyseed,
	krb5_keyblock **key));

krb5_error_code
krb5_keytab_key_proc __P((
	krb5_context context,
	krb5_enctype enctype,
	krb5_salt salt,
	krb5_const_pointer keyseed,
	krb5_keyblock **key));

krb5_error_code
krb5_keytype_to_enctypes __P((
	krb5_context context,
	krb5_keytype keytype,
	unsigned *len,
	krb5_enctype **val));

krb5_error_code
krb5_keytype_to_enctypes_default __P((
	krb5_context context,
	krb5_keytype keytype,
	unsigned *len,
	krb5_enctype **val));

krb5_error_code
krb5_keytype_to_string __P((
	krb5_context context,
	krb5_keytype keytype,
	char **string));

krb5_error_code
krb5_krbhst_format_string __P((
	krb5_context context,
	const krb5_krbhst_info *host,
	char *hostname,
	size_t hostlen));

void
krb5_krbhst_free __P((
	krb5_context context,
	krb5_krbhst_handle handle));

krb5_error_code
krb5_krbhst_get_addrinfo __P((
	krb5_context context,
	krb5_krbhst_info *host,
	struct addrinfo **ai));

krb5_error_code
krb5_krbhst_init __P((
	krb5_context context,
	const char *realm,
	unsigned int type,
	krb5_krbhst_handle *handle));

krb5_error_code
krb5_krbhst_next __P((
	krb5_context context,
	krb5_krbhst_handle handle,
	krb5_krbhst_info **host));

krb5_error_code
krb5_krbhst_next_as_string __P((
	krb5_context context,
	krb5_krbhst_handle handle,
	char *hostname,
	size_t hostlen));

void
krb5_krbhst_reset __P((
	krb5_context context,
	krb5_krbhst_handle handle));

krb5_error_code
krb5_kt_add_entry __P((
	krb5_context context,
	krb5_keytab id,
	krb5_keytab_entry *entry));

krb5_error_code
krb5_kt_close __P((
	krb5_context context,
	krb5_keytab id));

krb5_boolean
krb5_kt_compare __P((
	krb5_context context,
	krb5_keytab_entry *entry,
	krb5_const_principal principal,
	krb5_kvno vno,
	krb5_enctype enctype));

krb5_error_code
krb5_kt_copy_entry_contents __P((
	krb5_context context,
	const krb5_keytab_entry *in,
	krb5_keytab_entry *out));

krb5_error_code
krb5_kt_default __P((
	krb5_context context,
	krb5_keytab *id));

krb5_error_code
krb5_kt_default_modify_name __P((
	krb5_context context,
	char *name,
	size_t namesize));

krb5_error_code
krb5_kt_default_name __P((
	krb5_context context,
	char *name,
	size_t namesize));

krb5_error_code
krb5_kt_end_seq_get __P((
	krb5_context context,
	krb5_keytab id,
	krb5_kt_cursor *cursor));

krb5_error_code
krb5_kt_free_entry __P((
	krb5_context context,
	krb5_keytab_entry *entry));

krb5_error_code
krb5_kt_get_entry __P((
	krb5_context context,
	krb5_keytab id,
	krb5_const_principal principal,
	krb5_kvno kvno,
	krb5_enctype enctype,
	krb5_keytab_entry *entry));

krb5_error_code
krb5_kt_get_name __P((
	krb5_context context,
	krb5_keytab keytab,
	char *name,
	size_t namesize));

krb5_error_code
krb5_kt_next_entry __P((
	krb5_context context,
	krb5_keytab id,
	krb5_keytab_entry *entry,
	krb5_kt_cursor *cursor));

krb5_error_code
krb5_kt_read_service_key __P((
	krb5_context context,
	krb5_pointer keyprocarg,
	krb5_principal principal,
	krb5_kvno vno,
	krb5_enctype enctype,
	krb5_keyblock **key));

krb5_error_code
krb5_kt_register __P((
	krb5_context context,
	const krb5_kt_ops *ops));

krb5_error_code
krb5_kt_remove_entry __P((
	krb5_context context,
	krb5_keytab id,
	krb5_keytab_entry *entry));

krb5_error_code
krb5_kt_resolve __P((
	krb5_context context,
	const char *name,
	krb5_keytab *id));

krb5_error_code
krb5_kt_start_seq_get __P((
	krb5_context context,
	krb5_keytab id,
	krb5_kt_cursor *cursor));

krb5_boolean
krb5_kuserok __P((
	krb5_context context,
	krb5_principal principal,
	const char *luser));

krb5_error_code
krb5_log __P((
	krb5_context context,
	krb5_log_facility *fac,
	int level,
	const char *fmt,
	...))
    __attribute__((format (printf, 4, 5)));

krb5_error_code
krb5_log_msg __P((
	krb5_context context,
	krb5_log_facility *fac,
	int level,
	char **reply,
	const char *fmt,
	...))
    __attribute__((format (printf, 5, 6)));

krb5_error_code
krb5_make_addrport __P((
	krb5_context context,
	krb5_address **res,
	const krb5_address *addr,
	int16_t port));

krb5_error_code
krb5_make_principal __P((
	krb5_context context,
	krb5_principal *principal,
	krb5_const_realm realm,
	...));

size_t
krb5_max_sockaddr_size __P((void));

krb5_error_code
krb5_mk_error __P((
	krb5_context context,
	krb5_error_code error_code,
	const char *e_text,
	const krb5_data *e_data,
	const krb5_principal client,
	const krb5_principal server,
	time_t *ctime,
	int *cusec,
	krb5_data *reply));

krb5_error_code
krb5_mk_priv __P((
	krb5_context context,
	krb5_auth_context auth_context,
	const krb5_data *userdata,
	krb5_data *outbuf,
	void *outdata));

krb5_error_code
krb5_mk_rep __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_data *outbuf));

krb5_error_code
krb5_mk_req __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	const krb5_flags ap_req_options,
	const char *service,
	const char *hostname,
	krb5_data *in_data,
	krb5_ccache ccache,
	krb5_data *outbuf));

krb5_error_code
krb5_mk_req_exact __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	const krb5_flags ap_req_options,
	const krb5_principal server,
	krb5_data *in_data,
	krb5_ccache ccache,
	krb5_data *outbuf));

krb5_error_code
krb5_mk_req_extended __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	const krb5_flags ap_req_options,
	krb5_data *in_data,
	krb5_creds *in_creds,
	krb5_data *outbuf));

krb5_error_code
krb5_mk_req_internal __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	const krb5_flags ap_req_options,
	krb5_data *in_data,
	krb5_creds *in_creds,
	krb5_data *outbuf,
	krb5_key_usage checksum_usage,
	krb5_key_usage encrypt_usage));

krb5_error_code
krb5_mk_safe __P((
	krb5_context context,
	krb5_auth_context auth_context,
	const krb5_data *userdata,
	krb5_data *outbuf,
	void *outdata));

ssize_t
krb5_net_read __P((
	krb5_context context,
	void *p_fd,
	void *buf,
	size_t len));

ssize_t
krb5_net_write __P((
	krb5_context context,
	void *p_fd,
	const void *buf,
	size_t len));

krb5_error_code
krb5_openlog __P((
	krb5_context context,
	const char *program,
	krb5_log_facility **fac));

krb5_error_code
krb5_parse_address __P((
	krb5_context context,
	const char *string,
	krb5_addresses *addresses));

krb5_error_code
krb5_parse_name __P((
	krb5_context context,
	const char *name,
	krb5_principal *principal));

const char *
krb5_passwd_result_to_string __P((
	krb5_context context,
	int result));

krb5_error_code
krb5_password_key_proc __P((
	krb5_context context,
	krb5_enctype type,
	krb5_salt salt,
	krb5_const_pointer keyseed,
	krb5_keyblock **key));

krb5_realm*
krb5_princ_realm __P((
	krb5_context context,
	krb5_principal principal));

void
krb5_princ_set_realm __P((
	krb5_context context,
	krb5_principal principal,
	krb5_realm *realm));

krb5_error_code
krb5_principal2principalname __P((
	PrincipalName *p,
	const krb5_principal from));

krb5_boolean
krb5_principal_compare __P((
	krb5_context context,
	krb5_const_principal princ1,
	krb5_const_principal princ2));

krb5_boolean
krb5_principal_compare_any_realm __P((
	krb5_context context,
	krb5_const_principal princ1,
	krb5_const_principal princ2));

const char *
krb5_principal_get_comp_string __P((
	krb5_context context,
	krb5_principal principal,
	unsigned int component));

const char *
krb5_principal_get_realm __P((
	krb5_context context,
	krb5_principal principal));

int
krb5_principal_get_type __P((
	krb5_context context,
	krb5_principal principal));

krb5_boolean
krb5_principal_match __P((
	krb5_context context,
	krb5_const_principal princ,
	krb5_const_principal pattern));

krb5_error_code
krb5_print_address __P((
	const krb5_address *addr,
	char *str,
	size_t len,
	size_t *ret_len));

int
krb5_program_setup __P((
	krb5_context *context,
	int argc,
	char **argv,
	struct getargs *args,
	int num_args,
	void (*usage)(int, struct getargs*, int)));

int
krb5_prompter_posix __P((
	krb5_context context,
	void *data,
	const char *name,
	const char *banner,
	int num_prompts,
	krb5_prompt prompts[]));

krb5_error_code
krb5_rc_close __P((
	krb5_context context,
	krb5_rcache id));

krb5_error_code
krb5_rc_default __P((
	krb5_context context,
	krb5_rcache *id));

const char *
krb5_rc_default_name __P((krb5_context context));

const char *
krb5_rc_default_type __P((krb5_context context));

krb5_error_code
krb5_rc_destroy __P((
	krb5_context context,
	krb5_rcache id));

krb5_error_code
krb5_rc_expunge __P((
	krb5_context context,
	krb5_rcache id));

krb5_error_code
krb5_rc_get_lifespan __P((
	krb5_context context,
	krb5_rcache id,
	krb5_deltat *auth_lifespan));

const char*
krb5_rc_get_name __P((
	krb5_context context,
	krb5_rcache id));

const char*
krb5_rc_get_type __P((
	krb5_context context,
	krb5_rcache id));

krb5_error_code
krb5_rc_initialize __P((
	krb5_context context,
	krb5_rcache id,
	krb5_deltat auth_lifespan));

krb5_error_code
krb5_rc_recover __P((
	krb5_context context,
	krb5_rcache id));

krb5_error_code
krb5_rc_resolve __P((
	krb5_context context,
	krb5_rcache id,
	const char *name));

krb5_error_code
krb5_rc_resolve_full __P((
	krb5_context context,
	krb5_rcache *id,
	const char *string_name));

krb5_error_code
krb5_rc_resolve_type __P((
	krb5_context context,
	krb5_rcache *id,
	const char *type));

krb5_error_code
krb5_rc_store __P((
	krb5_context context,
	krb5_rcache id,
	krb5_donot_replay *rep));

krb5_error_code
krb5_rd_cred __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_data *in_data,
	krb5_creds ***ret_creds,
	krb5_replay_data *out_data));

krb5_error_code
krb5_rd_cred2 __P((
	krb5_context context,
	krb5_auth_context auth_context,
	krb5_ccache ccache,
	krb5_data *in_data));

krb5_error_code
krb5_rd_error __P((
	krb5_context context,
	krb5_data *msg,
	KRB_ERROR *result));

krb5_error_code
krb5_rd_priv __P((
	krb5_context context,
	krb5_auth_context auth_context,
	const krb5_data *inbuf,
	krb5_data *outbuf,
	void *outdata));

krb5_error_code
krb5_rd_rep __P((
	krb5_context context,
	krb5_auth_context auth_context,
	const krb5_data *inbuf,
	krb5_ap_rep_enc_part **repl));

krb5_error_code
krb5_rd_req __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	const krb5_data *inbuf,
	krb5_const_principal server,
	krb5_keytab keytab,
	krb5_flags *ap_req_options,
	krb5_ticket **ticket));

krb5_error_code
krb5_rd_req_with_keyblock __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	const krb5_data *inbuf,
	krb5_const_principal server,
	krb5_keyblock *keyblock,
	krb5_flags *ap_req_options,
	krb5_ticket **ticket));

krb5_error_code
krb5_rd_safe __P((
	krb5_context context,
	krb5_auth_context auth_context,
	const krb5_data *inbuf,
	krb5_data *outbuf,
	void *outdata));

krb5_error_code
krb5_read_message __P((
	krb5_context context,
	krb5_pointer p_fd,
	krb5_data *data));

krb5_error_code
krb5_read_priv_message __P((
	krb5_context context,
	krb5_auth_context ac,
	krb5_pointer p_fd,
	krb5_data *data));

krb5_error_code
krb5_read_safe_message __P((
	krb5_context context,
	krb5_auth_context ac,
	krb5_pointer p_fd,
	krb5_data *data));

krb5_boolean
krb5_realm_compare __P((
	krb5_context context,
	krb5_const_principal princ1,
	krb5_const_principal princ2));

krb5_error_code
krb5_recvauth __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	krb5_pointer p_fd,
	char *appl_version,
	krb5_principal server,
	int32_t flags,
	krb5_keytab keytab,
	krb5_ticket **ticket));

krb5_error_code
krb5_recvauth_match_version __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	krb5_pointer p_fd,
	krb5_boolean (*match_appl_version)(void *, const char*),
	void *match_data,
	krb5_principal server,
	int32_t flags,
	krb5_keytab keytab,
	krb5_ticket **ticket));

krb5_error_code
krb5_ret_address __P((
	krb5_storage *sp,
	krb5_address *adr));

krb5_error_code
krb5_ret_addrs __P((
	krb5_storage *sp,
	krb5_addresses *adr));

krb5_error_code
krb5_ret_authdata __P((
	krb5_storage *sp,
	krb5_authdata *auth));

krb5_error_code
krb5_ret_creds __P((
	krb5_storage *sp,
	krb5_creds *creds));

krb5_error_code
krb5_ret_data __P((
	krb5_storage *sp,
	krb5_data *data));

krb5_error_code
krb5_ret_int16 __P((
	krb5_storage *sp,
	int16_t *value));

krb5_error_code
krb5_ret_int32 __P((
	krb5_storage *sp,
	int32_t *value));

krb5_error_code
krb5_ret_int8 __P((
	krb5_storage *sp,
	int8_t *value));

krb5_error_code
krb5_ret_keyblock __P((
	krb5_storage *sp,
	krb5_keyblock *p));

krb5_error_code
krb5_ret_principal __P((
	krb5_storage *sp,
	krb5_principal *princ));

krb5_error_code
krb5_ret_string __P((
	krb5_storage *sp,
	char **string));

krb5_error_code
krb5_ret_stringz __P((
	krb5_storage *sp,
	char **string));

krb5_error_code
krb5_ret_times __P((
	krb5_storage *sp,
	krb5_times *times));

krb5_error_code
krb5_salttype_to_string __P((
	krb5_context context,
	krb5_enctype etype,
	krb5_salttype stype,
	char **string));

krb5_error_code
krb5_sendauth __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	krb5_pointer p_fd,
	const char *appl_version,
	krb5_principal client,
	krb5_principal server,
	krb5_flags ap_req_options,
	krb5_data *in_data,
	krb5_creds *in_creds,
	krb5_ccache ccache,
	krb5_error **ret_error,
	krb5_ap_rep_enc_part **rep_result,
	krb5_creds **out_creds));

krb5_error_code
krb5_sendto __P((
	krb5_context context,
	const krb5_data *send,
	krb5_krbhst_handle handle,
	krb5_data *receive));

krb5_error_code
krb5_sendto_kdc __P((
	krb5_context context,
	const krb5_data *send,
	const krb5_realm *realm,
	krb5_data *receive));

krb5_error_code
krb5_sendto_kdc2 __P((
	krb5_context context,
	const krb5_data *send,
	const krb5_realm *realm,
	krb5_data *receive,
	krb5_boolean master));

krb5_error_code
krb5_set_default_in_tkt_etypes __P((
	krb5_context context,
	const krb5_enctype *etypes));

krb5_error_code
krb5_set_default_realm __P((
	krb5_context context,
	const char *realm));

krb5_error_code
krb5_set_error_string __P((
	krb5_context context,
	const char *fmt,
	...))
    __attribute__((format (printf, 2, 3)));

krb5_error_code
krb5_set_extra_addresses __P((
	krb5_context context,
	const krb5_addresses *addresses));

krb5_error_code
krb5_set_fcache_version __P((
	krb5_context context,
	int version));

krb5_error_code
krb5_set_ignore_addresses __P((
	krb5_context context,
	const krb5_addresses *addresses));

void
krb5_set_use_admin_kdc __P((
	krb5_context context,
	krb5_boolean flag));

krb5_error_code
krb5_set_warn_dest __P((
	krb5_context context,
	krb5_log_facility *fac));

krb5_error_code
krb5_sname_to_principal __P((
	krb5_context context,
	const char *hostname,
	const char *sname,
	int32_t type,
	krb5_principal *ret_princ));

krb5_error_code
krb5_sock_to_principal __P((
	krb5_context context,
	int sock,
	const char *sname,
	int32_t type,
	krb5_principal *ret_princ));

krb5_error_code
krb5_sockaddr2address __P((
	krb5_context context,
	const struct sockaddr *sa,
	krb5_address *addr));

krb5_error_code
krb5_sockaddr2port __P((
	krb5_context context,
	const struct sockaddr *sa,
	int16_t *port));

krb5_boolean
krb5_sockaddr_uninteresting __P((const struct sockaddr *sa));

void
krb5_std_usage __P((
	int code,
	struct getargs *args,
	int num_args));

void
krb5_storage_clear_flags __P((
	krb5_storage *sp,
	krb5_flags flags));

krb5_storage *
krb5_storage_emem __P((void));

krb5_error_code
krb5_storage_free __P((krb5_storage *sp));

krb5_storage *
krb5_storage_from_data __P((krb5_data *data));

krb5_storage *
krb5_storage_from_fd __P((int fd));

krb5_storage *
krb5_storage_from_mem __P((
	void *buf,
	size_t len));

krb5_flags
krb5_storage_get_byteorder __P((
	krb5_storage *sp,
	krb5_flags byteorder));

krb5_boolean
krb5_storage_is_flags __P((
	krb5_storage *sp,
	krb5_flags flags));

void
krb5_storage_set_byteorder __P((
	krb5_storage *sp,
	krb5_flags byteorder));

void
krb5_storage_set_flags __P((
	krb5_storage *sp,
	krb5_flags flags));

krb5_error_code
krb5_storage_to_data __P((
	krb5_storage *sp,
	krb5_data *data));

krb5_error_code
krb5_store_address __P((
	krb5_storage *sp,
	krb5_address p));

krb5_error_code
krb5_store_addrs __P((
	krb5_storage *sp,
	krb5_addresses p));

krb5_error_code
krb5_store_authdata __P((
	krb5_storage *sp,
	krb5_authdata auth));

krb5_error_code
krb5_store_creds __P((
	krb5_storage *sp,
	krb5_creds *creds));

krb5_error_code
krb5_store_data __P((
	krb5_storage *sp,
	krb5_data data));

krb5_error_code
krb5_store_int16 __P((
	krb5_storage *sp,
	int16_t value));

krb5_error_code
krb5_store_int32 __P((
	krb5_storage *sp,
	int32_t value));

krb5_error_code
krb5_store_int8 __P((
	krb5_storage *sp,
	int8_t value));

krb5_error_code
krb5_store_keyblock __P((
	krb5_storage *sp,
	krb5_keyblock p));

krb5_error_code
krb5_store_principal __P((
	krb5_storage *sp,
	krb5_principal p));

krb5_error_code
krb5_store_string __P((
	krb5_storage *sp,
	const char *s));

krb5_error_code
krb5_store_stringz __P((
	krb5_storage *sp,
	const char *s));

krb5_error_code
krb5_store_times __P((
	krb5_storage *sp,
	krb5_times times));

krb5_error_code
krb5_string_to_deltat __P((
	const char *string,
	krb5_deltat *deltat));

krb5_error_code
krb5_string_to_enctype __P((
	krb5_context context,
	const char *string,
	krb5_enctype *etype));

krb5_error_code
krb5_string_to_key __P((
	krb5_context context,
	krb5_enctype enctype,
	const char *password,
	krb5_principal principal,
	krb5_keyblock *key));

krb5_error_code
krb5_string_to_key_data __P((
	krb5_context context,
	krb5_enctype enctype,
	krb5_data password,
	krb5_principal principal,
	krb5_keyblock *key));

krb5_error_code
krb5_string_to_key_data_salt __P((
	krb5_context context,
	krb5_enctype enctype,
	krb5_data password,
	krb5_salt salt,
	krb5_keyblock *key));

krb5_error_code
krb5_string_to_key_derived __P((
	krb5_context context,
	const void *str,
	size_t len,
	krb5_enctype etype,
	krb5_keyblock *key));

krb5_error_code
krb5_string_to_key_salt __P((
	krb5_context context,
	krb5_enctype enctype,
	const char *password,
	krb5_salt salt,
	krb5_keyblock *key));

krb5_error_code
krb5_string_to_keytype __P((
	krb5_context context,
	const char *string,
	krb5_keytype *keytype));

krb5_error_code
krb5_string_to_salttype __P((
	krb5_context context,
	krb5_enctype etype,
	const char *string,
	krb5_salttype *salttype));

krb5_error_code
krb5_timeofday __P((
	krb5_context context,
	krb5_timestamp *timeret));

krb5_error_code
krb5_unparse_name __P((
	krb5_context context,
	krb5_const_principal principal,
	char **name));

krb5_error_code
krb5_unparse_name_fixed __P((
	krb5_context context,
	krb5_const_principal principal,
	char *name,
	size_t len));

krb5_error_code
krb5_unparse_name_fixed_short __P((
	krb5_context context,
	krb5_const_principal principal,
	char *name,
	size_t len));

krb5_error_code
krb5_unparse_name_short __P((
	krb5_context context,
	krb5_const_principal principal,
	char **name));

krb5_error_code
krb5_us_timeofday __P((
	krb5_context context,
	int32_t *sec,
	int32_t *usec));

krb5_error_code
krb5_vabort __P((
	krb5_context context,
	krb5_error_code code,
	const char *fmt,
	va_list ap))
    __attribute__ ((noreturn, format (printf, 3, 0)));

krb5_error_code
krb5_vabortx __P((
	krb5_context context,
	const char *fmt,
	va_list ap))
    __attribute__ ((noreturn, format (printf, 2, 0)));

krb5_error_code
krb5_verify_ap_req __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	krb5_ap_req *ap_req,
	krb5_const_principal server,
	krb5_keyblock *keyblock,
	krb5_flags flags,
	krb5_flags *ap_req_options,
	krb5_ticket **ticket));

krb5_error_code
krb5_verify_ap_req2 __P((
	krb5_context context,
	krb5_auth_context *auth_context,
	krb5_ap_req *ap_req,
	krb5_const_principal server,
	krb5_keyblock *keyblock,
	krb5_flags flags,
	krb5_flags *ap_req_options,
	krb5_ticket **ticket,
	krb5_key_usage usage));

krb5_error_code
krb5_verify_authenticator_checksum __P((
	krb5_context context,
	krb5_auth_context ac,
	void *data,
	size_t len));

krb5_error_code
krb5_verify_checksum __P((
	krb5_context context,
	krb5_crypto crypto,
	krb5_key_usage usage,
	void *data,
	size_t len,
	Checksum *cksum));

krb5_error_code
krb5_verify_init_creds __P((
	krb5_context context,
	krb5_creds *creds,
	krb5_principal ap_req_server,
	krb5_keytab ap_req_keytab,
	krb5_ccache *ccache,
	krb5_verify_init_creds_opt *options));

void
krb5_verify_init_creds_opt_init __P((krb5_verify_init_creds_opt *options));

void
krb5_verify_init_creds_opt_set_ap_req_nofail __P((
	krb5_verify_init_creds_opt *options,
	int ap_req_nofail));

void
krb5_verify_opt_init __P((krb5_verify_opt *opt));

void
krb5_verify_opt_set_ccache __P((
	krb5_verify_opt *opt,
	krb5_ccache ccache));

void
krb5_verify_opt_set_flags __P((
	krb5_verify_opt *opt,
	unsigned int flags));

void
krb5_verify_opt_set_keytab __P((
	krb5_verify_opt *opt,
	krb5_keytab keytab));

void
krb5_verify_opt_set_secure __P((
	krb5_verify_opt *opt,
	krb5_boolean secure));

void
krb5_verify_opt_set_service __P((
	krb5_verify_opt *opt,
	const char *service));

krb5_error_code
krb5_verify_user __P((
	krb5_context context,
	krb5_principal principal,
	krb5_ccache ccache,
	const char *password,
	krb5_boolean secure,
	const char *service));

krb5_error_code
krb5_verify_user_lrealm __P((
	krb5_context context,
	krb5_principal principal,
	krb5_ccache ccache,
	const char *password,
	krb5_boolean secure,
	const char *service));

krb5_error_code
krb5_verify_user_opt __P((
	krb5_context context,
	krb5_principal principal,
	const char *password,
	krb5_verify_opt *opt));

krb5_error_code
krb5_verr __P((
	krb5_context context,
	int eval,
	krb5_error_code code,
	const char *fmt,
	va_list ap))
    __attribute__ ((noreturn, format (printf, 4, 0)));

krb5_error_code
krb5_verrx __P((
	krb5_context context,
	int eval,
	const char *fmt,
	va_list ap))
    __attribute__ ((noreturn, format (printf, 3, 0)));

krb5_error_code
krb5_vlog __P((
	krb5_context context,
	krb5_log_facility *fac,
	int level,
	const char *fmt,
	va_list ap))
    __attribute__((format (printf, 4, 0)));

krb5_error_code
krb5_vlog_msg __P((
	krb5_context context,
	krb5_log_facility *fac,
	char **reply,
	int level,
	const char *fmt,
	va_list ap))
    __attribute__((format (printf, 5, 0)));

krb5_error_code
krb5_vset_error_string __P((
	krb5_context context,
	const char *fmt,
	va_list args))
    __attribute__ ((format (printf, 2, 0)));

krb5_error_code
krb5_vwarn __P((
	krb5_context context,
	krb5_error_code code,
	const char *fmt,
	va_list ap))
    __attribute__ ((format (printf, 3, 0)));

krb5_error_code
krb5_vwarnx __P((
	krb5_context context,
	const char *fmt,
	va_list ap))
    __attribute__ ((format (printf, 2, 0)));

krb5_error_code
krb5_warn __P((
	krb5_context context,
	krb5_error_code code,
	const char *fmt,
	...))
    __attribute__ ((format (printf, 3, 4)));

krb5_error_code
krb5_warnx __P((
	krb5_context context,
	const char *fmt,
	...))
    __attribute__ ((format (printf, 2, 3)));

krb5_error_code
krb5_write_message __P((
	krb5_context context,
	krb5_pointer p_fd,
	krb5_data *data));

krb5_error_code
krb5_write_priv_message __P((
	krb5_context context,
	krb5_auth_context ac,
	krb5_pointer p_fd,
	krb5_data *data));

krb5_error_code
krb5_write_safe_message __P((
	krb5_context context,
	krb5_auth_context ac,
	krb5_pointer p_fd,
	krb5_data *data));

krb5_error_code
krb5_xfree __P((void *ptr));

krb5_error_code
principalname2krb5_principal __P((
	krb5_principal *principal,
	const PrincipalName from,
	const Realm realm));

#endif /* __krb5_protos_h__ */
