/*
 *	@doc HESIOD
 *
 * @module hesmailh.c |
 *
 * This file contains hes_postoffice, which retrieves post-office information
 * for a user.
 *
 *  For copying and distribution information, see the file
 *  <lt> mit-copyright.h <gt>
 *
 *  Original version by Steve Dyer, IBM/Project Athena.
 *
 *	WSHelper DNS/Hesiod Library for WINSOCK
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h> /*s*/

#include <hesiod.h>


#define LINESIZE 80

extern DWORD dwHesMailIndex;


/*
	This call is used to obtain a user's type of mail account and the location of that
	account. E.g. POP PO10.MIT.EDU or IMAP IMAP-TEST.MIT.EDU

	defined in hesmailh.c

	\param[in]	user	The username to be used when querying for the Hesiod Name Type POBOX.

	\retval				NULL if there was an error or if there was no entry for the
						username. Otherwise a pointer to a hes_postoffice structure is
						returned. The caller must never attempt to modify this structure or to free
						any of its components. Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
						issuing another getmailhost call

*/
struct hes_postoffice  *
WINAPI
hes_getmailhost(LPSTR user)
{
    struct hes_postoffice* ret;
    char linebuf[LINESIZE];
    char *p, *tmp;
    char **cp;


    cp = hes_resolve(user, "pobox");
    if (cp == NULL) return(NULL);

    ret = (struct hes_postoffice*)(TlsGetValue(dwHesMailIndex));
    if (ret == NULL) {
	LPVOID lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct hes_postoffice));
	if (lpvData != NULL) {
	    TlsSetValue(dwHesMailIndex, lpvData);
	    ret = (struct hes_postoffice*)lpvData;
	} else
	    return NULL;
    }
    if (!ret->po_type)
        ret->po_type = LocalAlloc(LPTR, LINESIZE);
    if (!ret->po_host)
        ret->po_host = LocalAlloc(LPTR, LINESIZE);
    if (!ret->po_name)
        ret->po_name = LocalAlloc(LPTR, LINESIZE);
    strcpy(linebuf, *cp);

    p = linebuf;
    tmp = linebuf;
    while(!isspace(*p)) p++;
    *p++ = '\0';
    strcpy(ret->po_type, tmp);
    tmp = p;
    while(!isspace(*p)) p++;
    *p++ = '\0';
    strcpy(ret->po_host, tmp);
    strcpy(ret->po_name, p);
    if (cp)
        hes_free(cp);
    return(ret);
}
