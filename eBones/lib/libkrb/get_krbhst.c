/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: get_krbhst.c,v 4.8 89/01/22 20:00:29 rfrench Exp $
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
 * Given a Kerberos realm, find a host on which the Kerberos authenti-
 * cation server can be found.
 *
 * krb_get_krbhst takes a pointer to be filled in, a pointer to the name
 * of the realm for which a server is desired, and an integer, n, and
 * returns (in h) the nth entry from the configuration file (KRB_CONF,
 * defined in "krb.h") associated with the specified realm.
 *
 * On end-of-file, krb_get_krbhst returns KFAILURE.  If n=1 and the
 * configuration file does not exist, krb_get_krbhst will return KRB_HOST
 * (also defined in "krb.h").  If all goes well, the routine returnes
 * KSUCCESS.
 *
 * The KRB_CONF file contains the name of the local realm in the first
 * line (not used by this routine), followed by lines indicating realm/host
 * entries.  The words "admin server" following the hostname indicate that
 * the host provides an administrative database server.
 *
 * For example:
 *
 *	ATHENA.MIT.EDU
 *	ATHENA.MIT.EDU kerberos-1.mit.edu admin server
 *	ATHENA.MIT.EDU kerberos-2.mit.edu
 *	LCS.MIT.EDU kerberos.lcs.mit.edu admin server
 *
 * This is a temporary hack to allow us to find the nearest system running
 * kerberos.  In the long run, this functionality will be provided by a
 * nameserver.
 */

int
krb_get_krbhst(h,r,n)
    char *h;
    char *r;
    int n;
{
    FILE *cnffile;
    char tr[REALM_SZ];
    char linebuf[BUFSIZ];
    register int i;

    if ((cnffile = fopen(KRB_CONF,"r")) == NULL) {
        if (n==1) {
            (void) strcpy(h,KRB_HOST);
            return(KSUCCESS);
        }
        else
            return(KFAILURE);
    }
    if (fscanf(cnffile,"%s",tr) == EOF)
        return(KFAILURE);
    /* run through the file, looking for the nth server for this realm */
    for (i = 1; i <= n;) {
	if (fgets(linebuf, BUFSIZ, cnffile) == NULL) {
            (void) fclose(cnffile);
            return(KFAILURE);
        }
	if (sscanf(linebuf, "%s %s", tr, h) != 2)
	    continue;
        if (!strcmp(tr,r))
            i++;
    }
    (void) fclose(cnffile);
    return(KSUCCESS);
}
