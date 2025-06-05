#ifndef __LEASH_INT_H__
#define __LEASH_INT_H__

#include <stdio.h>
#include <stdlib.h>

#include "leashdll.h"
#include <leashwin.h>

#include "tlhelp32.h"

#define MIT_PWD_DLL_CLASS "MITPasswordWndDLL"

BOOL
Register_MITPasswordEditControl(
    HINSTANCE hInst
    );

BOOL
Unregister_MITPasswordEditControl(
    HINSTANCE hInst
    );

// Some defines swiped from leash.h
//  These are necessary but they must be kept sync'ed with leash.h
#define HELPFILE "leash32.hlp"
extern char KRB_HelpFile[_MAX_PATH];

// Function Prototypes.
int DoNiftyErrorReport(long errnum, LPSTR what);
LONG Leash_timesync(int);

// Crap...
#include <krb5.h>

long
Leash_int_kinit_ex(
    krb5_context ctx,
    HWND hParent,
    char * principal,
    char * password,
    int lifetime,
    int forwardable,
    int proxiable,
    int renew_life,
    int addressless,
    unsigned long publicIP,
    int displayErrors
    );

long
Leash_int_checkpwd(
    char * principal,
    char * password,
    int    displayErrors
    );

long
Leash_int_changepwd(
    char * principal,
    char * password,
    char * newpassword,
    char** result_string,
    int    displayErrors
    );

int
Leash_krb5_kdestroy(
    void
    );

int
Leash_krb5_kinit(
    krb5_context,
    HWND hParent,
    char * principal_name,
    char * password,
    krb5_deltat lifetime,
    DWORD       forwardable,
    DWORD       proxiable,
    krb5_deltat renew_life,
    DWORD       addressless,
    DWORD       publicIP
    );

int
LeashKRB5_renew(void);

int
config_boolean_to_int(
    const char *s
    );

BOOL GetSecurityLogonSessionData(PSECURITY_LOGON_SESSION_DATA * ppSessionData);
BOOL IsKerberosLogon(VOID);

int Leash_krb5_error(krb5_error_code rc, LPCSTR FailedFunctionName,
                     int FreeContextFlag, krb5_context *ctx,
                     krb5_ccache *cache);
int Leash_krb5_initialize(krb5_context *);
krb5_error_code
Leash_krb5_cc_default(krb5_context *ctx, krb5_ccache *cache);

LPSTR err_describe(LPSTR buf, long code);

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
#define TOOLHELPDLL "kernel32.dll"
#define PSAPIDLL "psapi.dll"

// psapi functions
extern DECL_FUNC_PTR(GetModuleFileNameExA);
extern DECL_FUNC_PTR(EnumProcessModules);

// toolhelp functions
extern DECL_FUNC_PTR(CreateToolhelp32Snapshot);
extern DECL_FUNC_PTR(Module32First);
extern DECL_FUNC_PTR(Module32Next);

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
__cdecl,
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
__cdecl,
cc_shutdown,
    (
    apiCB** cc_ctx            // <> DLL's primary control structure. NULL after
    )
);

TYPEDEF_FUNC(
CCACHE_API,
__cdecl,
cc_get_NC_info,
    (
    apiCB* cc_ctx,          // >  DLL's primary control structure
    struct _infoNC*** ppNCi // <  (NULL before call) null terminated,
                            //    list of a structs (free via cc_free_infoNC())
    )
);

TYPEDEF_FUNC(
CCACHE_API,
__cdecl,
cc_free_NC_info,
    (
    apiCB* cc_ctx,
    struct _infoNC*** ppNCi // <  free list of structs returned by
                            //    cc_get_cache_names().  set to NULL on return
    )
);
#define CCAPI_DLL   "krbcc32.dll"

/* The following definitions are summarized from KRB5, Leash32, and
 * Leashw32 modules.  They are current as of KfW 2.6.2.  There is no
 * guarantee that changes to other modules will be updated in this list.
 */

/* Must match the values used in Leash32.exe */
#define LEASH_SETTINGS_REGISTRY_KEY_NAME "Software\\MIT\\Leash32\\Settings"
#define LEASH_SETTINGS_REGISTRY_VALUE_UPPERCASEREALM   "UpperCaseRealm"

/* These values are defined and used within Leashw32.dll */
#define LEASH_REGISTRY_KEY_NAME "Software\\MIT\\Leash"
#define LEASH_REGISTRY_VALUE_LIFETIME "lifetime"
#define LEASH_REGISTRY_VALUE_RENEW_TILL "renew_till"
#define LEASH_REGISTRY_VALUE_RENEWABLE "renewable"
#define LEASH_REGISTRY_VALUE_FORWARDABLE "forwardable"
#define LEASH_REGISTRY_VALUE_NOADDRESSES "noaddresses"
#define LEASH_REGISTRY_VALUE_PROXIABLE "proxiable"
#define LEASH_REGISTRY_VALUE_PUBLICIP "publicip"
#define LEASH_REGISTRY_VALUE_KINIT_OPT "hide_kinit_options"
#define LEASH_REGISTRY_VALUE_LIFE_MIN "life_min"
#define LEASH_REGISTRY_VALUE_LIFE_MAX "life_max"
#define LEASH_REGISTRY_VALUE_RENEW_MIN "renew_min"
#define LEASH_REGISTRY_VALUE_RENEW_MAX "renew_max"
#define LEASH_REGISTRY_VALUE_PRESERVE_KINIT "preserve_kinit_options"

/* must match values used within krb5_32.dll */
#define KRB5_REGISTRY_KEY_NAME "Software\\MIT\\Kerberos5"
#define KRB5_REGISTRY_VALUE_CCNAME      "ccname"
#define KRB5_REGISTRY_VALUE_CONFIGFILE  "config"

#endif /* __LEASH_INT_H__ */
