/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: get_krbrlm.c,v 4.8 89/01/22 20:02:54 rfrench Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <stdio.h>
#include <krb.h>
#include <strings.h>

/*
 * krb_get_lrealm takes a pointer to a string, and a number, n.  It fills
 * in the string, r, with the name of the nth realm specified on the
 * first line of the kerberos config file (KRB_CONF, defined in "krb.h").
 * It returns 0 (KSUCCESS) on success, and KFAILURE on failure.  If the
 * config file does not exist, and if n=1, a successful return will occur
 * with r = KRB_REALM (also defined in "krb.h").
 *
 * NOTE: for archaic & compatibility reasons, this routine will only return
 * valid results when n = 1.
 *
 * For the format of the KRB_CONF file, see comments describing the routine
 * krb_get_krbhst().
 */

int
krb_get_lrealm(r,n)
    char *r;
    int n;
{
    FILE *cnffile, *fopen();

    if (n > 1)
	return(KFAILURE);  /* Temporary restriction */

    if ((cnffile = fopen(KRB_CONF, "r")) == NULL) {
	if (n == 1) {
	    (void) strcpy(r, KRB_REALM);
	    return(KSUCCESS);
	}
	else
	    return(KFAILURE);
    }

    if (fscanf(cnffile,"%s",r) != 1) {
        (void) fclose(cnffile);
        return(KFAILURE);
    }
    (void) fclose(cnffile);
    return(KSUCCESS);
}
