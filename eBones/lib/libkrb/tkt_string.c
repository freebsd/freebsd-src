/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: tkt_string.c,v 4.6 89/01/05 12:31:51 raeburn Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <krb.h>
#include <string.h>
#include <sys/param.h>

/*
 * This routine is used to generate the name of the file that holds
 * the user's cache of server tickets and associated session keys.
 *
 * If it is set, krb_ticket_string contains the ticket file name.
 * Otherwise, the filename is constructed as follows:
 *
 * If it is set, the environment variable "KRBTKFILE" will be used as
 * the ticket file name.  Otherwise TKT_ROOT (defined in "krb.h") and
 * the user's uid are concatenated to produce the ticket file name
 * (e.g., "/tmp/tkt123").  A pointer to the string containing the ticket
 * file name is returned.
 */

static char krb_ticket_string[MAXPATHLEN] = "";

char *tkt_string()
{
    char *env;
    uid_t getuid();

    if (!*krb_ticket_string) {
        if ((env = getenv("KRBTKFILE"))) {
	    (void) strncpy(krb_ticket_string, env,
			   sizeof(krb_ticket_string)-1);
	    krb_ticket_string[sizeof(krb_ticket_string)-1] = '\0';
	} else {
	    /* 32 bits of signed integer will always fit in 11 characters
	     (including the sign), so no need to worry about overflow */
	    (void) sprintf(krb_ticket_string, "%s%d",TKT_ROOT,getuid());
        }
    }
    return krb_ticket_string;
}

/*
 * This routine is used to set the name of the file that holds the user's
 * cache of server tickets and associated session keys.
 *
 * The value passed in is copied into local storage.
 *
 * NOTE:  This routine should be called during initialization, before other
 * Kerberos routines are called; otherwise tkt_string() above may be called
 * and return an undesired ticket file name until this routine is called.
 */

void
krb_set_tkt_string(val)
char *val;
{

    (void) strncpy(krb_ticket_string, val, sizeof(krb_ticket_string)-1);
    krb_ticket_string[sizeof(krb_ticket_string)-1] = '\0';

    return;
}
