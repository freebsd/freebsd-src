//*****************************************************************************
// File:	lgobals.h
// By:		Arthur David Leather
// Created:	12/02/98
// Copyright:	@1998 Massachusetts Institute of Technology - All rights
//              reserved.
// Description:	H file for lgobals.cpp. Contains global variables and helper
//		functions
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 02/02/98	ADL	Original
//*****************************************************************************

#if !defined LEASHGLOBALS_H
#define LEASHGLOBALS_H

#include <tlhelp32.h>
#include <loadfuncs-com_err.h>
#include <loadfuncs-krb5.h>
////#include <loadfuncs-krb.h>
#include <loadfuncs-profile.h>
#include <loadfuncs-leash.h>
#include <krb5.h>

// toolhelp functions
TYPEDEF_FUNC(
    HANDLE,
    WINAPI,
    CreateToolhelp32Snapshot,
    (DWORD, DWORD)
    );
TYPEDEF_FUNC(
    BOOL,
    WINAPI,
    Module32First,
    (HANDLE, LPMODULEENTRY32)
    );
TYPEDEF_FUNC(
    BOOL,
    WINAPI,
    Module32Next,
    (HANDLE, LPMODULEENTRY32)
    );

// psapi functions
TYPEDEF_FUNC(
    DWORD,
    WINAPI,
    GetModuleFileNameExA,
    (HANDLE, HMODULE, LPSTR, DWORD)
    );
TYPEDEF_FUNC(
    BOOL,
    WINAPI,
    EnumProcessModules,
    (HANDLE, HMODULE*, DWORD, LPDWORD)
    );

#define pGetModuleFileNameEx pGetModuleFileNameExA

// leash functions
TYPEDEF_FUNC(
    long,
    WINAPIV,
    not_an_API_LeashKRB4GetTickets,
    (TICKETINFO *, TicketList **)
    );
TYPEDEF_FUNC(
    long,
    WINAPIV,
    not_an_API_LeashAFSGetToken,
    (TICKETINFO *, TicketList **, char *)
    );
TYPEDEF_FUNC(
    long,
    WINAPIV,
    not_an_API_LeashGetTimeServerName,
    (char *, const char*)
    );

extern DECL_FUNC_PTR(not_an_API_LeashKRB4GetTickets);
extern DECL_FUNC_PTR(not_an_API_LeashAFSGetToken);
extern DECL_FUNC_PTR(not_an_API_LeashGetTimeServerName);
extern DECL_FUNC_PTR(Leash_kdestroy);
extern DECL_FUNC_PTR(Leash_changepwd_dlg);
extern DECL_FUNC_PTR(Leash_changepwd_dlg_ex);
extern DECL_FUNC_PTR(Leash_kinit_dlg);
extern DECL_FUNC_PTR(Leash_kinit_dlg_ex);
extern DECL_FUNC_PTR(Leash_timesync);
extern DECL_FUNC_PTR(Leash_get_default_lifetime);
extern DECL_FUNC_PTR(Leash_set_default_lifetime);
extern DECL_FUNC_PTR(Leash_get_default_forwardable);
extern DECL_FUNC_PTR(Leash_set_default_forwardable);
extern DECL_FUNC_PTR(Leash_get_default_renew_till);
extern DECL_FUNC_PTR(Leash_set_default_renew_till);
extern DECL_FUNC_PTR(Leash_get_default_noaddresses);
extern DECL_FUNC_PTR(Leash_set_default_noaddresses);
extern DECL_FUNC_PTR(Leash_get_default_proxiable);
extern DECL_FUNC_PTR(Leash_set_default_proxiable);
extern DECL_FUNC_PTR(Leash_get_default_publicip);
extern DECL_FUNC_PTR(Leash_set_default_publicip);
extern DECL_FUNC_PTR(Leash_get_default_use_krb4);
extern DECL_FUNC_PTR(Leash_set_default_use_krb4);
extern DECL_FUNC_PTR(Leash_get_default_life_min);
extern DECL_FUNC_PTR(Leash_set_default_life_min);
extern DECL_FUNC_PTR(Leash_get_default_life_max);
extern DECL_FUNC_PTR(Leash_set_default_life_max);
extern DECL_FUNC_PTR(Leash_get_default_renew_min);
extern DECL_FUNC_PTR(Leash_set_default_renew_min);
extern DECL_FUNC_PTR(Leash_get_default_renew_max);
extern DECL_FUNC_PTR(Leash_set_default_renew_max);
extern DECL_FUNC_PTR(Leash_get_default_renewable);
extern DECL_FUNC_PTR(Leash_set_default_renewable);
extern DECL_FUNC_PTR(Leash_get_lock_file_locations);
extern DECL_FUNC_PTR(Leash_set_lock_file_locations);
extern DECL_FUNC_PTR(Leash_get_default_uppercaserealm);
extern DECL_FUNC_PTR(Leash_set_default_uppercaserealm);
extern DECL_FUNC_PTR(Leash_get_default_mslsa_import);
extern DECL_FUNC_PTR(Leash_set_default_mslsa_import);
extern DECL_FUNC_PTR(Leash_get_default_preserve_kinit_settings);
extern DECL_FUNC_PTR(Leash_set_default_preserve_kinit_settings);
extern DECL_FUNC_PTR(Leash_import);
extern DECL_FUNC_PTR(Leash_importable);
extern DECL_FUNC_PTR(Leash_renew);
extern DECL_FUNC_PTR(Leash_reset_defaults);

////Do we still need this one?
#define pLeashKRB4GetTickets     pnot_an_API_LeashKRB4GetTickets
#define pLeashAFSGetToken        pnot_an_API_LeashAFSGetToken
#define pLeashGetTimeServerName  pnot_an_API_LeashGetTimeServerName

// psapi functions
extern DECL_FUNC_PTR(GetModuleFileNameExA);
extern DECL_FUNC_PTR(EnumProcessModules);

// toolhelp functions
extern DECL_FUNC_PTR(CreateToolhelp32Snapshot);
extern DECL_FUNC_PTR(Module32First);
extern DECL_FUNC_PTR(Module32Next);

// com_err functions
extern DECL_FUNC_PTR(error_message);

// krb5 functions
extern DECL_FUNC_PTR(krb5_cc_default_name);
extern DECL_FUNC_PTR(krb5_cc_set_default_name);
extern DECL_FUNC_PTR(krb5_get_default_config_files);
extern DECL_FUNC_PTR(krb5_free_config_files);
extern DECL_FUNC_PTR(krb5_free_context);
extern DECL_FUNC_PTR(krb5_get_default_realm);
extern DECL_FUNC_PTR(krb5_free_default_realm);
extern DECL_FUNC_PTR(krb5_cc_get_principal);
extern DECL_FUNC_PTR(krb5_build_principal);
extern DECL_FUNC_PTR(krb5_c_random_make_octets);
extern DECL_FUNC_PTR(krb5_get_init_creds_password);
extern DECL_FUNC_PTR(krb5_free_cred_contents);
extern DECL_FUNC_PTR(krb5_cc_resolve);
extern DECL_FUNC_PTR(krb5_unparse_name);
extern DECL_FUNC_PTR(krb5_free_unparsed_name);
extern DECL_FUNC_PTR(krb5_free_principal);
extern DECL_FUNC_PTR(krb5_cc_close);
extern DECL_FUNC_PTR(krb5_cc_default);
extern DECL_FUNC_PTR(krb5_cc_destroy);
extern DECL_FUNC_PTR(krb5_cc_set_flags);
extern DECL_FUNC_PTR(krb5_cc_get_name);
extern DECL_FUNC_PTR(krb5_cc_start_seq_get);
extern DECL_FUNC_PTR(krb5_cc_end_seq_get);
extern DECL_FUNC_PTR(krb5_cc_next_cred);
extern DECL_FUNC_PTR(krb5_cccol_cursor_new);
extern DECL_FUNC_PTR(krb5_cccol_cursor_next);
extern DECL_FUNC_PTR(krb5_cccol_cursor_free);
extern DECL_FUNC_PTR(krb5_decode_ticket);
extern DECL_FUNC_PTR(krb5_free_ticket);
extern DECL_FUNC_PTR(krb5_init_context);
extern DECL_FUNC_PTR(krb5_is_config_principal);
extern DECL_FUNC_PTR(krb5_cc_switch);
extern DECL_FUNC_PTR(krb5_build_principal_ext);
extern DECL_FUNC_PTR(krb5_get_renewed_creds);
extern DECL_FUNC_PTR(krb5_cc_initialize);
extern DECL_FUNC_PTR(krb5_cc_store_cred);
extern DECL_FUNC_PTR(krb5_cc_get_full_name);
extern DECL_FUNC_PTR(krb5_free_string);
extern DECL_FUNC_PTR(krb5_enctype_to_name);
extern DECL_FUNC_PTR(krb5_cc_get_type);
extern DECL_FUNC_PTR(krb5int_cc_user_set_default_name);
// extern DECL_FUNC_PTR(krb5_get_host_realm);

// profile functions
extern DECL_FUNC_PTR(profile_release);
extern DECL_FUNC_PTR(profile_init);
extern DECL_FUNC_PTR(profile_flush);
extern DECL_FUNC_PTR(profile_rename_section);
extern DECL_FUNC_PTR(profile_update_relation);
extern DECL_FUNC_PTR(profile_clear_relation);
extern DECL_FUNC_PTR(profile_add_relation);
extern DECL_FUNC_PTR(profile_get_relation_names);
extern DECL_FUNC_PTR(profile_get_subsection_names);
extern DECL_FUNC_PTR(profile_get_values);
extern DECL_FUNC_PTR(profile_free_list);
extern DECL_FUNC_PTR(profile_abandon);
extern DECL_FUNC_PTR(profile_get_string);
extern DECL_FUNC_PTR(profile_release_string);

#define SKIP_MINSIZE  0
#define LEFT_SIDE     1
#define RIGHT_SIDE    2
#define TOP_SIDE      3
#define RESET_MINSIZE 4
#define BOTTOM_SIDE   6

#define ADMIN_SERVER "admin_server"

#define ON  1
#define OFF 0
#define TRUE_FLAG		1
#define FALSE_FLAG		0
#ifdef _WIN64
#define LEASHDLL "leashw64.dll"
#define KERB5DLL "krb5_64.dll"
#define KERB5_PPROFILE_DLL "xpprof64.dll"
#else
#define LEASHDLL "leashw32.dll"
#define KERB5DLL "krb5_32.dll"
#define KERB5_PPROFILE_DLL "xpprof32.dll"
#endif
#define SECUR32DLL "secur32.dll"
#define KRB_FILE		"KRB.CON"
#define KRBREALM_FILE	"KRBREALM.CON"
#define TICKET_FILE		"TICKET.KRB"

#define LEASH_HELP_FILE "leash.chm"

extern int  config_boolean_to_int(const char *);
extern BOOL SetRegistryVariable(const CString& regVariable,
                                const CString& regValue,
                                const char* regSubKey = "Software\\MIT\\Leash32\\Settings");
extern VOID LeashErrorBox(LPCSTR errorMsg, LPCSTR insertedString,
                          LPCSTR errorFlag = "Error");

// Get ticket info for the default ccache only
extern void LeashKRB5ListDefaultTickets(TICKETINFO *ticketinfo);
// clean up ticket info
extern void LeashKRB5FreeTicketInfo(TICKETINFO *ticketinfo);

// Allocate TICKETINFO for each ccache that contain tickets
extern void LeashKRB5ListAllTickets(TICKETINFO **ticketinfolist);
// clean up ticket info list
extern void LeashKRB5FreeTickets(TICKETINFO **ticketinfolist);



class Directory
{
    CHAR m_savCurPath[MAX_PATH];
    CString m_pathToValidate;

public:
    Directory(LPCSTR pathToValidate);
    virtual ~Directory();

    BOOL IsValidDirectory();
    BOOL IsValidFile();
};

class TicketInfoWrapper {
  public:
    HANDLE     lockObj;
    TICKETINFO Krb5;
    TICKETINFO Afs;
};
extern TicketInfoWrapper ticketinfo;

#endif
