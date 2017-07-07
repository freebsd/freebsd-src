#ifndef __LOADFUNCS_KRB5_H__
#define __LOADFUNCS_KRB5_H__

#include "loadfuncs.h"
#include <krb5.h>

#if defined(_WIN64)
#define KRB5_DLL      "krb5_64.dll"
#else
#define KRB5_DLL      "krb5_32.dll"
#endif

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_principal,
    (krb5_context, krb5_principal)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_authenticator,
    (krb5_context, krb5_authenticator * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_authenticator_contents,
    (krb5_context, krb5_authenticator * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_addresses,
    (krb5_context, krb5_address * * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_address,
    (krb5_context, krb5_address * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_authdata,
    (krb5_context, krb5_authdata * * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_enc_tkt_part,
    (krb5_context, krb5_enc_tkt_part * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_ticket,
    (krb5_context, krb5_ticket * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_tickets,
    (krb5_context, krb5_ticket * * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_kdc_req,
    (krb5_context, krb5_kdc_req * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_kdc_rep,
    (krb5_context, krb5_kdc_rep * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_last_req,
    (krb5_context, krb5_last_req_entry * * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_enc_kdc_rep_part,
    (krb5_context, krb5_enc_kdc_rep_part * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_error,
    (krb5_context, krb5_error * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_ap_req,
    (krb5_context, krb5_ap_req * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_ap_rep,
    (krb5_context, krb5_ap_rep * )
    );

/* Removed around the time of krb5_rc_* change... */
#if 0
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_safe,
    (krb5_context, krb5_safe * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_priv,
    (krb5_context, krb5_priv * )
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_priv_enc_part,
    (krb5_context, krb5_priv_enc_part * )
    );
#endif

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_cred,
    (krb5_context, krb5_cred *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_creds,
    (krb5_context, krb5_creds *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_cred_contents,
    (krb5_context, krb5_creds *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_cred_enc_part,
    (krb5_context, krb5_cred_enc_part *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_checksum,
    (krb5_context, krb5_checksum *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_checksum_contents,
    (krb5_context, krb5_checksum *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_keyblock,
    (krb5_context, krb5_keyblock *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_keyblock_contents,
    (krb5_context, krb5_keyblock *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_pa_data,
    (krb5_context, krb5_pa_data * *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_ap_rep_enc_part,
    (krb5_context, krb5_ap_rep_enc_part *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_tkt_authent,
    (krb5_context, krb5_tkt_authent *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_data,
    (krb5_context, krb5_data *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_data_contents,
    (krb5_context, krb5_data *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_unparsed_name,
    (krb5_context, char *)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_cksumtypes,
    (krb5_context, krb5_cksumtype *)
    );

/* ------------------------------------------------------------------------- */

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_encrypt,
    (krb5_context context, const krb5_keyblock *key,
     krb5_keyusage usage, const krb5_data *ivec,
     const krb5_data *input, krb5_enc_data *output)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_decrypt,
    (krb5_context context, const krb5_keyblock *key,
     krb5_keyusage usage, const krb5_data *ivec,
     const krb5_enc_data *input, krb5_data *output)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_encrypt_length,
    (krb5_context context, krb5_enctype enctype,
     size_t inputlen, size_t *length)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_block_size,
    (krb5_context context, krb5_enctype enctype,
     size_t *blocksize)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_make_random_key,
    (krb5_context context, krb5_enctype enctype,
     krb5_keyblock *random_key)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_random_make_octets,
    (krb5_context context, krb5_data *data)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_random_seed,
    (krb5_context context, krb5_data *data)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_string_to_key,
    (krb5_context context, krb5_enctype enctype,
     const krb5_data *string, const krb5_data *salt,
     krb5_keyblock *key)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_enctype_compare,
    (krb5_context context, krb5_enctype e1, krb5_enctype e2,
     krb5_boolean *similar)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_make_checksum,
    (krb5_context context, krb5_cksumtype cksumtype,
     const krb5_keyblock *key, krb5_keyusage usage,
     const krb5_data *input, krb5_checksum *cksum)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_verify_checksum,
    (krb5_context context,
     const krb5_keyblock *key, krb5_keyusage usage,
     const krb5_data *data,
     const krb5_checksum *cksum,
     krb5_boolean *valid)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_checksum_length,
    (krb5_context context, krb5_cksumtype cksumtype,
     size_t *length)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_c_keyed_checksum_types,
    (krb5_context context, krb5_enctype enctype,
     unsigned int *count, krb5_cksumtype **cksumtypes)
    );

/* ------------------------------------------------------------------------- */

TYPEDEF_FUNC(
    krb5_boolean,
    KRB5_CALLCONV,
    valid_enctype,
    (const krb5_enctype ktype)
    );

TYPEDEF_FUNC(
    krb5_boolean,
    KRB5_CALLCONV,
    valid_cksumtype,
    (const krb5_cksumtype ctype)
    );

TYPEDEF_FUNC(
    krb5_boolean,
    KRB5_CALLCONV,
    is_coll_proof_cksum,
    (const krb5_cksumtype ctype)
    );

TYPEDEF_FUNC(
    krb5_boolean,
    KRB5_CALLCONV,
    is_keyed_cksum,
    (const krb5_cksumtype ctype)
    );

/* ------------------------------------------------------------------------- */

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_init_context,
    (krb5_context *)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_context,
    (krb5_context)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_decrypt_tkt_part,
    (krb5_context,
     const krb5_keyblock *,
     krb5_ticket * )
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_tgt_creds,
    (krb5_context,
     krb5_creds ** )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_credentials,
    (krb5_context,
     const krb5_flags,
     krb5_ccache,
     krb5_creds *,
     krb5_creds * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_credentials_validate,
    (krb5_context,
     const krb5_flags,
     krb5_ccache,
     krb5_creds *,
     krb5_creds * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_credentials_renew,
    (krb5_context,
     const krb5_flags,
     krb5_ccache,
     krb5_creds *,
     krb5_creds * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_req,
    (krb5_context,
     krb5_auth_context *,
     const krb5_flags,
     char *,
     char *,
     krb5_data *,
     krb5_ccache,
     krb5_data * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_req_extended,
    (krb5_context,
     krb5_auth_context *,
     const krb5_flags,
     krb5_data *,
     krb5_creds *,
     krb5_data * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_rep,
    (krb5_context,
     krb5_auth_context,
     krb5_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_rd_rep,
    (krb5_context,
     krb5_auth_context,
     const krb5_data *,
     krb5_ap_rep_enc_part * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_error,
    (krb5_context,
     const krb5_error *,
     krb5_data * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_rd_error,
    (krb5_context,
     const krb5_data *,
     krb5_error * * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_rd_safe,
    (krb5_context,
     krb5_auth_context,
     const krb5_data *,
     krb5_data *,
     krb5_replay_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_rd_priv,
    (krb5_context,
     krb5_auth_context,
     const krb5_data *,
     krb5_data *,
     krb5_replay_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_parse_name,
    (krb5_context,
     const char *,
     krb5_principal * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_unparse_name,
    (krb5_context,
     krb5_const_principal,
     char * * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_unparse_name_ext,
    (krb5_context,
     krb5_const_principal,
     char * *,
     int *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_set_principal_realm,
    (krb5_context, krb5_principal, const char *)
    );

TYPEDEF_FUNC(
    krb5_boolean,
    KRB5_CALLCONV,
    krb5_principal_compare,
    (krb5_context,
     krb5_const_principal,
     krb5_const_principal)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_keyblock,
    (krb5_context,
     const krb5_keyblock *,
     krb5_keyblock * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_keyblock_contents,
    (krb5_context,
     const krb5_keyblock *,
     krb5_keyblock *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_creds,
    (krb5_context,
     const krb5_creds *,
     krb5_creds * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_data,
    (krb5_context,
     const krb5_data *,
     krb5_data * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_principal,
    (krb5_context,
     krb5_const_principal,
     krb5_principal *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_addr,
    (krb5_context,
     const krb5_address *,
     krb5_address * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_addresses,
    (krb5_context,
     krb5_address * const *,
     krb5_address * * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_ticket,
    (krb5_context,
     const krb5_ticket *,
     krb5_ticket * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_authdata,
    (krb5_context,
     krb5_authdata * const *,
     krb5_authdata * * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_authenticator,
    (krb5_context,
     const krb5_authenticator *,
     krb5_authenticator * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_copy_checksum,
    (krb5_context,
     const krb5_checksum *,
     krb5_checksum * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_server_rcache,
    (krb5_context,
     const krb5_data *, krb5_rcache *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV_C,
    krb5_build_principal_ext,
    (krb5_context, krb5_principal *, int, const char *, ...)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV_C,
    krb5_build_principal,
    (krb5_context, krb5_principal *, int, const char *, ...)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_425_conv_principal,
    (krb5_context,
     const char *name,
     const char *instance, const char *realm,
     krb5_principal *princ)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_524_conv_principal,
    (krb5_context context, const krb5_principal princ,
     char *name, char *inst, char *realm)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_chpw_req,
    (krb5_context context, krb5_auth_context auth_context,
     krb5_data *ap_req, char *passwd, krb5_data *packet)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_rd_chpw_rep,
    (krb5_context context, krb5_auth_context auth_context,
     krb5_data *packet, int *result_code,
     krb5_data *result_data)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_chpw_result_code_string,
    (krb5_context context, int result_code,
     char **result_codestr)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_register,
    (krb5_context,
     struct _krb5_kt_ops * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_resolve,
    (krb5_context,
     const char *,
     krb5_keytab * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_default_name,
    (krb5_context,
     char *,
     int )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_default,
    (krb5_context,
     krb5_keytab * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_free_entry,
    (krb5_context,
     krb5_keytab_entry * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_remove_entry,
    (krb5_context,
     krb5_keytab,
     krb5_keytab_entry * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_add_entry,
    (krb5_context,
     krb5_keytab,
     krb5_keytab_entry * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_resolve,
    (krb5_context,
     const char *,
     krb5_ccache * )
    );

TYPEDEF_FUNC(
    const char*,
    KRB5_CALLCONV,
    krb5_cc_default_name,
    (krb5_context)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_set_default_name,
    (krb5_context, const char *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_default,
    (krb5_context,
     krb5_ccache *)
    );

TYPEDEF_FUNC(
    unsigned int,
    KRB5_CALLCONV,
    krb5_get_notification_message,
    (void)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_copy_creds,
    (krb5_context context,
     krb5_ccache incc,
     krb5_ccache outcc)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_us_timeofday,
    (krb5_context,
     krb5_int32 *,
     krb5_int32 * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_timeofday,
    (krb5_context,
     krb5_int32 * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_os_localaddr,
    (krb5_context,
     krb5_address * * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_default_realm,
    (krb5_context,
     char * * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_set_default_realm,
    (krb5_context,
     const char * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_free_default_realm,
    (krb5_context,
     const char * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_sname_to_principal,
    (krb5_context,
     const char *,
     const char *,
     krb5_int32,
     krb5_principal *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_change_password,
    (krb5_context context, krb5_creds *creds, char *newpw,
     int *result_code, krb5_data *result_code_string,
     krb5_data *result_string)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_default_config_files,
    (char ***filenames)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_config_files,
    (char **filenames)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_in_tkt,
    (krb5_context,
     const krb5_flags,
     krb5_address * const *,
     krb5_enctype *,
     krb5_preauthtype *,
     krb5_error_code ( * )(krb5_context,
                           const krb5_enctype,
                           krb5_data *,
                           krb5_const_pointer,
                           krb5_keyblock * *),
     krb5_const_pointer,
     krb5_error_code ( * )(krb5_context,
                           const krb5_keyblock *,
                           krb5_const_pointer,
                           krb5_kdc_rep * ),
     krb5_const_pointer,
     krb5_creds *,
     krb5_ccache,
     krb5_kdc_rep * * )
    );


TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_in_tkt_with_password,
    (krb5_context,
     const krb5_flags,
     krb5_address * const *,
     krb5_enctype *,
     krb5_preauthtype *,
     const char *,
     krb5_ccache,
     krb5_creds *,
     krb5_kdc_rep * * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_in_tkt_with_skey,
    (krb5_context,
     const krb5_flags,
     krb5_address * const *,
     krb5_enctype *,
     krb5_preauthtype *,
     const krb5_keyblock *,
     krb5_ccache,
     krb5_creds *,
     krb5_kdc_rep * * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_in_tkt_with_keytab,
    (krb5_context,
     const krb5_flags,
     krb5_address * const *,
     krb5_enctype *,
     krb5_preauthtype *,
     const krb5_keytab,
     krb5_ccache,
     krb5_creds *,
     krb5_kdc_rep * * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_rd_req,
    (krb5_context,
     krb5_auth_context *,
     const krb5_data *,
     krb5_const_principal,
     krb5_keytab,
     krb5_flags *,
     krb5_ticket * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_read_service_key,
    (krb5_context,
     krb5_pointer,
     krb5_principal,
     krb5_kvno,
     krb5_enctype,
     krb5_keyblock * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_safe,
    (krb5_context,
     krb5_auth_context,
     const krb5_data *,
     krb5_data *,
     krb5_replay_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_priv,
    (krb5_context,
     krb5_auth_context,
     const krb5_data *,
     krb5_data *,
     krb5_replay_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_register,
    (krb5_context,
     krb5_cc_ops *,
     krb5_boolean )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_sendauth,
    (krb5_context,
     krb5_auth_context *,
     krb5_pointer,
     char *,
     krb5_principal,
     krb5_principal,
     krb5_flags,
     krb5_data *,
     krb5_creds *,
     krb5_ccache,
     krb5_error * *,
     krb5_ap_rep_enc_part * *,
     krb5_creds * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_recvauth,
    (krb5_context,
     krb5_auth_context *,
     krb5_pointer,
     char *,
     krb5_principal,
     krb5_int32,
     krb5_keytab,
     krb5_ticket * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_ncred,
    (krb5_context,
     krb5_auth_context,
     krb5_creds * *,
     krb5_data * *,
     krb5_replay_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_mk_1cred,
    (krb5_context,
     krb5_auth_context,
     krb5_creds *,
     krb5_data * *,
     krb5_replay_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_rd_cred,
    (krb5_context,
     krb5_auth_context,
     krb5_data *,
     krb5_creds * * *,
     krb5_replay_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_fwd_tgt_creds,
    (krb5_context,
     krb5_auth_context,
     char *,
     krb5_principal,
     krb5_principal,
     krb5_ccache,
     int forwardable,
     krb5_data *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_init,
    (krb5_context,
     krb5_auth_context *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_free,
    (krb5_context,
     krb5_auth_context)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_setflags,
    (krb5_context,
     krb5_auth_context,
     krb5_int32)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_getflags,
    (krb5_context,
     krb5_auth_context,
     krb5_int32 *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_setuseruserkey,
    (krb5_context,
     krb5_auth_context,
     krb5_keyblock *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_getkey,
    (krb5_context,
     krb5_auth_context,
     krb5_keyblock **)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_getlocalsubkey,
    (krb5_context,
     krb5_auth_context,
     krb5_keyblock * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_set_req_cksumtype,
    (krb5_context,
     krb5_auth_context,
     krb5_cksumtype)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_getlocalseqnumber,
    (krb5_context,
     krb5_auth_context,
     krb5_int32 *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_getremoteseqnumber,
    (krb5_context,
     krb5_auth_context,
     krb5_int32 *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_setrcache,
    (krb5_context,
     krb5_auth_context,
     krb5_rcache)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_getauthenticator,
    (krb5_context,
     krb5_auth_context,
     krb5_authenticator * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_getremotesubkey,
    (krb5_context,
     krb5_auth_context,
     krb5_keyblock * *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_read_password,
    (krb5_context,
     const char *,
     const char *,
     char *,
     int * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_host_realm,
    (krb5_context,
     const char *,
     char * * * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_free_host_realm,
    (krb5_context,
     char * const * )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_realm_domain,
    (krb5_context,
     const char *,
     char ** )
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_auth_con_genaddrs,
    (krb5_context,
     krb5_auth_context,
     int, int)
    );

/* ------------------------------------------------------------------------- */

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_string_to_enctype,
    (char *, krb5_enctype *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_string_to_salttype,
    (char *, krb5_int32 *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_string_to_cksumtype,
    (char *, krb5_cksumtype *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_string_to_timestamp,
    (char *, krb5_timestamp *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_string_to_deltat,
    (char *, krb5_deltat *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_enctype_to_string,
    (krb5_enctype, char *, size_t)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_enctype_to_name,
    (krb5_enctype, krb5_boolean, char *, size_t)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_salttype_to_string,
    (krb5_int32, char *, size_t)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cksumtype_to_string,
    (krb5_cksumtype, char *, size_t)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_timestamp_to_string,
    (krb5_timestamp, char *, size_t)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_timestamp_to_sfstring,
    (krb5_timestamp, char *, size_t, char *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_deltat_to_string,
    (krb5_deltat, char *, size_t)
    );

/* ------------------------------------------------------------------------- */

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_prompter_posix,
    (krb5_context context,
     void *data,
     const char *name,
     const char *banner,
     int num_prompts,
     krb5_prompt prompts[])
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_alloc,
    (krb5_context ctx,
     krb5_get_init_creds_opt **opt)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_free,
    (krb5_context ctx,
     krb5_get_init_creds_opt *opt)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_init,
    (krb5_get_init_creds_opt *opt)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_tkt_life,
    (krb5_get_init_creds_opt *opt,
     krb5_deltat tkt_life)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_renew_life,
    (krb5_get_init_creds_opt *opt,
     krb5_deltat renew_life)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_forwardable,
    (krb5_get_init_creds_opt *opt,
     int forwardable)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_proxiable,
    (krb5_get_init_creds_opt *opt,
     int proxiable)
    );


TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_etype_list,
    (krb5_get_init_creds_opt *opt,
     krb5_enctype *etype_list,
     int etype_list_length)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_address_list,
    (krb5_get_init_creds_opt *opt,
     krb5_address **addresses)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_preauth_list,
    (krb5_get_init_creds_opt *opt,
     krb5_preauthtype *preauth_list,
     int preauth_list_length)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_salt,
    (krb5_get_init_creds_opt *opt,
     krb5_data *salt)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_change_password_prompt,
    (krb5_get_init_creds_opt *opt,
     int prompt)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_init_creds_opt_set_out_ccache,
    (krb5_context context,
     krb5_get_init_creds_opt *opt,
     krb5_ccache ccache)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_init_creds_password,
    (krb5_context context,
     krb5_creds *creds,
     krb5_principal client,
     char *password,
     krb5_prompter_fct prompter,
     void *data,
     krb5_deltat start_time,
     char *in_tkt_service,
     krb5_get_init_creds_opt *options)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_init_creds_keytab,
    (krb5_context context,
     krb5_creds *creds,
     krb5_principal client,
     krb5_keytab arg_keytab,
     krb5_deltat start_time,
     char *in_tkt_service,
     krb5_get_init_creds_opt *options)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_verify_init_creds_opt_init,
    (krb5_verify_init_creds_opt *options)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_verify_init_creds_opt_set_ap_req_nofail,
    (krb5_verify_init_creds_opt *options,
     int ap_req_nofail)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_verify_init_creds,
    (krb5_context context,
     krb5_creds *creds,
     krb5_principal ap_req_server,
     krb5_keytab ap_req_keytab,
     krb5_ccache *ccache,
     krb5_verify_init_creds_opt *options)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_validated_creds,
    (krb5_context context,
     krb5_creds *creds,
     krb5_principal client,
     krb5_ccache ccache,
     char *in_tkt_service)
    );


TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_get_renewed_creds,
    (krb5_context context,
     krb5_creds *creds,
     krb5_principal client,
     krb5_ccache ccache,
     char *in_tkt_service)
    );

/* ------------------------------------------------------------------------- */

TYPEDEF_FUNC(
    krb5_prompt_type*,
    KRB5_CALLCONV,
    krb5_get_prompt_types,
    (krb5_context context)
    );

/* NOT IN krb5.h HEADER: */

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_decode_ticket,
    (const krb5_data *code, krb5_ticket **rep)
    );

/* --- more --- */

TYPEDEF_FUNC(
    char *,
    KRB5_CALLCONV,
    krb5_cc_get_name,
    (krb5_context context, krb5_ccache cache)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_gen_new,
    (krb5_context context, krb5_ccache *cache)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_initialize,
    (krb5_context context, krb5_ccache cache, krb5_principal principal)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_destroy,
    (krb5_context context, krb5_ccache cache)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_close,
    (krb5_context context, krb5_ccache cache)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_store_cred,
    (krb5_context context, krb5_ccache cache, krb5_creds *creds)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_retrieve_cred,
    (krb5_context context, krb5_ccache cache,
     krb5_flags flags, krb5_creds *mcreds,
     krb5_creds *creds)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_get_principal,
    (krb5_context context, krb5_ccache cache, krb5_principal *principal)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_start_seq_get,
    (krb5_context context, krb5_ccache cache, krb5_cc_cursor *cursor)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_next_cred,
    (krb5_context context, krb5_ccache cache, krb5_cc_cursor *cursor,
     krb5_creds *creds)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_end_seq_get,
    (krb5_context context, krb5_ccache cache, krb5_cc_cursor *cursor)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_remove_cred,
    (krb5_context context, krb5_ccache cache, krb5_flags flags,
     krb5_creds *creds)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_set_flags,
    (krb5_context context, krb5_ccache cache, krb5_flags flags)
    );

TYPEDEF_FUNC(
    const char *,
    KRB5_CALLCONV,
    krb5_cc_get_type,
    (krb5_context context, krb5_ccache cache)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_get_full_name,
    (krb5_context context, krb5_ccache cache, char **)
    );

TYPEDEF_FUNC(
    char *,
    KRB5_CALLCONV,
    krb5_kt_get_type,
    (krb5_context, krb5_keytab keytab)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_get_name,
    (krb5_context context, krb5_keytab keytab, char *name,
     unsigned int namelen)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_close,
    (krb5_context context, krb5_keytab keytab)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_get_entry,
    (krb5_context context, krb5_keytab keytab,
     krb5_const_principal principal, krb5_kvno vno,
     krb5_enctype enctype, krb5_keytab_entry *entry)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_start_seq_get,
    (krb5_context context, krb5_keytab keytab, krb5_kt_cursor *cursor)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_next_entry,
    (krb5_context context, krb5_keytab keytab,
     krb5_keytab_entry *entry, krb5_kt_cursor *cursor)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_kt_end_seq_get,
    (krb5_context context, krb5_keytab keytab, krb5_kt_cursor *cursor)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_locate_kdc,
    (krb5_context context, const krb5_data *realm,
      struct addrlist *addrlist,
      int get_masters, int socktype, int family)
    );

TYPEDEF_FUNC(
    const char *,
    KRB5_CALLCONV,
    krb5_get_error_message,
    (krb5_context, krb5_error_code)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_error_message,
    (krb5_context, const char *)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_clear_error_message,
    (krb5_context)
    );

TYPEDEF_FUNC(
    krb5_boolean,
    KRB5_CALLCONV,
    krb5_is_config_principal,
    (krb5_context, krb5_const_principal)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cccol_cursor_new,
    (krb5_context, krb5_cccol_cursor *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cccol_cursor_next,
    (krb5_context, krb5_cccol_cursor cursor, krb5_ccache *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cccol_cursor_free,
    (krb5_context, krb5_cccol_cursor *cursor)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_cache_match,
    (krb5_context, krb5_principal, krb5_ccache *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_new_unique,
    (krb5_context, const char *, const char *, krb5_ccache *)
    );

TYPEDEF_FUNC(
    krb5_boolean,
    KRB5_CALLCONV,
    krb5_cc_support_switch,
    (krb5_context, const char *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5_cc_switch,
    (krb5_context, krb5_ccache)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    krb5_free_string,
    (krb5_context, char *)
    );

TYPEDEF_FUNC(
    krb5_error_code,
    KRB5_CALLCONV,
    krb5int_cc_user_set_default_name,
    (krb5_context context, const char *)
    );

#endif /* __LOADFUNCS_KRB5_H__ */
