/*
  @doc HESIOD

  @module hesiod.c |

  This module contains the defintions for the exported functions:
		hes_to_bind
		hes_resolve
		hes_error
        hes_free
  as well as the internal function hes_init. The hes_init function
  is the one that determines what the Hesiod servers are for your
  site and will parse the configuration files, if any are
  present.

  WSHelper DNS/Hesiod Library for WINSOCK

*/

/* This file is part of the Hesiod library.
 *
 * The BIND 4.8.1 implementation of T_TXT is incorrect; BIND 4.8.1 declares
 * it as a NULL terminated string.  The RFC defines T_TXT to be a length
 * byte followed by arbitrary changes.
 *
 * Because of this incorrect declaration in BIND 4.8.1, when this bug is fixed,
 * T_TXT requests between machines running different versions of BIND will
 * not be compatible (nor is there any way of adding compatibility).
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.  See the
 * file <mit-copyright.h> for copying and distribution information.
 */

#define index(str, c) strchr(str,c)
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <windows.h>
#include <winsock.h>
#include <string.h>
#include <hesiod.h>
#include <resolv.h>
#include <windns.h>

#include "resource.h"


#define USE_HS_QUERY	/* undefine this if your higher-level name servers */
			/* don't know class HS */

char HesConfigFile[_MAX_PATH];
static char Hes_LHS[256];
static char Hes_RHS[256];
static int Hes_Errno = HES_ER_UNINIT;

extern  DWORD dwHesIndex;



/*

  @func int | hes_init |

  This function is not exported.  It takes no arguments. However it is
  important to understand how this works. It sets the global variables
  Hes_LHS and Hes_RHS which are used to form the Hesiod
  queries. Understanding how this works and setting up the correct
  configuration will determine if the Hesiod queries will work at your
  site. Settings can be configured by makgin source code changes and
  rebuilding the DLL, editing resources in the DLL, using a
  configuration file, or setting an environment variable.

  The function first tries to open the HesConfigFile and set the
  Hes_RHS and Hes_LHS variables from this. If there is no config file
  then the function tries to load a string resource from the DLL to
  set the LHS and RHS. If the string resources cannot be loaded then
  the LHS and RHS will be set by the values of DEF_LHS and DEF_RHS,
  these are defined in hesiod.h. Note that the string resources are by
  default set to these same values since the RC files include hesiod.h

  Finally if the user sets the environment variable HES_DOMAIN the RHS
  will be overridden by the value of the HES_DOMAIN value.

  Note that LoadString requires us to first find the module handle of
  the DLL. We have to use the internal module name as defined in the
  DEF file. If you change the library name within the DEF file you
  also need to change the appropriate string in hesiod.c

*/
int hes_init( void )
{
    register FILE *fp;
    register char  *key;
    register char  *cp;
    char buf[MAXDNAME+7];
    HMODULE hModWSHelp;


    Hes_Errno = HES_ER_UNINIT;
    Hes_LHS[0] = '\0';
    Hes_RHS[0] = '\0';

    // Note: these must match the DEF file entries
#if defined(_WIN64)
	hModWSHelp = GetModuleHandle( "WSHELP64" );
#else
	hModWSHelp = GetModuleHandle( "WSHELP32" );
#endif

    if(!LoadString( hModWSHelp, IDS_DEF_HES_CONFIG_FILE,
                    HesConfigFile, sizeof(HesConfigFile) )){
        strcpy( HesConfigFile, HESIOD_CONF);
    }

    if ((fp = fopen(HesConfigFile, "r")) == NULL) {
        /* use defaults compiled in */
        /* no file or no access uses defaults */
        /* but poorly formed file returns error */

        if(!LoadString( hModWSHelp, IDS_DEF_HES_RHS, Hes_RHS, sizeof(Hes_RHS) )){
            strcpy( Hes_RHS, DEF_RHS);
        }

        if(!LoadString( hModWSHelp, IDS_DEF_HES_LHS, Hes_LHS, sizeof(Hes_LHS) )){
            strcpy( Hes_LHS, DEF_LHS);
        }
    } else {
        while(fgets((LPSTR) buf, MAXDNAME+7, fp) != NULL) {
            cp = (LPSTR) buf;
            if (*cp == '#' || *cp == '\n'){
                continue;
            }
            while(*cp == ' ' || *cp == '\t'){
                cp++;
            }
            key = cp;
            while(*cp != ' ' && *cp != '\t' && *cp != '='){
                cp++;
            }
            *cp++ = '\0';
            if (strcmp(key, "lhs") == 0){
                strncpy(&Hes_LHS[0], cp, (strlen(cp)-1));
            } else if (strcmp(key, "rhs") == 0){
                strncpy(&Hes_RHS[0], cp, (strlen(cp)-1));
            } else {
                continue;
            }
            while(*cp == ' ' || *cp == '\t' || *cp == '='){
                cp++;
            }
            if (*cp != '.') {
                Hes_Errno = HES_ER_CONFIG;
                fclose(fp);
                return(Hes_Errno);
            }
            // len = strlen(cp);
            // *cpp = calloc((unsigned int) len, sizeof(char));
            // (void) strncpy(*cpp, cp, len-1);
        }
        fclose(fp);
    }
    /* see if the RHS is overridden by environment variable */
    if ((cp = getenv("HES_DOMAIN")) != NULL){
        // Hes_RHS = strcpy(malloc(strlen(cp)+1),cp);
        strcpy(Hes_RHS,cp);
    }
    /* the LHS may be null, the RHS must not be null */
    if (Hes_RHS == NULL)
        Hes_Errno = HES_ER_CONFIG;
    else
        Hes_Errno = HES_ER_OK;
    return(Hes_Errno);
}


/*
  hes_to_bind function use the LHS and RHS values and
  binds them with the parameters so that a well formed DNS query may
  be performed.

  \param[in]	HesiodName		The Hesiod name such as a username or service name
  \param[in]	HesiodNameType	The Hesiod name type such as pobox, passwd, or sloc

  \retval		Returns NULL if there was an error. Otherwise the pointer to a string containing a valid query is returned.

*/
char *
WINAPI
hes_to_bind(LPSTR HesiodName,
            LPSTR HesiodNameType)
{
    register char *cp, **cpp;
    char* bindname;
    LPVOID lpvData;
    char *RHS;

    cp = NULL;
    cpp = NULL;

    bindname = (LPSTR)(TlsGetValue(dwHesIndex));
    if (bindname == NULL)
    {
        lpvData = LocalAlloc(LPTR, DNS_MAX_NAME_BUFFER_LENGTH);
        if (lpvData != NULL)
        {
            TlsSetValue(dwHesIndex, lpvData);
            bindname = (LPSTR)lpvData;
        }
        else
            return NULL;
    }
    if (Hes_Errno == HES_ER_UNINIT || Hes_Errno == HES_ER_CONFIG)
        (void) hes_init();
    if (Hes_Errno == HES_ER_CONFIG)
	return(NULL);
    if (cp = index(HesiodName,'@')) {
        if (index(++cp,'.'))
            RHS = cp;
        else
            if (cpp = hes_resolve(cp, "rhs-extension"))
                RHS = *cpp;
            else {
                Hes_Errno = HES_ER_NOTFOUND;
                return(NULL);
            }
        (void) strcpy(bindname,HesiodName);
        (*index(bindname,'@')) = '\0';
    } else {
        RHS = Hes_RHS;
        (void) strcpy(bindname, HesiodName);
    }
    (void) strcat(bindname, ".");
    (void) strcat(bindname, HesiodNameType);
    if (Hes_LHS) {
        if (Hes_LHS[0] != '.')
            (void) strcat(bindname,".");
        (void) strcat(bindname, Hes_LHS);
    }
    if (RHS[0] != '.')
        (void) strcat(bindname,".");
    (void) strcat(bindname, RHS);

    if(cpp != NULL )
        hes_free(cpp);

    return(bindname);
}


/*
	This function calls hes_to_bind to form a valid hesiod query, then queries the dns database.
	defined in hesiod.c

	\param[in]	HesiodName		The Hesiod name such as a username or service name
	\param[in]	HesiodNameType	The Hesiod name type such as pobox, passwd, or sloc

	\retval		returns a NULL terminated vector of strings (a la argv),
				one for each resource record containing Hesiod data, or NULL if
				there is any error. If there is an error call hes_error() to get
				further information. You will need to call hes_free to free the result

*/
char **
WINAPI
hes_resolve(LPSTR HesiodName, LPSTR HesiodNameType)
{
    register char  *cp;
    LPSTR* retvec;
    DNS_STATUS status;

    PDNS_RECORD pDnsRecord;
    PDNS_RECORD pR;
    DNS_FREE_TYPE freetype ;
    int i = 0;
    freetype =  DnsFreeRecordListDeep;


    cp = hes_to_bind(HesiodName, HesiodNameType);
    if (cp == NULL) return(NULL);
    errno = 0;


    status = DnsQuery_A(cp,                 //pointer to OwnerName
                        DNS_TYPE_TEXT,         //Type of the record to be queried
                        DNS_QUERY_STANDARD,     // Bypasses the resolver cache on the lookup.
                        NULL,                   //contains DNS server IP address
                        &pDnsRecord,                //Resource record comprising the response
                        NULL);                     //reserved for future use

    if (status) {
        errno = status;
        Hes_Errno = HES_ER_NOTFOUND;
        return  NULL;
    }

    pR = pDnsRecord;
    while (pR)
    {
        if (pR->wType == DNS_TYPE_TEXT)
            i++;
        pR = pR->pNext;
    }
    i++;
    retvec = LocalAlloc(LPTR, i*sizeof(LPSTR));
    pR = pDnsRecord;
    i = 0;
    while (pR)
    {
        if (pR->wType == DNS_TYPE_TEXT){
            SIZE_T l = strlen(((pR->Data).Txt.pStringArray)[0]);
            retvec[i] = LocalAlloc(LPTR, l+1);
            strcpy(retvec[i], ((pR->Data).Txt.pStringArray)[0]);
            i++;
        }
        pR = pR->pNext;
    }
    retvec[i] = NULL;
    DnsRecordListFree(pDnsRecord, freetype);
    return retvec;

}


/*
	The  function  hes_error may be called to determine the
	source of the error.  It does not take an argument.

	\retval		return one of the HES_ER_* codes defined in hesiod.h.
*/

int
WINAPI
hes_error(void)
{
    return(Hes_Errno);
}


/*

	The function hes_free should be called to free up memeory returned by
	hes_resolve

	\param[in]	hesinfo		a NULL terminiated array of strings returned by hes_resolve


*/
void
WINAPI
hes_free(LPSTR* info)
{
    int i= 0;
    for (; info[i]; i++)
    {
        LocalFree(info[i]);
    }
    LocalFree(info);
}