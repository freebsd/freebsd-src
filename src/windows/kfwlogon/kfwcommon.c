/*
Copyright 2005,2006 by the Massachusetts Institute of Technology
Copyright 2007 by Secure Endpoints Inc.

All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the Massachusetts
Institute of Technology (M.I.T.) not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

#include "kfwlogon.h"
#include <windows.h>
#include <Aclapi.h>
#include <userenv.h>
#include <Sddl.h>

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <winsock2.h>
#include <lm.h>
#include <nb30.h>

#include <errno.h>
#include <malloc.h>


/* Function Pointer Declarations for Delayed Loading */
// CCAPI
DECL_FUNC_PTR(cc_initialize);
DECL_FUNC_PTR(cc_shutdown);
DECL_FUNC_PTR(cc_get_NC_info);
DECL_FUNC_PTR(cc_free_NC_info);

// leash functions
DECL_FUNC_PTR(Leash_get_default_lifetime);
DECL_FUNC_PTR(Leash_get_default_forwardable);
DECL_FUNC_PTR(Leash_get_default_renew_till);
DECL_FUNC_PTR(Leash_get_default_noaddresses);
DECL_FUNC_PTR(Leash_get_default_proxiable);
DECL_FUNC_PTR(Leash_get_default_publicip);
DECL_FUNC_PTR(Leash_get_default_use_krb4);
DECL_FUNC_PTR(Leash_get_default_life_min);
DECL_FUNC_PTR(Leash_get_default_life_max);
DECL_FUNC_PTR(Leash_get_default_renew_min);
DECL_FUNC_PTR(Leash_get_default_renew_max);
DECL_FUNC_PTR(Leash_get_default_renewable);
DECL_FUNC_PTR(Leash_get_default_mslsa_import);

// krb5 functions
DECL_FUNC_PTR(krb5_change_password);
DECL_FUNC_PTR(krb5_get_init_creds_opt_init);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_tkt_life);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_renew_life);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_forwardable);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_proxiable);
DECL_FUNC_PTR(krb5_get_init_creds_opt_set_address_list);
DECL_FUNC_PTR(krb5_get_init_creds_password);
DECL_FUNC_PTR(krb5_build_principal_ext);
DECL_FUNC_PTR(krb5_cc_get_name);
DECL_FUNC_PTR(krb5_cc_resolve);
DECL_FUNC_PTR(krb5_cc_default);
DECL_FUNC_PTR(krb5_cc_default_name);
DECL_FUNC_PTR(krb5_cc_set_default_name);
DECL_FUNC_PTR(krb5_cc_initialize);
DECL_FUNC_PTR(krb5_cc_destroy);
DECL_FUNC_PTR(krb5_cc_close);
DECL_FUNC_PTR(krb5_cc_store_cred);
DECL_FUNC_PTR(krb5_cc_copy_creds);
DECL_FUNC_PTR(krb5_cc_retrieve_cred);
DECL_FUNC_PTR(krb5_cc_get_principal);
DECL_FUNC_PTR(krb5_cc_start_seq_get);
DECL_FUNC_PTR(krb5_cc_next_cred);
DECL_FUNC_PTR(krb5_cc_end_seq_get);
DECL_FUNC_PTR(krb5_cc_remove_cred);
DECL_FUNC_PTR(krb5_cc_set_flags);
DECL_FUNC_PTR(krb5_cc_get_type);
DECL_FUNC_PTR(krb5_free_context);
DECL_FUNC_PTR(krb5_free_cred_contents);
DECL_FUNC_PTR(krb5_free_principal);
DECL_FUNC_PTR(krb5_get_in_tkt_with_password);
DECL_FUNC_PTR(krb5_init_context);
DECL_FUNC_PTR(krb5_parse_name);
DECL_FUNC_PTR(krb5_timeofday);
DECL_FUNC_PTR(krb5_timestamp_to_sfstring);
DECL_FUNC_PTR(krb5_unparse_name);
DECL_FUNC_PTR(krb5_get_credentials);
DECL_FUNC_PTR(krb5_mk_req);
DECL_FUNC_PTR(krb5_sname_to_principal);
DECL_FUNC_PTR(krb5_get_credentials_renew);
DECL_FUNC_PTR(krb5_free_data);
DECL_FUNC_PTR(krb5_free_data_contents);
DECL_FUNC_PTR(krb5_free_unparsed_name);
DECL_FUNC_PTR(krb5_os_localaddr);
DECL_FUNC_PTR(krb5_copy_keyblock_contents);
DECL_FUNC_PTR(krb5_copy_data);
DECL_FUNC_PTR(krb5_free_creds);
DECL_FUNC_PTR(krb5_build_principal);
DECL_FUNC_PTR(krb5_get_renewed_creds);
DECL_FUNC_PTR(krb5_get_default_config_files);
DECL_FUNC_PTR(krb5_free_config_files);
DECL_FUNC_PTR(krb5_get_default_realm);
DECL_FUNC_PTR(krb5_free_default_realm);
DECL_FUNC_PTR(krb5_free_ticket);
DECL_FUNC_PTR(krb5_decode_ticket);
DECL_FUNC_PTR(krb5_get_host_realm);
DECL_FUNC_PTR(krb5_free_host_realm);
DECL_FUNC_PTR(krb5_free_addresses);
DECL_FUNC_PTR(krb5_c_random_make_octets);

// ComErr functions
DECL_FUNC_PTR(com_err);
DECL_FUNC_PTR(error_message);

// Profile functions
DECL_FUNC_PTR(profile_init);
DECL_FUNC_PTR(profile_release);
DECL_FUNC_PTR(profile_get_subsection_names);
DECL_FUNC_PTR(profile_free_list);
DECL_FUNC_PTR(profile_get_string);
DECL_FUNC_PTR(profile_release_string);

// Service functions
DECL_FUNC_PTR(OpenSCManagerA);
DECL_FUNC_PTR(OpenServiceA);
DECL_FUNC_PTR(QueryServiceStatus);
DECL_FUNC_PTR(CloseServiceHandle);
DECL_FUNC_PTR(LsaNtStatusToWinError);

// LSA Functions
DECL_FUNC_PTR(LsaConnectUntrusted);
DECL_FUNC_PTR(LsaLookupAuthenticationPackage);
DECL_FUNC_PTR(LsaCallAuthenticationPackage);
DECL_FUNC_PTR(LsaFreeReturnBuffer);
DECL_FUNC_PTR(LsaGetLogonSessionData);

// CCAPI
FUNC_INFO ccapi_fi[] = {
    MAKE_FUNC_INFO(cc_initialize),
    MAKE_FUNC_INFO(cc_shutdown),
    MAKE_FUNC_INFO(cc_get_NC_info),
    MAKE_FUNC_INFO(cc_free_NC_info),
    END_FUNC_INFO
};

FUNC_INFO leash_fi[] = {
    MAKE_FUNC_INFO(Leash_get_default_lifetime),
    MAKE_FUNC_INFO(Leash_get_default_renew_till),
    MAKE_FUNC_INFO(Leash_get_default_forwardable),
    MAKE_FUNC_INFO(Leash_get_default_noaddresses),
    MAKE_FUNC_INFO(Leash_get_default_proxiable),
    MAKE_FUNC_INFO(Leash_get_default_publicip),
    MAKE_FUNC_INFO(Leash_get_default_use_krb4),
    MAKE_FUNC_INFO(Leash_get_default_life_min),
    MAKE_FUNC_INFO(Leash_get_default_life_max),
    MAKE_FUNC_INFO(Leash_get_default_renew_min),
    MAKE_FUNC_INFO(Leash_get_default_renew_max),
    MAKE_FUNC_INFO(Leash_get_default_renewable),
    END_FUNC_INFO
};

FUNC_INFO leash_opt_fi[] = {
    MAKE_FUNC_INFO(Leash_get_default_mslsa_import),
    END_FUNC_INFO
};

FUNC_INFO k5_fi[] = {
    MAKE_FUNC_INFO(krb5_change_password),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_init),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_tkt_life),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_renew_life),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_forwardable),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_proxiable),
    MAKE_FUNC_INFO(krb5_get_init_creds_opt_set_address_list),
    MAKE_FUNC_INFO(krb5_get_init_creds_password),
    MAKE_FUNC_INFO(krb5_build_principal_ext),
    MAKE_FUNC_INFO(krb5_cc_get_name),
    MAKE_FUNC_INFO(krb5_cc_resolve),
    MAKE_FUNC_INFO(krb5_cc_default),
    MAKE_FUNC_INFO(krb5_cc_default_name),
    MAKE_FUNC_INFO(krb5_cc_set_default_name),
    MAKE_FUNC_INFO(krb5_cc_initialize),
    MAKE_FUNC_INFO(krb5_cc_destroy),
    MAKE_FUNC_INFO(krb5_cc_close),
    MAKE_FUNC_INFO(krb5_cc_copy_creds),
    MAKE_FUNC_INFO(krb5_cc_store_cred),
    MAKE_FUNC_INFO(krb5_cc_retrieve_cred),
    MAKE_FUNC_INFO(krb5_cc_get_principal),
    MAKE_FUNC_INFO(krb5_cc_start_seq_get),
    MAKE_FUNC_INFO(krb5_cc_next_cred),
    MAKE_FUNC_INFO(krb5_cc_end_seq_get),
    MAKE_FUNC_INFO(krb5_cc_remove_cred),
    MAKE_FUNC_INFO(krb5_cc_set_flags),
    MAKE_FUNC_INFO(krb5_cc_get_type),
    MAKE_FUNC_INFO(krb5_free_context),
    MAKE_FUNC_INFO(krb5_free_cred_contents),
    MAKE_FUNC_INFO(krb5_free_principal),
    MAKE_FUNC_INFO(krb5_get_in_tkt_with_password),
    MAKE_FUNC_INFO(krb5_init_context),
    MAKE_FUNC_INFO(krb5_parse_name),
    MAKE_FUNC_INFO(krb5_timeofday),
    MAKE_FUNC_INFO(krb5_timestamp_to_sfstring),
    MAKE_FUNC_INFO(krb5_unparse_name),
    MAKE_FUNC_INFO(krb5_get_credentials),
    MAKE_FUNC_INFO(krb5_mk_req),
    MAKE_FUNC_INFO(krb5_sname_to_principal),
    MAKE_FUNC_INFO(krb5_get_credentials_renew),
    MAKE_FUNC_INFO(krb5_free_data),
    MAKE_FUNC_INFO(krb5_free_data_contents),
    MAKE_FUNC_INFO(krb5_free_unparsed_name),
    MAKE_FUNC_INFO(krb5_os_localaddr),
    MAKE_FUNC_INFO(krb5_copy_keyblock_contents),
    MAKE_FUNC_INFO(krb5_copy_data),
    MAKE_FUNC_INFO(krb5_free_creds),
    MAKE_FUNC_INFO(krb5_build_principal),
    MAKE_FUNC_INFO(krb5_get_renewed_creds),
    MAKE_FUNC_INFO(krb5_free_addresses),
    MAKE_FUNC_INFO(krb5_get_default_config_files),
    MAKE_FUNC_INFO(krb5_free_config_files),
    MAKE_FUNC_INFO(krb5_get_default_realm),
    MAKE_FUNC_INFO(krb5_free_default_realm),
    MAKE_FUNC_INFO(krb5_free_ticket),
    MAKE_FUNC_INFO(krb5_decode_ticket),
    MAKE_FUNC_INFO(krb5_get_host_realm),
    MAKE_FUNC_INFO(krb5_free_host_realm),
    MAKE_FUNC_INFO(krb5_free_addresses),
    MAKE_FUNC_INFO(krb5_c_random_make_octets),
    END_FUNC_INFO
};

FUNC_INFO profile_fi[] = {
        MAKE_FUNC_INFO(profile_init),
        MAKE_FUNC_INFO(profile_release),
        MAKE_FUNC_INFO(profile_get_subsection_names),
        MAKE_FUNC_INFO(profile_free_list),
        MAKE_FUNC_INFO(profile_get_string),
        MAKE_FUNC_INFO(profile_release_string),
        END_FUNC_INFO
};

FUNC_INFO ce_fi[] = {
    MAKE_FUNC_INFO(com_err),
    MAKE_FUNC_INFO(error_message),
    END_FUNC_INFO
};

FUNC_INFO service_fi[] = {
    MAKE_FUNC_INFO(OpenSCManagerA),
    MAKE_FUNC_INFO(OpenServiceA),
    MAKE_FUNC_INFO(QueryServiceStatus),
    MAKE_FUNC_INFO(CloseServiceHandle),
    MAKE_FUNC_INFO(LsaNtStatusToWinError),
    END_FUNC_INFO
};

FUNC_INFO lsa_fi[] = {
    MAKE_FUNC_INFO(LsaConnectUntrusted),
    MAKE_FUNC_INFO(LsaLookupAuthenticationPackage),
    MAKE_FUNC_INFO(LsaCallAuthenticationPackage),
    MAKE_FUNC_INFO(LsaFreeReturnBuffer),
    MAKE_FUNC_INFO(LsaGetLogonSessionData),
    END_FUNC_INFO
};

/* Static Declarations */
static int       inited = 0;
static HINSTANCE hKrb5 = 0;
static HINSTANCE hKrb524 = 0;
static HINSTANCE hSecur32 = 0;
static HINSTANCE hAdvApi32 = 0;
static HINSTANCE hComErr = 0;
static HINSTANCE hService = 0;
static HINSTANCE hProfile = 0;
static HINSTANCE hLeash = 0;
static HINSTANCE hLeashOpt = 0;
static HINSTANCE hCCAPI = 0;

static DWORD TraceOption = 0;
static HANDLE hDLL;

BOOL IsDebugLogging(void)
{
    DWORD LSPsize;
    HKEY NPKey;
    DWORD dwDebug = FALSE;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		     "System\\CurrentControlSet\\Services\\MIT Kerberos\\NetworkProvider",
		     0, KEY_QUERY_VALUE, &NPKey) == ERROR_SUCCESS)
    {
	LSPsize=sizeof(dwDebug);
	if (RegQueryValueEx(NPKey, "Debug", NULL, NULL, (LPBYTE)&dwDebug, &LSPsize) != ERROR_SUCCESS)
	{
	    dwDebug = FALSE;
	}
	RegCloseKey (NPKey);
    }

    return(dwDebug ? TRUE : FALSE);
}

void DebugEvent0(char *a)
{
    HANDLE h; char *ptbuf[1];

    if (IsDebugLogging()) {
	h = RegisterEventSource(NULL, KFW_LOGON_EVENT_NAME);
	if (h) {
            ptbuf[0] = a;
            ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (const char **)ptbuf, NULL);
            DeregisterEventSource(h);
        }
    }
}

#define MAXBUF_ 512
void DebugEvent(char *b,...)
{
    HANDLE h; char *ptbuf[1],buf[MAXBUF_+1];
    va_list marker;

    if (IsDebugLogging()) {
	h = RegisterEventSource(NULL, KFW_LOGON_EVENT_NAME);
        if (h) {
            va_start(marker,b);
            StringCbVPrintf(buf, MAXBUF_+1,b,marker);
            buf[MAXBUF_] = '\0';
            ptbuf[0] = buf;
            ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (const char **)ptbuf, NULL);
            DeregisterEventSource(h);
            va_end(marker);
        }
    }
}

static HANDLE hInitMutex = NULL;
static BOOL bInit = FALSE;

/* KFW_initialize cannot be called from DllEntryPoint */
void
KFW_initialize(void)
{
    static int inited = 0;

    if ( !inited ) {
        char mutexName[MAX_PATH];
        HANDLE hMutex = NULL;

        sprintf(mutexName, "AFS KFW Init pid=%d", getpid());

        hMutex = CreateMutex( NULL, TRUE, mutexName );
        if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
            if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
                return;
            }
        }
        if ( !inited ) {
            inited = 1;
            LoadFuncs(KRB5_DLL, k5_fi, &hKrb5, 0, 1, 0, 0);
            LoadFuncs(COMERR_DLL, ce_fi, &hComErr, 0, 0, 1, 0);
            LoadFuncs(SERVICE_DLL, service_fi, &hService, 0, 1, 0, 0);
            LoadFuncs(SECUR32_DLL, lsa_fi, &hSecur32, 0, 1, 1, 1);
            LoadFuncs(PROFILE_DLL, profile_fi, &hProfile, 0, 1, 0, 0);
            LoadFuncs(LEASH_DLL, leash_fi, &hLeash, 0, 1, 0, 0);
            LoadFuncs(CCAPI_DLL, ccapi_fi, &hCCAPI, 0, 1, 0, 0);
            LoadFuncs(LEASH_DLL, leash_opt_fi, &hLeashOpt, 0, 1, 0, 0);
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
}

void
KFW_cleanup(void)
{
    if (hLeashOpt)
        FreeLibrary(hLeashOpt);
    if (hCCAPI)
        FreeLibrary(hCCAPI);
    if (hLeash)
        FreeLibrary(hLeash);
    if (hKrb524)
        FreeLibrary(hKrb524);
    if (hSecur32)
        FreeLibrary(hSecur32);
    if (hService)
        FreeLibrary(hService);
    if (hComErr)
        FreeLibrary(hComErr);
    if (hProfile)
        FreeLibrary(hProfile);
    if (hKrb5)
        FreeLibrary(hKrb5);
}


int
KFW_is_available(void)
{
    KFW_initialize();
    if ( hKrb5 && hComErr && hService &&
#ifdef USE_MS2MIT
         hSecur32 &&
#endif /* USE_MS2MIT */
         hProfile && hLeash && hCCAPI )
        return TRUE;

    return FALSE;
}

/* Given a principal return an existing ccache or create one and return */
int
KFW_get_ccache(krb5_context alt_ctx, krb5_principal principal, krb5_ccache * cc)
{
    krb5_context ctx;
    char * pname = 0;
    char * ccname = 0;
    krb5_error_code code;

    if (!pkrb5_init_context)
        return 0;

    if ( alt_ctx ) {
        ctx = alt_ctx;
    } else {
        code = pkrb5_init_context(&ctx);
        if (code) goto cleanup;
    }

    if ( principal ) {
        code = pkrb5_unparse_name(ctx, principal, &pname);
        if (code) goto cleanup;

	ccname = (char *)malloc(strlen(pname) + 5);
	sprintf(ccname,"API:%s",pname);

	DebugEvent0(ccname);
	code = pkrb5_cc_resolve(ctx, ccname, cc);
    } else {
        code = pkrb5_cc_default(ctx, cc);
        if (code) goto cleanup;
    }

  cleanup:
    if (ccname)
        free(ccname);
    if (pname)
        pkrb5_free_unparsed_name(ctx,pname);
    if (ctx && (ctx != alt_ctx))
        pkrb5_free_context(ctx);
    return(code);
}


int
KFW_kinit( krb5_context alt_ctx,
            krb5_ccache  alt_cc,
            HWND hParent,
            char *principal_name,
            char *password,
            krb5_deltat lifetime,
            DWORD                       forwardable,
            DWORD                       proxiable,
            krb5_deltat                 renew_life,
            DWORD                       addressless,
            DWORD                       publicIP
            )
{
    krb5_error_code		        code = 0;
    krb5_context		        ctx = 0;
    krb5_ccache			        cc = 0;
    krb5_principal		        me = 0;
    char*                       name = 0;
    krb5_creds			        my_creds;
    krb5_get_init_creds_opt     options;
    krb5_address **             addrs = NULL;
    int                         i = 0, addr_count = 0;

    if (!pkrb5_init_context)
        return 0;

    pkrb5_get_init_creds_opt_init(&options);
    memset(&my_creds, 0, sizeof(my_creds));

    if (alt_ctx)
    {
        ctx = alt_ctx;
    }
    else
    {
        code = pkrb5_init_context(&ctx);
        if (code) goto cleanup;
    }

    if ( alt_cc ) {
        cc = alt_cc;
    } else {
        code = pkrb5_cc_default(ctx, &cc);
        if (code) goto cleanup;
    }

    code = pkrb5_parse_name(ctx, principal_name, &me);
    if (code)
	goto cleanup;

    code = pkrb5_unparse_name(ctx, me, &name);
    if (code)
	goto cleanup;

    if (lifetime == 0)
        lifetime = pLeash_get_default_lifetime();
    lifetime *= 60;

    if (renew_life > 0)
	renew_life *= 60;

    if (lifetime)
        pkrb5_get_init_creds_opt_set_tkt_life(&options, lifetime);
	pkrb5_get_init_creds_opt_set_forwardable(&options,
                                                 forwardable ? 1 : 0);
	pkrb5_get_init_creds_opt_set_proxiable(&options,
                                               proxiable ? 1 : 0);
	pkrb5_get_init_creds_opt_set_renew_life(&options,
                                               renew_life);
    if (addressless)
        pkrb5_get_init_creds_opt_set_address_list(&options,NULL);
    else {
	if (publicIP)
        {
            // we are going to add the public IP address specified by the user
            // to the list provided by the operating system
            krb5_address ** local_addrs=NULL;
            DWORD           netIPAddr;

            pkrb5_os_localaddr(ctx, &local_addrs);
            while ( local_addrs[i++] );
            addr_count = i + 1;

            addrs = (krb5_address **) malloc((addr_count+1) * sizeof(krb5_address *));
            if ( !addrs ) {
                pkrb5_free_addresses(ctx, local_addrs);
                goto cleanup;
            }
            memset(addrs, 0, sizeof(krb5_address *) * (addr_count+1));
            i = 0;
            while ( local_addrs[i] ) {
                addrs[i] = (krb5_address *)malloc(sizeof(krb5_address));
                if (addrs[i] == NULL) {
                    pkrb5_free_addresses(ctx, local_addrs);
                    goto cleanup;
                }

                addrs[i]->magic = local_addrs[i]->magic;
                addrs[i]->addrtype = local_addrs[i]->addrtype;
                addrs[i]->length = local_addrs[i]->length;
                addrs[i]->contents = (unsigned char *)malloc(addrs[i]->length);
                if (!addrs[i]->contents) {
                    pkrb5_free_addresses(ctx, local_addrs);
                    goto cleanup;
                }

                memcpy(addrs[i]->contents,local_addrs[i]->contents,
                        local_addrs[i]->length);        /* safe */
                i++;
            }
            pkrb5_free_addresses(ctx, local_addrs);

            addrs[i] = (krb5_address *)malloc(sizeof(krb5_address));
            if (addrs[i] == NULL)
                goto cleanup;

            addrs[i]->magic = KV5M_ADDRESS;
            addrs[i]->addrtype = AF_INET;
            addrs[i]->length = 4;
            addrs[i]->contents = (unsigned char *)malloc(addrs[i]->length);
            if (!addrs[i]->contents)
                goto cleanup;

            netIPAddr = htonl(publicIP);
            memcpy(addrs[i]->contents,&netIPAddr,4);

            pkrb5_get_init_creds_opt_set_address_list(&options,addrs);

        }
    }

    code = pkrb5_get_init_creds_password(ctx,
                                       &my_creds,
                                       me,
                                       password, // password
                                       NULL,     // no prompter
                                       hParent, // prompter data
                                       0, // start time
                                       0, // service name
                                       &options);
    if (code)
	goto cleanup;

    code = pkrb5_cc_initialize(ctx, cc, me);
    if (code)
	goto cleanup;

    code = pkrb5_cc_store_cred(ctx, cc, &my_creds);
    if (code)
	goto cleanup;

 cleanup:
    if ( addrs ) {
        for ( i=0;i<addr_count;i++ ) {
            if ( addrs[i] ) {
                if ( addrs[i]->contents )
                    free(addrs[i]->contents);
                free(addrs[i]);
            }
        }
    }
    if (my_creds.client == me)
	my_creds.client = 0;
    pkrb5_free_cred_contents(ctx, &my_creds);
    if (name)
        pkrb5_free_unparsed_name(ctx, name);
    if (me)
        pkrb5_free_principal(ctx, me);
    if (cc && (cc != alt_cc))
        pkrb5_cc_close(ctx, cc);
    if (ctx && (ctx != alt_ctx))
        pkrb5_free_context(ctx);
    return(code);
}


int
KFW_get_cred( char * username,
	      char * password,
	      int lifetime,
	      char ** reasonP )
{
    krb5_context ctx = 0;
    krb5_ccache cc = 0;
    char * realm = 0;
    krb5_principal principal = 0;
    char * pname = 0;
    krb5_error_code code;

    if (!pkrb5_init_context || !username || !password || !password[0])
        return 0;

    DebugEvent0(username);

    code = pkrb5_init_context(&ctx);
    if ( code ) goto cleanup;

    code = pkrb5_get_default_realm(ctx, &realm);

    if (realm) {
        pname = malloc(strlen(username) + strlen(realm) + 2);
	if (!pname)
	    goto cleanup;
	strcpy(pname, username);
	strcat(pname, "@");
	strcat(pname, realm);
    } else {
	goto cleanup;
    }

    DebugEvent0(realm);
    DebugEvent0(pname);

    code = pkrb5_parse_name(ctx, pname, &principal);
    if ( code ) goto cleanup;

    DebugEvent0("parsed name");
    code = KFW_get_ccache(ctx, principal, &cc);
    if ( code ) goto cleanup;

    DebugEvent0("got ccache");

    if ( lifetime == 0 )
        lifetime = pLeash_get_default_lifetime();

    DebugEvent0("got lifetime");

    code = KFW_kinit( ctx, cc, HWND_DESKTOP,
		      pname,
		      password,
		      lifetime,
		      pLeash_get_default_forwardable(),
		      pLeash_get_default_proxiable(),
		      pLeash_get_default_renewable() ? pLeash_get_default_renew_till() : 0,
		      pLeash_get_default_noaddresses(),
		      pLeash_get_default_publicip());
    DebugEvent0("kinit returned");
    if ( code ) goto cleanup;

  cleanup:
    if ( pname )
        free(pname);
    if ( realm )
	pkrb5_free_default_realm(ctx, realm);
    if ( cc )
        pkrb5_cc_close(ctx, cc);

    if ( code && reasonP ) {
        *reasonP = (char *)perror_message(code);
    }
    return(code);
}

int KFW_set_ccache_dacl(char *filename, HANDLE hUserToken)
{
    // SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_SID_AUTHORITY;
    PSID pSystemSID = NULL;
    DWORD SystemSIDlength = 0, UserSIDlength = 0;
    PACL ccacheACL = NULL;
    DWORD ccacheACLlength = 0;
    PTOKEN_USER pTokenUser = NULL;
    DWORD retLen;
    DWORD gle;
    int ret = 0;

    if (!filename) {
	DebugEvent0("KFW_set_ccache_dacl - invalid parms");
	return 1;
    }

    DebugEvent0("KFW_set_ccache_dacl");

    /* Get System SID */
    if (!ConvertStringSidToSid("S-1-5-18", &pSystemSID)) {
	DebugEvent("KFW_set_ccache_dacl - ConvertStringSidToSid GLE = 0x%x", GetLastError());
	ret = 1;
	goto cleanup;
    }

    /* Create ACL */
    SystemSIDlength = GetLengthSid(pSystemSID);
    ccacheACLlength = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE)
        + SystemSIDlength - sizeof(DWORD);

    if (hUserToken) {
	if (!GetTokenInformation(hUserToken, TokenUser, NULL, 0, &retLen))
	{
	    if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {
		pTokenUser = (PTOKEN_USER) LocalAlloc(LPTR, retLen);

		if (!GetTokenInformation(hUserToken, TokenUser, pTokenUser, retLen, &retLen))
		{
		    DebugEvent("GetTokenInformation failed: GLE = %lX", GetLastError());
		}
	    }
	}

	if (pTokenUser) {
	    UserSIDlength = GetLengthSid(pTokenUser->User.Sid);

	    ccacheACLlength += sizeof(ACCESS_ALLOWED_ACE) + UserSIDlength
		- sizeof(DWORD);
	}
    }

    ccacheACL = (PACL) LocalAlloc(LPTR, ccacheACLlength);
    if (!ccacheACL) {
	DebugEvent("KFW_set_ccache_dacl - LocalAlloc GLE = 0x%x", GetLastError());
	ret = 1;
	goto cleanup;
    }

    InitializeAcl(ccacheACL, ccacheACLlength, ACL_REVISION);
    AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
                         STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
                         pSystemSID);
    if (pTokenUser) {
	AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
			     STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
			     pTokenUser->User.Sid);
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL,
				   ccacheACL,
				   NULL)) {
	    gle = GetLastError();
	    DebugEvent("SetNamedSecurityInfo DACL (1) failed: GLE = 0x%lX", gle);
	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   OWNER_SECURITY_INFORMATION,
				   pTokenUser->User.Sid,
				   NULL,
				   NULL,
				   NULL)) {
	    gle = GetLastError();
	    DebugEvent("SetNamedSecurityInfo OWNER (2) failed: GLE = 0x%lX", gle);
	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    } else {
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL,
				   ccacheACL,
				   NULL)) {
	    gle = GetLastError();
	    DebugEvent("SetNamedSecurityInfo DACL (3) failed: GLE = 0x%lX", gle);
	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    }

  cleanup:
    if (pSystemSID)
	LocalFree(pSystemSID);
    if (pTokenUser)
	LocalFree(pTokenUser);
    if (ccacheACL)
	LocalFree(ccacheACL);
    return ret;
}

int KFW_set_ccache_dacl_with_user_sid(char *filename, PSID pUserSID)
{
    // SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_SID_AUTHORITY;
    PSID pSystemSID = NULL;
    DWORD SystemSIDlength = 0, UserSIDlength = 0;
    PACL ccacheACL = NULL;
    DWORD ccacheACLlength = 0;
    DWORD gle;
    int ret = 0;

    if (!filename) {
	DebugEvent0("KFW_set_ccache_dacl_with_user_sid - invalid parms");
	return 1;
    }

    DebugEvent0("KFW_set_ccache_dacl_with_user_sid");

    /* Get System SID */
    if (!ConvertStringSidToSid("S-1-5-18", &pSystemSID)) {
	DebugEvent("KFW_set_ccache_dacl - ConvertStringSidToSid GLE = 0x%x", GetLastError());
	ret = 1;
	goto cleanup;
    }

    /* Create ACL */
    SystemSIDlength = GetLengthSid(pSystemSID);
    ccacheACLlength = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE)
        + SystemSIDlength - sizeof(DWORD);

    if (pUserSID) {
	UserSIDlength = GetLengthSid(pUserSID);

	ccacheACLlength += sizeof(ACCESS_ALLOWED_ACE) + UserSIDlength
	    - sizeof(DWORD);
    }

    ccacheACL = (PACL) LocalAlloc(LPTR, ccacheACLlength);
    if (!ccacheACL) {
	DebugEvent("KFW_set_ccache_dacl - LocalAlloc GLE = 0x%x", GetLastError());
	ret = 1;
	goto cleanup;
    }

    InitializeAcl(ccacheACL, ccacheACLlength, ACL_REVISION);
    AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
                         STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
                         pSystemSID);
    if (pUserSID) {
	AddAccessAllowedAceEx(ccacheACL, ACL_REVISION, 0,
			     STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
			     pUserSID);
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL,
				   ccacheACL,
				   NULL)) {
	    gle = GetLastError();
	    DebugEvent("SetNamedSecurityInfo DACL (4) failed: GLE = 0x%lX", gle);
	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   OWNER_SECURITY_INFORMATION,
				   pUserSID,
				   NULL,
				   NULL,
				   NULL)) {
	    gle = GetLastError();
	    DebugEvent("SetNamedSecurityInfo OWNER (5) failed: GLE = 0x%lX", gle);
	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    } else {
	if (!SetNamedSecurityInfo( filename, SE_FILE_OBJECT,
				   DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				   NULL,
				   NULL,
				   ccacheACL,
				   NULL)) {
	    gle = GetLastError();
	    DebugEvent("SetNamedSecurityInfo DACL (6) failed: GLE = 0x%lX", gle);
	    if (gle != ERROR_NO_TOKEN)
		ret = 1;
	}
    }

  cleanup:
    if (pSystemSID)
	LocalFree(pSystemSID);
    if (ccacheACL)
	LocalFree(ccacheACL);
    return ret;
}

int KFW_obtain_user_temp_directory(HANDLE hUserToken, char *newfilename, int size)
{
    int  retval = 0;
    DWORD dwSize = size-1;	/* leave room for nul */
    DWORD dwLen  = 0;

    if (!hUserToken || !newfilename || size <= 0)
	return 1;

    *newfilename = '\0';

    dwLen = ExpandEnvironmentStringsForUser(hUserToken, "%TEMP%", newfilename, dwSize);
    if ( !dwLen || dwLen > dwSize )
	dwLen = ExpandEnvironmentStringsForUser(hUserToken, "%TMP%", newfilename, dwSize);
    if ( !dwLen || dwLen > dwSize )
	return 1;

    newfilename[dwSize] = '\0';
    return 0;
}

void
KFW_copy_cache_to_system_file(const char * user, const char * filename)
{
    char cachename[MAX_PATH + 8] = "FILE:";
    krb5_context		ctx = 0;
    krb5_error_code		code;
    krb5_principal              princ = 0;
    krb5_ccache			cc  = 0;
    krb5_ccache                 ncc = 0;
    PSECURITY_ATTRIBUTES        pSA = NULL;

    if (!pkrb5_init_context || !user || !filename)
        return;

    strncat(cachename, filename, sizeof(cachename));
    cachename[sizeof(cachename)-1] = '\0';

    DebugEvent("KFW_Logon_Event - ccache %s", cachename);

    DeleteFile(filename);

    code = pkrb5_init_context(&ctx);
    if (code) goto cleanup;

    code = pkrb5_parse_name(ctx, user, &princ);
    if (code) goto cleanup;

    code = KFW_get_ccache(ctx, princ, &cc);
    if (code) goto cleanup;

    code = pkrb5_cc_resolve(ctx, cachename, &ncc);
    if (code) goto cleanup;

    code = pkrb5_cc_initialize(ctx, ncc, princ);
    if (code) goto cleanup;

    code = KFW_set_ccache_dacl(filename, NULL);
    if (code) goto cleanup;

    code = pkrb5_cc_copy_creds(ctx,cc,ncc);

  cleanup:
    if ( cc ) {
        pkrb5_cc_close(ctx, cc);
        cc = 0;
    }
    if ( ncc ) {
        pkrb5_cc_close(ctx, ncc);
        ncc = 0;
    }
    if ( princ ) {
        pkrb5_free_principal(ctx, princ);
        princ = 0;
    }

    if (ctx)
        pkrb5_free_context(ctx);
}

int
KFW_copy_file_cache_to_default_cache(char * filename)
{
    char cachename[MAX_PATH + 8] = "FILE:";
    krb5_context		ctx = 0;
    krb5_error_code		code;
    krb5_principal              princ = 0;
    krb5_ccache			cc  = 0;
    krb5_ccache                 ncc = 0;
    int retval = 1;

    if (!pkrb5_init_context || !filename)
        return 1;

    if ( strlen(filename) + sizeof("FILE:") > sizeof(cachename) )
        return 1;

    code = pkrb5_init_context(&ctx);
    if (code) return 1;

    strcat(cachename, filename);

    code = pkrb5_cc_resolve(ctx, cachename, &cc);
    if (code) {
	DebugEvent0("kfwcpcc krb5_cc_resolve failed");
	goto cleanup;
    }

    code = pkrb5_cc_get_principal(ctx, cc, &princ);
    if (code) {
	DebugEvent0("kfwcpcc krb5_cc_get_principal failed");
	goto cleanup;
    }

    code = pkrb5_cc_default(ctx, &ncc);
    if (code) {
	DebugEvent0("kfwcpcc krb5_cc_default failed");
	goto cleanup;
    }
    if (!code) {
        code = pkrb5_cc_initialize(ctx, ncc, princ);

        if (!code)
            code = pkrb5_cc_copy_creds(ctx,cc,ncc);
	if (code) {
	    DebugEvent0("kfwcpcc krb5_cc_copy_creds failed");
	    goto cleanup;
	}
    }
    if ( ncc ) {
        pkrb5_cc_close(ctx, ncc);
        ncc = 0;
    }

    retval=0;   /* success */

  cleanup:
    if ( cc ) {
        pkrb5_cc_close(ctx, cc);
        cc = 0;
    }

    DeleteFile(filename);

    if ( princ ) {
        pkrb5_free_principal(ctx, princ);
        princ = 0;
    }

    if (ctx)
        pkrb5_free_context(ctx);

    return 0;
}


int
KFW_copy_file_cache_to_api_cache(char * filename)
{
    char cachename[MAX_PATH + 8] = "FILE:";
    krb5_context		ctx = 0;
    krb5_error_code		code;
    krb5_principal              princ = 0;
    krb5_ccache			cc  = 0;
    krb5_ccache                 ncc = 0;
    char 			*name = NULL;
    int retval = 1;

    if (!pkrb5_init_context || !filename)
        return 1;

    if ( strlen(filename) + sizeof("FILE:") > sizeof(cachename) )
        return 1;

    code = pkrb5_init_context(&ctx);
    if (code) return 1;

    strcat(cachename, filename);

    code = pkrb5_cc_resolve(ctx, cachename, &cc);
    if (code) {
	DebugEvent0("kfwcpcc krb5_cc_resolve failed");
	goto cleanup;
    }

    code = pkrb5_cc_get_principal(ctx, cc, &princ);
    if (code) {
	DebugEvent0("kfwcpcc krb5_cc_get_principal failed");
	goto cleanup;
    }

    code = pkrb5_unparse_name(ctx, princ, &name);
    if (code) {
	DebugEvent0("kfwcpcc krb5_unparse_name failed");
	goto cleanup;
    }

    sprintf(cachename, "API:%s", name);

    code = pkrb5_cc_resolve(ctx, cachename, &ncc);
    if (code) {
	DebugEvent0("kfwcpcc krb5_cc_default failed");
	goto cleanup;
    }
    if (!code) {
        code = pkrb5_cc_initialize(ctx, ncc, princ);

        if (!code)
            code = pkrb5_cc_copy_creds(ctx,cc,ncc);
	if (code) {
	    DebugEvent0("kfwcpcc krb5_cc_copy_creds failed");
	    goto cleanup;
	}
    }
    if ( ncc ) {
        pkrb5_cc_close(ctx, ncc);
        ncc = 0;
    }

    retval=0;   /* success */

  cleanup:
    if (name)
	pkrb5_free_unparsed_name(ctx, name);

    if ( cc ) {
        pkrb5_cc_close(ctx, cc);
        cc = 0;
    }

    DeleteFile(filename);

    if ( princ ) {
        pkrb5_free_principal(ctx, princ);
        princ = 0;
    }

    if (ctx)
        pkrb5_free_context(ctx);

    return 0;
}


int
KFW_destroy_tickets_for_principal(char * user)
{
    krb5_context		ctx = 0;
    krb5_error_code		code;
    krb5_principal      princ = 0;
    krb5_ccache			cc  = 0;

    if (!pkrb5_init_context)
        return 0;

    code = pkrb5_init_context(&ctx);
    if (code) return 1;

    code = pkrb5_parse_name(ctx, user, &princ);
    if (code) goto loop_cleanup;

    code = KFW_get_ccache(ctx, princ, &cc);
    if (code) goto loop_cleanup;

    code = pkrb5_cc_destroy(ctx, cc);
    if (!code) cc = 0;

  loop_cleanup:
    if ( cc ) {
        pkrb5_cc_close(ctx, cc);
        cc = 0;
    }
    if ( princ ) {
        pkrb5_free_principal(ctx, princ);
        princ = 0;
    }

    pkrb5_free_context(ctx);
    return 0;
}


/* There are scenarios in which an interactive logon will not
 * result in the LogonScript being executed.  This will result
 * in orphaned cache files being left in the Temp directory.
 * This function will search for cache files in the Temp
 * directory and delete any that are older than five minutes.
 */
void
KFW_cleanup_orphaned_caches(void)
{
    char * temppath = NULL;
    char * curdir = NULL;
    DWORD count, count2;
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    FILETIME now;
    ULARGE_INTEGER uli_now;
    FILETIME expired;

    count = GetTempPath(0, NULL);
    if (count <= 0)
        return;
    temppath = (char *) malloc(count);
    if (!temppath)
        goto cleanup;
    count2 = GetTempPath(count, temppath);
    if (count2 <= 0 || count2 > count)
        goto cleanup;

    count = GetCurrentDirectory(0, NULL);
    curdir = (char *)malloc(count);
    if (!curdir)
        goto cleanup;
    count2 = GetCurrentDirectory(count, curdir);
    if (count2 <= 0 || count2 > count)
        goto cleanup;

    if (!SetCurrentDirectory(temppath))
        goto cleanup;

    GetSystemTimeAsFileTime(&now);
    uli_now.u.LowPart = now.dwLowDateTime;
    uli_now.u.HighPart = now.dwHighDateTime;

    uli_now.QuadPart -= 3000000000;        /* 5 minutes == 3 billion 100 nano seconds */

    expired.dwLowDateTime = uli_now.u.LowPart;
    expired.dwHighDateTime = uli_now.u.HighPart;

    hFind = FindFirstFile("kfwlogon-*", &FindFileData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (CompareFileTime(&FindFileData.ftCreationTime, &expired) < 0) {
                DebugEvent("Deleting orphaned cache file: \"%s\"", FindFileData.cFileName);
                DeleteFile(FindFileData.cFileName);
            }
        } while ( FindNextFile(hFind, &FindFileData) );
    }

    SetCurrentDirectory(curdir);

  cleanup:
    if (temppath)
        free(temppath);
    if (hFind != INVALID_HANDLE_VALUE)
        FindClose(hFind);
    if (curdir)
        free(curdir);
}
