/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/*
 * krb_kntoln converts an auth name into a local name by looking up
 * the auth name in the /etc/aname file.  The format of the aname
 * file is:
 *
 * +-----+-----+-----+-----+------+----------+-------+-------+
 * | anl | inl | rll | lnl | name | instance | realm | lname |
 * +-----+-----+-----+-----+------+----------+-------+-------+
 * | 1by | 1by | 1by | 1by | name | instance | realm | lname |
 * +-----+-----+-----+-----+------+----------+-------+-------+
 *
 * If the /etc/aname file can not be opened it will set the
 * local name to the auth name.  Thus, in this case it performs as
 * the identity function.
 *
 * The name instance and realm are passed to krb_kntoln through
 * the AUTH_DAT structure (ad).
 *
 * Now here's what it *really* does:
 *
 * Given a Kerberos name in an AUTH_DAT structure, check that the
 * instance is null, and that the realm is the same as the local
 * realm, and return the principal's name in "lname".  Return
 * KSUCCESS if all goes well, otherwise KFAILURE.
 */

#include "krb_locl.h"

RCSID("$Id: kntoln.c,v 1.10 1998/06/09 19:25:21 joda Exp $");

int
krb_kntoln(AUTH_DAT *ad, char *lname)
{
    static char lrealm[REALM_SZ] = "";

    if (!(*lrealm) && (krb_get_lrealm(lrealm,1) == KFAILURE))
        return(KFAILURE);

    if (strcmp(ad->pinst, ""))
        return(KFAILURE);
    if (strcmp(ad->prealm, lrealm))
        return(KFAILURE);
    strcpy(lname, ad->pname);
    return(KSUCCESS);
}

#if 0
/* Posted to usenet by "Derrick J. Brashear" <shadow+@andrew.cmu.edu> */

#include <krb.h>
#include <ndbm.h>
#include <stdio.h>
#include <sys/file.h>
#include <strings.h>
#include <sys/syslog.h>
#include <sys/errno.h>

extern int errno;
/*
 * antoln converts an authentication name into a local name by looking up
 * the authentication name in the /etc/aname dbm database.
 * 
 * If the /etc/aname file can not be opened it will set the 
 * local name to the principal name.  Thus, in this case it performs as 
 * the identity function.
 * 
 * The name instance and realm are passed to antoln through
 * the AUTH_DAT structure (ad).
 */

static char     lrealm[REALM_SZ] = "";

int
an_to_ln(AUTH_DAT *ad, char *lname)
{
    static DBM *aname = NULL;
    char keyname[ANAME_SZ+INST_SZ+REALM_SZ+2];

    if(!(*lrealm) && (krb_get_lrealm(lrealm,1) == KFAILURE))
	return(KFAILURE);

    if((strcmp(ad->pinst,"") && strcmp(ad->pinst,"root")) ||
       strcmp(ad->prealm,lrealm)) {
	datum val;
	datum key;
	/*
	 * Non-local name (or) non-null and non-root instance.
	 * Look up in dbm file.
	 */
	if (!aname) {
	    if ((aname = dbm_open("/etc/aname", O_RDONLY, 0))
		== NULL) return (KFAILURE);
	}
	/* Construct dbm lookup key. */
	an_to_a(ad, keyname);
	key.dptr = keyname;
	key.dsize = strlen(keyname)+1;
	flock(dbm_dirfno(aname), LOCK_SH);
	val = dbm_fetch(aname, key);
	flock(dbm_dirfno(aname), LOCK_UN);
	if (!val.dptr) {
	    dbm_close(aname);
	    return(KFAILURE);
	}
	/* Got it! */
	strcpy(lname,val.dptr);
	return(KSUCCESS);
    } else strcpy(lname,ad->pname);
    return(KSUCCESS);
}

void
an_to_a(AUTH_DAT *ad, char *str)
{
    strcpy(str, ad->pname);
    if(*ad->pinst) {
	strcat(str, ".");
	strcat(str, ad->pinst);
    }
    strcat(str, "@");
    strcat(str, ad->prealm);
}

/*
 * Parse a string of the form "user[.instance][@realm]" 
 * into a struct AUTH_DAT.
 */

int
a_to_an(char *str, AUTH_DAT *ad)
{
    char *buf = (char *)malloc(strlen(str)+1);
    char *rlm, *inst, *princ;

    if(!(*lrealm) && (krb_get_lrealm(lrealm,1) == KFAILURE)) {
	free(buf);
	return(KFAILURE);
    }
    /* destructive string hacking is more fun.. */
    strcpy(buf, str);

    if (rlm = index(buf, '@')) {
	*rlm++ = '\0';
    }
    if (inst = index(buf, '.')) {
	*inst++ = '\0';
    }
    strcpy(ad->pname, buf);
    if(inst) strcpy(ad->pinst, inst);
    else *ad->pinst = '\0';
    if (rlm) strcpy(ad->prealm, rlm);
    else strcpy(ad->prealm, lrealm);
    free(buf);
    return(KSUCCESS);
}
#endif
