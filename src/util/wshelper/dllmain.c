#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
#include "wsh-int.h"
#include <windns.h>
#include "hesiod.h"
#include "pwd.h"


DWORD dwHesIndex; // for hes_to_bind
DWORD dwHesMailIndex; // for hes_getmailhost
DWORD dwHesServIndex;  // for hes_getservbyname
DWORD dwHesPwNamIndex; // for hes_getpwnam;
DWORD dwHesPwUidIndex; // for hes_getpwuid
DWORD dwGhnIndex; // for rgethostbyname
DWORD dwGhaIndex; // for rgethostbyaddr

#define LISTSIZE 15

void FreeThreadLocalMemory();
void AllocateThreadLocalMemory();
void FreePasswdStruct(LPVOID lpvData);
void FreeHostentStruct(LPVOID lpvData);

BOOL
WINAPI
DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpvReserved   // reserved
)
{
    switch(fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if ((dwHesIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
        if ((dwHesMailIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
        if ((dwHesServIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
        if ((dwHesPwNamIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
	if ((dwHesPwUidIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
	if ((dwHesPwUidIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
	if ((dwGhnIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
	if ((dwGhaIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
            return FALSE;
        res_init_startup();
     case DLL_THREAD_ATTACH:
        // Initialize the TLS index for this thread.
        AllocateThreadLocalMemory();
        break;

     case DLL_THREAD_DETACH:

        // Release the allocated memory for this thread.
        FreeThreadLocalMemory();
        break;


    case DLL_PROCESS_DETACH:
        // Release the TLS index.
        FreeThreadLocalMemory();
        TlsFree(dwHesIndex);
        TlsFree(dwHesMailIndex);
        TlsFree(dwHesServIndex);
        TlsFree(dwHesPwNamIndex);
	TlsFree(dwHesPwUidIndex);
	TlsFree(dwGhnIndex);
	TlsFree(dwGhaIndex);

        res_init_cleanup();
        break;
    }
    return TRUE;
}

void AllocateThreadLocalMemory()
{
    LPVOID lpvData;

    lpvData = (LPVOID) LocalAlloc(LPTR, DNS_MAX_NAME_BUFFER_LENGTH);
    if (lpvData != NULL)
        TlsSetValue(dwHesIndex, lpvData);

    lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct hes_postoffice));
    if (lpvData != NULL)
        TlsSetValue(dwHesMailIndex, lpvData);

    lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct servent));
    if (lpvData != NULL)
        TlsSetValue(dwHesServIndex, lpvData);

    lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct passwd));
    if (lpvData != NULL)
        TlsSetValue(dwHesPwNamIndex, lpvData);

    lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct passwd));
    if (lpvData != NULL)
        TlsSetValue(dwHesPwUidIndex, lpvData);

    lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct hostent));
    if (lpvData != NULL)
        TlsSetValue(dwGhnIndex, lpvData);

    lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct hostent));
    if (lpvData != NULL)
        TlsSetValue(dwGhaIndex, lpvData);

}
void FreeThreadLocalMemory()
{
    LPVOID lpvData;
    int i;

    lpvData = TlsGetValue(dwHesIndex);
    if (lpvData != NULL)
        LocalFree((HLOCAL) lpvData);

    // free hes_postoffice
    lpvData = TlsGetValue(dwHesMailIndex);
    if (lpvData)
    {
        struct hes_postoffice* p = (struct hes_postoffice*) lpvData;
        if (p->po_type)
        {
            LocalFree(p->po_type);
            p->po_type = NULL;
        }
        if (p->po_host)
        {
           LocalFree(p->po_host);
           p->po_host = NULL;
        }
        if (p->po_name)
        {
            LocalFree(p->po_name);
            p->po_name = NULL;
        }
        LocalFree((HLOCAL) lpvData);
     }

    // free servent
    lpvData = TlsGetValue(dwHesServIndex);
    if (lpvData)
    {
        struct servent* s = (struct servent*) lpvData;
        if (s->s_name)
        {
            LocalFree(s->s_name);
            s->s_name = NULL;
        }
        if (s->s_proto)
        {
            LocalFree(s->s_proto);
            s->s_proto = NULL;
        }
        if (s->s_aliases)
        {
            for (i = 0; i<LISTSIZE; i++)
            {
                if (s->s_aliases[i])
                {
                    LocalFree(s->s_aliases[i]);
                    s->s_aliases[i] = NULL;
                }
            }
            LocalFree(s->s_aliases);
        }
        LocalFree((HLOCAL) lpvData);
    }

    // free struct passwd
    lpvData = TlsGetValue(dwHesPwNamIndex);
    FreePasswdStruct(lpvData);

    lpvData = TlsGetValue(dwHesPwUidIndex);
    FreePasswdStruct(lpvData);

    // free struct hostent
    lpvData = TlsGetValue(dwGhnIndex);
    FreeHostentStruct(lpvData);

    lpvData = TlsGetValue(dwGhaIndex);
    FreeHostentStruct(lpvData);

}


void FreeHostentStruct(LPVOID lpvData)
{
    if (lpvData)
    {
	int i = 0;
	struct hostent* host = (struct hostent*) lpvData;
	if (host->h_name)
	    LocalFree(host->h_name);
	if (host->h_aliases)
	{
	    while(host->h_aliases[i])
	    {
		LocalFree(host->h_aliases[i]);
		host->h_aliases[i] = NULL;
		i++;
	    }
	    LocalFree(host->h_aliases);
	}
	if (host->h_addr_list)
	{
	    i = 0;
	    while (host->h_addr_list[i])
	    {
		LocalFree(host->h_addr_list[i]);
		host->h_addr_list[i] = NULL;
		i++;
	    }
	    LocalFree(host->h_addr_list);
	}
	LocalFree((HLOCAL) lpvData);
    }
}

void FreePasswdStruct(LPVOID lpvData)
{
    if (lpvData)
    {
        struct passwd* p = (struct passwd*) lpvData;
        if (p->pw_name)
        {
            LocalFree(p->pw_name);
            p->pw_name = NULL;
        }
        if (p->pw_passwd)
        {
            LocalFree(p->pw_passwd);
            p->pw_passwd = NULL;
        }
        if (p->pw_comment)
        {
            LocalFree(p->pw_comment);
            p->pw_comment = NULL;
        }
        if (p->pw_gecos)
        {
            LocalFree(p->pw_gecos);
            p->pw_gecos = NULL;
        }
        if (p->pw_dir)
        {
            LocalFree(p->pw_dir);
            p->pw_dir = NULL;
        }
        if (p->pw_shell)
        {
            LocalFree(p->pw_shell);
            p->pw_shell = NULL;
        }
        LocalFree((HLOCAL) lpvData);
    }
}
