#ifndef _LEASHDLL_H_
#define _LEASHDLL_H_

#include <com_err.h>
#ifdef __cplusplus
extern "C" {
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

extern HINSTANCE hKrb5;
extern HINSTANCE hProfile;

#define TIMEHOST "TIMEHOST"

#define LEASH_DEBUG_CLASS_GENERIC   0

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
#include <loadfuncs-lsa.h>

#include <errno.h>

// service definitions
typedef SC_HANDLE (WINAPI *FP_OpenSCManagerA)(char *, char *, DWORD);
typedef SC_HANDLE (WINAPI *FP_OpenServiceA)(SC_HANDLE, char *, DWORD);
typedef BOOL (WINAPI *FP_QueryServiceStatus)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL (WINAPI *FP_CloseServiceHandle)(SC_HANDLE);

//////////////////////////////////////////////////////////////////////////////

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
