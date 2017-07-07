/*
 *	@doc HESIOD
 *
 * @module hespwnam.c |
 *
 * This file contains hes_getpwnam, for retrieving passwd information about
 * a user.
 *
 * For copying and distribution information, see the file
 * <lt> mit-copyright.h <gt>
 *
 * Original version by Steve Dyer, IBM/Project Athena.
 *
 *	  WSHelper DNS/Hesiod Library for WINSOCK
 *
 *
 */

/* This file contains hes_getpwnam, for retrieving passwd information about
 * a user.
 *
 * For copying and distribution information, see the file <mit-copyright.h>
 *
 * Original version by Steve Dyer, IBM/Project Athena.
 *
 */

#include <stdio.h>
#include <string.h> /*s*/

#include <stdlib.h>

#include <windows.h>
#include <hesiod.h>

#include "pwd.h"

extern DWORD dwHesPwNamIndex;
extern DWORD dwHesPwUidIndex;

#define MAX_PW_BUFFER_LENGTH  64

static char *
_NextPWField(char *ptr);

struct passwd *  GetPasswdStruct(struct passwd* pw, char* buf);




/*
	Given a UID this function will return the pwd information, eg username, uid,
	gid, fullname, office location, phone number, home directory, and default shell

	defined in hespwnam.c
	\param	uid			The user ID
	\retval				NULL if there was an error or a pointer to the passwd structure. The caller must
						never attempt to modify this structure or to free any of its components.
						Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
						issuing another hes_getpwuid call
*/
struct passwd *
WINAPI
hes_getpwuid(int uid)
{
    char **pp;
    struct passwd* pw = NULL;
    char buf[256];

    char nam[8];
    sprintf(nam, "%d", uid);

    pp = hes_resolve(nam, "uid");
    if (pp == NULL || *pp == NULL)
        return(NULL);

    pw = (struct passwd*)(TlsGetValue(dwHesPwUidIndex));
    if (pw == NULL) {
	LPVOID lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct passwd));
	if (lpvData != NULL) {
	    TlsSetValue(dwHesPwUidIndex, lpvData);
	    pw = (struct passwd*)lpvData;
	} else
	    return NULL;
    }

    strcpy(buf, pp[0]);
    hes_free(pp);
    return GetPasswdStruct(pw, buf);
}


/*
	Given a username this function will return the pwd information, eg
	username, uid, gid, fullname, office location, phone number, home
	directory, and default shell

	defined in hespwnam.c

	\param	nam			a pointer to the username

	\retval				NULL if there was an error or a pointer to the passwd structure. The caller must
						never attempt to modify this structure or to free any of its components.
						Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
						issuing another hes_getpwnam call

*/
struct passwd *
WINAPI
hes_getpwnam(char *nam)
{

   char **pp;
   struct passwd* pw = NULL;
   char buf[256];

    pp = hes_resolve(nam, "passwd");
    if (pp == NULL || *pp == NULL)
        return(NULL);

    pw = (struct passwd*)(TlsGetValue(dwHesPwNamIndex));
    if (pw == NULL) {
	LPVOID lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct passwd));
	if (lpvData != NULL) {
	    TlsSetValue(dwHesPwNamIndex, lpvData);
	    pw = (struct passwd*)lpvData;
	} else
	    return NULL;
    }

    strcpy(buf, pp[0]);
    hes_free(pp);
    return GetPasswdStruct(pw, buf);
}


struct passwd*  GetPasswdStruct(struct passwd* pw, char* buf)
{
    char* temp;
    char* p;

    if (pw->pw_name == NULL)
	pw->pw_name = LocalAlloc(LPTR, MAX_PW_BUFFER_LENGTH);
    if (pw->pw_passwd == NULL)
	pw->pw_passwd = LocalAlloc(LPTR, MAX_PW_BUFFER_LENGTH);
    if (pw->pw_comment == NULL)
	pw->pw_comment = LocalAlloc(LPTR, MAX_PW_BUFFER_LENGTH);
    if (pw->pw_gecos == NULL)
	pw->pw_gecos = LocalAlloc(LPTR, MAX_PW_BUFFER_LENGTH);
    if (pw->pw_dir == NULL)
	pw->pw_dir = LocalAlloc(LPTR, MAX_PW_BUFFER_LENGTH);
    if (pw->pw_shell == NULL)
	pw->pw_shell = LocalAlloc(LPTR, MAX_PW_BUFFER_LENGTH);
    /* choose only the first response (only 1 expected) */
    p = buf;
    temp = p;
    p = _NextPWField(p);
    strcpy(pw->pw_name, temp);
    temp = p;
    p = _NextPWField(p);
    strcpy(pw->pw_passwd, temp);
    pw->pw_uid = atoi(p);
    p = _NextPWField(p);
    pw->pw_gid = atoi(p);
    pw->pw_quota = 0;
    strcpy(pw->pw_comment, "");
    p = _NextPWField(p);
    temp = p;
    p = _NextPWField(p);
    strcpy(pw->pw_gecos, temp);
    temp = p;
    p = _NextPWField(p);
    strcpy(pw->pw_dir,  temp);
    temp = p;
    while (*p && *p != '\n')
        p++;
    *p = '\0';
    strcpy(pw->pw_shell, temp);
    return pw;


}

/* Move the pointer forward to the next colon-separated field in the
 * password entry.
 */

static char *
_NextPWField(char *ptr)
{
    while (*ptr && *ptr != '\n' && *ptr != ':')
        ptr++;
    if (*ptr)
        *ptr++ = '\0';
    return(ptr);
}
