#ifndef _LEASHDLL_H_
#define _LEASHDLL_H_

#include <com_err.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef NO_KRB4
/*
 * This is a hack needed because the real com_err.h does
 * not define err_func.  We need it in the case where
 * we pull in the real com_err instead of the krb4
 * impostor.
 */
#ifndef _DCNS_MIT_COM_ERR_H
typedef LPSTR (*err_func)(int, long);
#endif

#include <krberr.h>
extern void Leash_initialize_krb_error_func(err_func func,struct et_list **);
#undef init_krb_err_func
#define init_krb_err_func(erf) Leash_initialize_krb_error_func(erf,&_et_list)

#include <kadm_err.h>

extern void Leash_initialize_kadm_error_table(struct et_list **);
#undef init_kadm_err_tbl
#define init_kadm_err_tbl() Leash_initialize_kadm_error_table(&_et_list)
#define kadm_err_base ERROR_TABLE_BASE_kadm
#endif

#define krb_err_func Leash_krb_err_func

#include <stdarg.h>
int lsh_com_err_proc (LPSTR whoami, long code,
		      LPSTR fmt, va_list args);
void FAR Leash_load_com_err_callback(FARPROC,FARPROC,FARPROC);


#ifndef KRBERR
#define KRBERR(code) (code + krb_err_base)
#endif


/* Internal Stuff */

#include <windows.h>
#define SECURITY_WIN32
#include <security.h>

/* _WIN32_WINNT must be 0x0501 or greater to pull in definition of
 * all required LSA data types when the Vista SDK NtSecAPI.h is used.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#else
#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif
#include <ntsecapi.h>

#include <krb5.h>

#ifndef NO_KRB4
extern HINSTANCE hKrb4;
#endif
extern HINSTANCE hKrb5;
extern HINSTANCE hProfile;

#define TIMEHOST "TIMEHOST"

#define LEASH_DEBUG_CLASS_GENERIC   0
#define LEASH_DEBUG_CLASS_KRB4      1
#define LEASH_DEBUG_CLASS_KRB4_APP  2

#define LEASH_PRIORITY_LOW  0
#define LEASH_PRIORITY_HIGH 1

///////////////////////////////////////////////////////////////////////////////

#ifdef _WIN64
#define LEASH_DLL     "leashw64.dll"
#define KRBCC32_DLL   "krbcc64.dll"
#else
#define LEASH_DLL     "leashw32.dll"
#define KRBCC32_DLL   "krbcc32.dll"
#endif
#define SERVICE_DLL   "advapi32.dll"
#define SECUR32_DLL   "secur32.dll"

//////////////////////////////////////////////////////////////////////////////

#include <loadfuncs-com_err.h>
#include <loadfuncs-krb5.h>
#include <loadfuncs-profile.h>
#ifndef NO_KRB4
#include <loadfuncs-krb.h>
#include <loadfuncs-krb524.h>
#endif
#include <loadfuncs-lsa.h>

#include <errno.h>

#ifndef NO_AFS
////Can't find it!
////#include "afscompat.h"
#endif

// service definitions
typedef SC_HANDLE (WINAPI *FP_OpenSCManagerA)(char *, char *, DWORD);
typedef SC_HANDLE (WINAPI *FP_OpenServiceA)(SC_HANDLE, char *, DWORD);
typedef BOOL (WINAPI *FP_QueryServiceStatus)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL (WINAPI *FP_CloseServiceHandle)(SC_HANDLE);

//////////////////////////////////////////////////////////////////////////////

#ifndef NO_KRB4
// krb4 functions
extern DECL_FUNC_PTR(get_krb_err_txt_entry);
extern DECL_FUNC_PTR(k_isinst);
extern DECL_FUNC_PTR(k_isname);
extern DECL_FUNC_PTR(k_isrealm);
extern DECL_FUNC_PTR(kadm_change_your_password);
extern DECL_FUNC_PTR(kname_parse);
extern DECL_FUNC_PTR(krb_get_cred);
extern DECL_FUNC_PTR(krb_get_krbhst);
extern DECL_FUNC_PTR(krb_get_lrealm);
extern DECL_FUNC_PTR(krb_get_pw_in_tkt);
extern DECL_FUNC_PTR(krb_get_tf_realm);
extern DECL_FUNC_PTR(krb_mk_req);
extern DECL_FUNC_PTR(krb_realmofhost);
extern DECL_FUNC_PTR(tf_init);
extern DECL_FUNC_PTR(tf_close);
extern DECL_FUNC_PTR(tf_get_cred);
extern DECL_FUNC_PTR(tf_get_pname);
extern DECL_FUNC_PTR(tf_get_pinst);
extern DECL_FUNC_PTR(LocalHostAddr);
extern DECL_FUNC_PTR(tkt_string);
extern DECL_FUNC_PTR(krb_set_tkt_string);
extern DECL_FUNC_PTR(initialize_krb_error_func);
extern DECL_FUNC_PTR(initialize_kadm_error_table);
extern DECL_FUNC_PTR(dest_tkt);
extern DECL_FUNC_PTR(lsh_LoadKrb4LeashErrorTables); // XXX
extern DECL_FUNC_PTR(krb_in_tkt);
extern DECL_FUNC_PTR(krb_save_credentials);
extern DECL_FUNC_PTR(krb_get_krbconf2);
extern DECL_FUNC_PTR(krb_get_krbrealm2);
extern DECL_FUNC_PTR(krb_life_to_time);
#endif

// krb5 functions
extern DECL_FUNC_PTR(krb5_change_password);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_alloc);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_free);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_init);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_set_tkt_life);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_set_renew_life);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_set_forwardable);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_set_proxiable);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_set_renew_life);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_set_address_list);
extern DECL_FUNC_PTR(krb5_get_init_creds_opt_set_out_ccache);
extern DECL_FUNC_PTR(krb5_get_init_creds_password);
extern DECL_FUNC_PTR(krb5_build_principal_ext);
extern DECL_FUNC_PTR(krb5_cc_get_name);
extern DECL_FUNC_PTR(krb5_cc_resolve);
extern DECL_FUNC_PTR(krb5_cc_default);
extern DECL_FUNC_PTR(krb5_cc_default_name);
extern DECL_FUNC_PTR(krb5_cc_set_default_name);
extern DECL_FUNC_PTR(krb5_cc_initialize);
extern DECL_FUNC_PTR(krb5_cc_destroy);
extern DECL_FUNC_PTR(krb5_cc_close);
extern DECL_FUNC_PTR(krb5_cc_copy_creds);
extern DECL_FUNC_PTR(krb5_cc_store_cred);
// extern DECL_FUNC_PTR(krb5_cc_retrieve_cred);
extern DECL_FUNC_PTR(krb5_cc_get_principal);
extern DECL_FUNC_PTR(krb5_cc_start_seq_get);
extern DECL_FUNC_PTR(krb5_cc_next_cred);
extern DECL_FUNC_PTR(krb5_cc_end_seq_get);
// extern DECL_FUNC_PTR(krb5_cc_remove_cred);
extern DECL_FUNC_PTR(krb5_cc_set_flags);
// extern DECL_FUNC_PTR(krb5_cc_get_type);
extern DECL_FUNC_PTR(krb5_cc_get_full_name);
extern DECL_FUNC_PTR(krb5_free_context);
extern DECL_FUNC_PTR(krb5_free_cred_contents);
extern DECL_FUNC_PTR(krb5_free_principal);
extern DECL_FUNC_PTR(krb5_free_string);
extern DECL_FUNC_PTR(krb5_get_in_tkt_with_password);
extern DECL_FUNC_PTR(krb5_init_context);
extern DECL_FUNC_PTR(krb5_parse_name);
extern DECL_FUNC_PTR(krb5_timeofday);
extern DECL_FUNC_PTR(krb5_timestamp_to_sfstring);
extern DECL_FUNC_PTR(krb5_unparse_name);
extern DECL_FUNC_PTR(krb5_get_credentials);
extern DECL_FUNC_PTR(krb5_mk_req);
extern DECL_FUNC_PTR(krb5_sname_to_principal);
extern DECL_FUNC_PTR(krb5_get_credentials_renew);
extern DECL_FUNC_PTR(krb5_free_data);
extern DECL_FUNC_PTR(krb5_free_data_contents);
// extern DECL_FUNC_PTR(krb5_get_realm_domain);
extern DECL_FUNC_PTR(krb5_free_unparsed_name);
extern DECL_FUNC_PTR(krb5_os_localaddr);
extern DECL_FUNC_PTR(krb5_copy_keyblock_contents);
extern DECL_FUNC_PTR(krb5_copy_data);
extern DECL_FUNC_PTR(krb5_free_creds);
extern DECL_FUNC_PTR(krb5_build_principal);
extern DECL_FUNC_PTR(krb5_get_renewed_creds);
extern DECL_FUNC_PTR(krb5_free_addresses);
extern DECL_FUNC_PTR(krb5_get_default_config_files);
extern DECL_FUNC_PTR(krb5_free_config_files);
extern DECL_FUNC_PTR(krb5_get_default_realm);
extern DECL_FUNC_PTR(krb5_free_ticket);
extern DECL_FUNC_PTR(krb5_decode_ticket);
extern DECL_FUNC_PTR(krb5_get_host_realm);
extern DECL_FUNC_PTR(krb5_free_host_realm);
extern DECL_FUNC_PTR(krb5_c_random_make_octets);
extern DECL_FUNC_PTR(krb5_free_default_realm);
extern DECL_FUNC_PTR(krb5_principal_compare);
extern DECL_FUNC_PTR(krb5_string_to_deltat);
extern DECL_FUNC_PTR(krb5_is_config_principal);
extern DECL_FUNC_PTR(krb5_cccol_cursor_new);
extern DECL_FUNC_PTR(krb5_cccol_cursor_next);
extern DECL_FUNC_PTR(krb5_cccol_cursor_free);
extern DECL_FUNC_PTR(krb5_cc_cache_match);
extern DECL_FUNC_PTR(krb5_cc_get_type);
extern DECL_FUNC_PTR(krb5_cc_new_unique);
extern DECL_FUNC_PTR(krb5_cc_support_switch);
extern DECL_FUNC_PTR(krb5_cc_switch);
extern DECL_FUNC_PTR(krb5int_cc_user_set_default_name);

#ifndef NO_KRB4
// Krb524 functions
extern DECL_FUNC_PTR(krb524_init_ets);
extern DECL_FUNC_PTR(krb524_convert_creds_kdc);
#endif

// ComErr functions
extern DECL_FUNC_PTR(com_err);
extern DECL_FUNC_PTR(error_message);

// Profile functions
extern DECL_FUNC_PTR(profile_init);
extern DECL_FUNC_PTR(profile_release);
extern DECL_FUNC_PTR(profile_get_subsection_names);
extern DECL_FUNC_PTR(profile_free_list);
extern DECL_FUNC_PTR(profile_get_string);
extern DECL_FUNC_PTR(profile_release_string);
extern DECL_FUNC_PTR(profile_get_integer);

// Service functions

extern DECL_FUNC_PTR(OpenSCManagerA);
extern DECL_FUNC_PTR(OpenServiceA);
extern DECL_FUNC_PTR(QueryServiceStatus);
extern DECL_FUNC_PTR(CloseServiceHandle);
extern DECL_FUNC_PTR(LsaNtStatusToWinError);

// LSA Functions

extern DECL_FUNC_PTR(LsaConnectUntrusted);
extern DECL_FUNC_PTR(LsaLookupAuthenticationPackage);
extern DECL_FUNC_PTR(LsaCallAuthenticationPackage);
extern DECL_FUNC_PTR(LsaFreeReturnBuffer);
extern DECL_FUNC_PTR(LsaGetLogonSessionData);

#ifdef __cplusplus
}
#endif

#endif /* _LEASHDLL_H_ */
