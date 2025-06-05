#ifndef __LOADFUNCS_LSA_H__
#define __LOADFUNCS_LSA_H__

#include "loadfuncs.h"

#define SECUR32_DLL "secur32.dll"
#define ADVAPI32_DLL "advapi32.dll"

TYPEDEF_FUNC(
    NTSTATUS,
    NTAPI,
    LsaConnectUntrusted,
    (PHANDLE)
    );
TYPEDEF_FUNC(
    NTSTATUS,
    NTAPI,
    LsaLookupAuthenticationPackage,
    (HANDLE, PLSA_STRING, PULONG)
    );
TYPEDEF_FUNC(
    NTSTATUS,
    NTAPI,
    LsaCallAuthenticationPackage,
    (HANDLE, ULONG, PVOID, ULONG, PVOID *, PULONG, PNTSTATUS)
    );
TYPEDEF_FUNC(
    NTSTATUS,
    NTAPI,
    LsaFreeReturnBuffer,
    (PVOID)
    );
TYPEDEF_FUNC(
    ULONG,
    NTAPI,
    LsaNtStatusToWinError,
    (NTSTATUS)
    );
TYPEDEF_FUNC(
    NTSTATUS,
    NTAPI,
    LsaGetLogonSessionData,
    (PLUID, PSECURITY_LOGON_SESSION_DATA*)
    );
#endif /* __LOADFUNCS_LSA_H__ */
