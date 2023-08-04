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

/* We only support VC 1200 and above anyway */
#pragma once

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

#include <windows.h>
#include <npapi.h>
#define SECURITY_WIN32
#include <security.h>
#include <ntsecapi.h>
#include <tchar.h>
#include <strsafe.h>

typedef int errcode_t;

#include <loadfuncs-lsa.h>
#include <krb5.h>
#include <loadfuncs-com_err.h>
#include <loadfuncs-krb5.h>
#include <loadfuncs-profile.h>
#include <loadfuncs-leash.h>

// service definitions
#define SERVICE_DLL   "advapi32.dll"
typedef SC_HANDLE (WINAPI *FP_OpenSCManagerA)(char *, char *, DWORD);
typedef SC_HANDLE (WINAPI *FP_OpenServiceA)(SC_HANDLE, char *, DWORD);
typedef BOOL (WINAPI *FP_QueryServiceStatus)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL (WINAPI *FP_CloseServiceHandle)(SC_HANDLE);

/* In order to avoid including the private CCAPI headers */
typedef int cc_int32;

#define CC_API_VER_1 1
#define CC_API_VER_2 2

#define CCACHE_API cc_int32

/*
** The Official Error Codes
*/
#define CC_NOERROR           0
#define CC_BADNAME           1
#define CC_NOTFOUND          2
#define CC_END               3
#define CC_IO                4
#define CC_WRITE             5
#define CC_NOMEM             6
#define CC_FORMAT            7
#define CC_LOCKED            8
#define CC_BAD_API_VERSION   9
#define CC_NO_EXIST          10
#define CC_NOT_SUPP          11
#define CC_BAD_PARM          12
#define CC_ERR_CACHE_ATTACH  13
#define CC_ERR_CACHE_RELEASE 14
#define CC_ERR_CACHE_FULL    15
#define CC_ERR_CRED_VERSION  16

enum {
    CC_CRED_VUNKNOWN = 0,       // For validation
    /* CC_CRED_V4 = 1, */
    CC_CRED_V5 = 2,
    CC_CRED_VMAX = 3            // For validation
};

typedef struct opaque_dll_control_block_type* apiCB;
typedef struct _infoNC {
    char*     name;
    char*     principal;
    cc_int32  vers;
} infoNC;

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_initialize,
    (
    apiCB** cc_ctx,           // <  DLL's primary control structure.
                              //    returned here, passed everywhere else
    cc_int32 api_version,     // >  ver supported by caller (use CC_API_VER_1)
    cc_int32*  api_supported, // <  if ~NULL, max ver supported by DLL
    const char** vendor       // <  if ~NULL, vendor name in read only C string
    )
);

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_shutdown,
    (
    apiCB** cc_ctx            // <> DLL's primary control structure. NULL after
    )
);

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_get_NC_info,
    (
    apiCB* cc_ctx,          // >  DLL's primary control structure
    struct _infoNC*** ppNCi // <  (NULL before call) null terminated,
                            //    list of a structs (free via cc_free_infoNC())
    )
);

TYPEDEF_FUNC(
CCACHE_API,
CALLCONV_C,
cc_free_NC_info,
    (
    apiCB* cc_ctx,
    struct _infoNC*** ppNCi // <  free list of structs returned by
                            //    cc_get_cache_names().  set to NULL on return
    )
);
/* End private ccapiv2 headers */

#ifdef _WIN64
#define CCAPI_DLL   "krbcc64.dll"
#else
#define CCAPI_DLL   "krbcc32.dll"
#endif


/* */
#define MAX_USERNAME_LENGTH 256
#define MAX_PASSWORD_LENGTH 256
#define MAX_DOMAIN_LENGTH 256

#define KFW_LOGON_EVENT_NAME TEXT("MIT Kerberos")

BOOLEAN APIENTRY DllEntryPoint(HANDLE dll, DWORD reason, PVOID reserved);

DWORD APIENTRY NPGetCaps(DWORD index);

DWORD APIENTRY NPLogonNotify(
	PLUID lpLogonId,
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	LPWSTR *lpLogonScript);

DWORD APIENTRY NPPasswordChangeNotify(
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	DWORD dwChangeInfo);

#ifdef __cplusplus
extern "C" {
#endif

void DebugEvent0(char *a);
void DebugEvent(char *b,...);

DWORD MapAuthError(DWORD code);

static BOOL WINAPI UnicodeStringToANSI(UNICODE_STRING uInputString, LPSTR lpszOutputString, int nOutStringLen);

int KFW_is_available(void);
int KFW_get_cred( char * username, char * password, int lifetime, char ** reasonP );
void KFW_copy_cache_to_system_file(const char * user, const char * filename);
int KFW_destroy_tickets_for_principal(char * user);
int KFW_set_ccache_dacl(char *filename, HANDLE hUserToken);
int KFW_set_ccache_dacl_with_user_sid(char *filename, PSID pUserSID);
int KFW_obtain_user_temp_directory(HANDLE hUserToken, char *newfilename, int size);
void KFW_cleanup_orphaned_caches(void);

void CALLBACK LogonEventHandlerA(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow);

#ifdef __cplusplus
}
#endif
