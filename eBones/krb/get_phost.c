/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: get_phost.c,v 4.6 89/01/23 09:25:40 jtkohl Exp $
 *	$Id: get_phost.c,v 1.3 1995/07/18 16:38:35 mark Exp $
 */

#if 0
#ifndef lint
static char rcsid[] =
"$Id: get_phost.c,v 1.3 1995/07/18 16:38:35 mark Exp $";
#endif /* lint */
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <krb.h>

/*
 * This routine takes an alias for a host name and returns the first
 * field, lower case, of its domain name.  For example, if "menel" is
 * an alias for host officially named "menelaus" (in /etc/hosts), for
 * the host whose official name is "MENELAUS.MIT.EDU", the name "menelaus"
 * is returned.
 *
 * This is done for historical Athena reasons: the Kerberos name of
 * rcmd servers (rlogin, rsh, rcp) is of the form "rcmd.host@realm"
 * where "host"is the lowercase for of the host name ("menelaus").
 * This should go away: the instance should be the domain name
 * (MENELAUS.MIT.EDU).  But for now we need this routine...
 *
 * A pointer to the name is returned, if found, otherwise a pointer
 * to the original "alias" argument is returned.
 */

char *krb_get_phost(char *alias)
{
    struct hostent *h;
    char *phost = alias;
    if ((h=gethostbyname(alias)) != (struct hostent *)NULL ) {
        char *p = index( h->h_name, '.' );
        if (p)
            *p = NULL;
        p = phost = h->h_name;
        do {
            if (isupper(*p)) *p=tolower(*p);
        } while (*p++);
    }
    return(phost);
}
