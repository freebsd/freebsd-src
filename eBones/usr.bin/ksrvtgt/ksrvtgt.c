/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Get a ticket-granting-ticket given a service key file (srvtab)
 * The lifetime is the shortest allowed [1 five-minute interval]
 *
 *	from: ksrvtgt.c,v 4.3 89/07/28 10:17:28 jtkohl Exp $
 *	$FreeBSD$
 */

#ifndef lint
const char rcsid[] =
"$FreeBSD$";
#endif /* lint */

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <krb.h>
#include <conf.h>

int
main(argc,argv)
    int argc;
    char **argv;
{
    char realm[REALM_SZ + 1];
    register int code;
    char srvtab[MAXPATHLEN + 1];

    bzero(realm, sizeof(realm));
    bzero(srvtab, sizeof(srvtab));

    if (argc < 3 || argc > 5) {
	fprintf(stderr, "Usage: %s name instance [[realm] srvtab]\n",
		argv[0]);
	exit(1);
    }

    if (argc == 4)
	(void) strncpy(srvtab, argv[3], sizeof(srvtab) -1);

    if (argc == 5) {
	(void) strncpy(realm, argv[3], sizeof(realm) - 1);
	(void) strncpy(srvtab, argv[4], sizeof(srvtab) -1);
    }

    if (srvtab[0] == 0)
	(void) strcpy(srvtab, KEYFILE);

    if (realm[0] == 0)
	if (krb_get_lrealm(realm, 1) != KSUCCESS)
	    (void) strcpy(realm, KRB_REALM);

    code = krb_get_svc_in_tkt(argv[1], argv[2], realm,
			      "krbtgt", realm, 1, srvtab);
    if (code)
	fprintf(stderr, "%s\n", krb_err_txt[code]);
    exit(code);
}
