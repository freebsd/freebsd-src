/*
 * $Header$
 *
 * Copyright 2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <windows.h>
#include <stdio.h>  // for _snprintf
#include <malloc.h>
#include <stdlib.h>

extern "C" {
#include "cci_debugging.h"
#include "ccutils.h"
    }

#include "util.h"
#include "secure.hxx"


void* malloc_alloc_p(size_t size) {
    return malloc(size);
    }

void free_alloc_p(void *pptr) {
    void **real_pptr = (void**)pptr;
    if (*real_pptr) {
        free(*real_pptr);
        *real_pptr = 0;
        }
    }

extern "C" DWORD alloc_textual_sid(
    PSID pSid,          // binary Sid
    LPSTR *pTextualSid  // buffer for Textual representation of Sid
    ) {
    PSID_IDENTIFIER_AUTHORITY psia;
    DWORD dwSubAuthorities;
    DWORD dwSidRev = SID_REVISION;
    DWORD dwCounter;
    DWORD dwSidSize;

    *pTextualSid = 0;

    //
    // test if Sid passed in is valid
    //
    if(!IsValidSid(pSid)) return ERROR_INVALID_PARAMETER;

    // obtain SidIdentifierAuthority
    psia = GetSidIdentifierAuthority(pSid);
 
    // obtain sidsubauthority count
    dwSubAuthorities =* GetSidSubAuthorityCount(pSid);
 
    //
    // compute buffer length
    // S-SID_REVISION- + identifierauthority- + subauthorities- + NULL
    //
    dwSidSize = (15 + 12 + (12 * dwSubAuthorities) + 1) * sizeof(TCHAR);
    *pTextualSid = (LPSTR)malloc_alloc_p(dwSidSize);
    if (!*pTextualSid)
        return GetLastError();

    LPSTR TextualSid = *pTextualSid;

    //
    // prepare S-SID_REVISION-
    //
    wsprintf(TextualSid, TEXT("S-%lu-"), dwSidRev );
 
    //
    // prepare SidIdentifierAuthority
    //
    if ( (psia->Value[0] != 0) || (psia->Value[1] != 0) )
    {
        wsprintf(TextualSid + lstrlen(TextualSid),
                 TEXT("0x%02hx%02hx%02hx%02hx%02hx%02hx"),
                 (USHORT)psia->Value[0],
                 (USHORT)psia->Value[1],
                 (USHORT)psia->Value[2],
                 (USHORT)psia->Value[3],
                 (USHORT)psia->Value[4],
                 (USHORT)psia->Value[5]);
    }
    else
    {
        wsprintf(TextualSid + lstrlen(TextualSid), TEXT("%lu"),
                 (ULONG)(psia->Value[5]      )   +
                 (ULONG)(psia->Value[4] <<  8)   +
                 (ULONG)(psia->Value[3] << 16)   +
                 (ULONG)(psia->Value[2] << 24)   );
    }
 
    //
    // loop through SidSubAuthorities
    //
    for (dwCounter=0 ; dwCounter < dwSubAuthorities ; dwCounter++)
    {
        wsprintf(TextualSid + lstrlen(TextualSid), TEXT("-%lu"),
                 *GetSidSubAuthority(pSid, dwCounter) );
    }
    return 0;
}

DWORD alloc_token_user(HANDLE hToken, PTOKEN_USER *pptu) {
    DWORD status = 0;
    DWORD size = 0;
    *pptu = 0;

    GetTokenInformation(hToken, TokenUser, *pptu, 0, &size);
    if (size == 0) status = GetLastError();

    if (!status) {
        if (!(*pptu = (PTOKEN_USER)malloc_alloc_p(size)))
            status = GetLastError();
        }

    if (!status) {
        if (!GetTokenInformation(hToken, TokenUser, *pptu, size, &size))
            status = GetLastError();
        }

    if (status && *pptu) {
        free_alloc_p(pptu);
        }
    return status;
    }

DWORD
alloc_username(
    PSID Sid,
    LPSTR* pname,
    LPSTR* pdomain = 0
    )
{
    DWORD status = 0;
    DWORD name_len = 0;
    DWORD domain_len = 0;
    SID_NAME_USE snu;
    LPSTR name = 0;
    LPSTR domain = 0;

    *pname = 0;
    if (pdomain) *pdomain = 0;

    LookupAccountSidA(NULL, Sid, 0, &name_len, 0, &domain_len, &snu);
    if ((name_len == 0) || (domain_len == 0)) status = GetLastError();

    if (!status) {
        if (!(name = (LPSTR)malloc_alloc_p(name_len))) status = GetLastError();
        }

    if (!status) {
        if (!(domain = (LPSTR)malloc_alloc_p(domain_len))) status = GetLastError();
        }

    if (!status) {
        if (!LookupAccountSidA(NULL, Sid, name, &name_len, domain, &domain_len, &snu)) status = GetLastError();
        }

    if (status) {
        if (name)   free_alloc_p(&name);
        if (domain) free_alloc_p(&domain);
        } 
    else {
        if (pdomain) {
            *pname = name;
            *pdomain = domain;
            } 
        else {
            DWORD size = name_len + domain_len + 1;
            *pname = (LPSTR)malloc_alloc_p(size);
            if (!*pname) status = GetLastError();
            else _snprintf(*pname, size, "%s\\%s", name, domain);
            }
        }
    return status;
    }

DWORD get_authentication_id(HANDLE hToken, LUID* pAuthId) {
    TOKEN_STATISTICS ts;
    DWORD len;

    if (!GetTokenInformation(hToken, TokenStatistics, &ts, sizeof(ts), &len))
        return GetLastError();
    *pAuthId = ts.AuthenticationId;
    return 0;
    }

DWORD
alloc_name_9x(
    LPSTR* pname,
    LPSTR postfix
    )
{
    char prefix[] = "krbcc";
    DWORD len = (sizeof(prefix) - 1) + 1 + strlen(postfix) + 1;

    *pname = (LPSTR)malloc_alloc_p(len);
    if (!*pname) return GetLastError();
    _snprintf(*pname, len, "%s.%s", prefix, postfix);
    return 0;
}

DWORD alloc_name_NT(LPSTR* pname, LPSTR postfix) {
    DWORD status = 0;
    HANDLE hToken = 0;
    LUID auth_id;
#ifdef _DEBUG
    PTOKEN_USER ptu = 0;
    LPSTR name = 0;
    LPSTR domain = 0;
    LPSTR sid = 0;
#endif
    char prefix[] = "krbcc";
    // Play it safe and say 3 characters are needed per 8 bits (byte).
    // Note that 20 characters are needed for a 64-bit number in
    // decimal (plus one for the string termination.
    // and include room for sessionId.
    char lid[3*sizeof(LUID)+1+5];
    DWORD sessionId;
    DWORD len = 0;

    *pname = 0;

    status = SecureClient::Token(hToken);

    if (!status) {
        status = get_authentication_id(hToken, &auth_id);
        }

    if (!status) {
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
	        sessionId = 0;
        }

#ifdef _DEBUG
    if (!status) {status = alloc_token_user(hToken, &ptu);}
    if (!status) {status = alloc_username(ptu->User.Sid, &name, &domain);}
    if (!status) {status = alloc_textual_sid(ptu->User.Sid, &sid);}
#endif

    if (!status) {
        _snprintf(lid, sizeof(lid), "%I64u.%u", auth_id, sessionId);
        lid[sizeof(lid)-1] = 0; // be safe

        len = (sizeof(prefix) - 1) + 1 + strlen(lid) + 1 + strlen(postfix) + 1;
        *pname = (LPSTR)malloc_alloc_p(len);
        if (!*pname) status = GetLastError();
        }

    //
    // We used to allocate a name of the form:
    // "prefix.domain.name.sid.lid.postfix" (usually under 80
    // characters, depending on username).  However, XP thought this
    // was "invalid" (too long?) for some reason.
    //
    // Therefore, we now use "prefix.lid.postfix"
    // and for Terminal server we use "prefix.lid.sessionId.postfix"
    //

    if (!status) {
        _snprintf(*pname, len, "%s.%s.%s", prefix, lid, postfix);
        }

#ifdef _DEBUG
    if (sid)
        free_alloc_p(&sid);
    if (name)
        free_alloc_p(&name);
    if (domain)
        free_alloc_p(&domain);
    if (ptu)
        free_alloc_p(&ptu);
#endif
    if (hToken && hToken != INVALID_HANDLE_VALUE)
        CloseHandle(hToken);
    if (status && *pname)
        free_alloc_p(pname);
    return status;
}

extern "C" DWORD alloc_name(LPSTR* pname, LPSTR postfix, BOOL isNT) {
    return isNT ? alloc_name_NT(pname, postfix) : 
        alloc_name_9x(pname, postfix);
    }

extern "C" DWORD alloc_own_security_descriptor_NT(PSECURITY_DESCRIPTOR* ppsd) {
    DWORD status = 0;
    HANDLE hToken = 0;
    PTOKEN_USER ptu = 0;
    PSID pSid = 0;
    PACL pAcl = 0;
    DWORD size = 0;
    SECURITY_DESCRIPTOR sd;

    *ppsd = 0;

    if (!status) {status = SecureClient::Token(hToken);}

    // Get SID:
    if (!status) {status = alloc_token_user(hToken, &ptu);}

    if (!status) {
        size = GetLengthSid(ptu->User.Sid);
        pSid = (PSID) malloc_alloc_p(size);
        if (!pSid) status = GetLastError();
        }
    if (!status) {
        if (!CopySid(size, pSid, ptu->User.Sid)) status = GetLastError();
        }

    if (!status) {
        // Prepare ACL:
        size = sizeof(ACL);
        // Add an ACE:
        size += sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + GetLengthSid(pSid);
        pAcl = (PACL) malloc_alloc_p(size);
        if (!pAcl) status = GetLastError();
        }

    if (!status) {
        if (!InitializeAcl(pAcl, size, ACL_REVISION)) status = GetLastError();
        }

    if (!status) {
        if (!AddAccessAllowedAce(pAcl, ACL_REVISION, GENERIC_ALL, pSid)) status = GetLastError();
        }

    if (!status) {
        // Prepare SD itself:
        if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) status = GetLastError();
        }

    if (!status) {
        if (!SetSecurityDescriptorDacl(&sd, TRUE, pAcl, FALSE)) status = GetLastError();
        }

    if (!status) {
        if (!SetSecurityDescriptorOwner(&sd, pSid, FALSE)) status = GetLastError();
        }

    if (!status) {
        if (!IsValidSecurityDescriptor(&sd)) status = ERROR_INVALID_PARAMETER;
        }

    if (!status) {
        // We now have a SD.  Let's copy it.
        {
            // This should not succeed.  Instead it should give us the size.
            BOOL ok = MakeSelfRelativeSD(&sd, 0, &size);
            }
        if (size == 0) status = GetLastError();
        }

    if (!status) {
        *ppsd = (PSECURITY_DESCRIPTOR) malloc_alloc_p(size);
        if (!*ppsd) status = GetLastError();
        }

    if (!status) {
        if (!MakeSelfRelativeSD(&sd, *ppsd, &size)) status = GetLastError();
        }

    if (ptu)                free_alloc_p(&ptu);
    if (pSid)               free_alloc_p(&pSid);
    if (pAcl)               free_alloc_p(&pAcl);
    if (hToken && hToken != INVALID_HANDLE_VALUE)   CloseHandle(hToken);
    if (status && *ppsd)    free_alloc_p(ppsd);
    return status;
}

DWORD
alloc_module_file_name(
    char* module,
    char** pname
    )
{
    const DWORD max = 8192;
    DWORD status = 0;
    DWORD got = 0;
    DWORD size = 512; // use low number to test...
    HMODULE h = 0;
    BOOL ok = FALSE;
    char* name = 0;

    if (!pname)
        return ERROR_INVALID_PARAMETER;
    *pname = 0;

    h = GetModuleHandle(module);

    if (!h) return GetLastError();

    // We assume size < max and size > 0
    while (!status && !ok) {
        if (size > max) {
            // XXX - Assert?
            status = ERROR_INVALID_DATA;
            continue;
        }
        if (name) free_alloc_p(&name);
        name = (char*)malloc_alloc_p(size + 1);
        if (!name) {
            status = ERROR_NOT_ENOUGH_MEMORY;
            continue;
        }
        name[size] = 0;
        got = GetModuleFileName(h, name, size);
        if (!got) {
            status = GetLastError();
            // sanity check:
            if (!status) {
                // XXX - print nasty message...assert?
                status = ERROR_INVALID_DATA;
            }
            continue;
        }
        // To know we're ok, we need to verify that what we got
        // was bigger than GetModuleSize thought it got.
        ok = got && (got < size) && !name[got];
        size *= 2;
    }
    if (status && name)
        free_alloc_p(&name);
    else
        *pname = name;
    return status;
}

DWORD
alloc_module_dir_name(
    char* module,
    char** pname
    )
{
    DWORD status = alloc_module_file_name(module, pname);
    if (!status) {
        char* name = *pname;
        char* p = name + strlen(name);
        while ((p >= name) && (*p != '\\') && (*p != '/')) p--;
        if (p < name) {
            free_alloc_p(pname);
            status = ERROR_INVALID_DATA;
        } else {
            *p = 0;
        }
    }
    return status;
}

DWORD
alloc_module_dir_name_with_file(
    char* module,
    char* file,
    char** pname
    )
{
    DWORD status = alloc_module_dir_name(module, pname);
    if (!status) {
        char* name = *pname;
        size_t name_size = strlen(name);
        size_t size = name_size + 1 + strlen(file) + 1;
        char* result = (char*)malloc_alloc_p(size);
        if (!result) {
            status = ERROR_NOT_ENOUGH_MEMORY;
            free_alloc_p(pname);
        } else {
            strcpy(result, name);
            result[name_size] = '\\';
            strcpy(result + name_size + 1, file);
            free_alloc_p(pname);
            *pname = result;
        }
    }
    return status;
}

DWORD alloc_cmdline_2_args(char* prog,
                           char* arg1,
                           char* arg2,
                           char** pname) {
    DWORD   status  = 0;
    size_t  size    = strlen(prog) + strlen(arg1) + strlen(arg2) + 4;
    char*   result  = (char*)malloc_alloc_p(size);
    if (!result) {
        status = ERROR_NOT_ENOUGH_MEMORY;
        } 
    else {
        strcpy(result, prog);
        strcat(result, " ");
        strcat(result, arg1);
        strcat(result, " ");
        strcat(result, arg2);
        *pname = result;
        }
    cci_debug_printf("%s made <%s>", __FUNCTION__, result);
    return status;
    }
