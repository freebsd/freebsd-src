/* 
 * tclUnixSock.c --
 *
 *	This file contains Unix-specific socket related code.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixSock.c 1.9 97/10/09 18:24:49
 */

#include "tcl.h"
#include "tclPort.h"

/*
 * There is no portable macro for the maximum length
 * of host names returned by gethostbyname().  We should only
 * trust SYS_NMLN if it is at least 255 + 1 bytes to comply with DNS
 * host name limits.
 *
 * Note:  SYS_NMLN is a restriction on "uname" not on gethostbyname!
 *
 * For example HP-UX 10.20 has SYS_NMLN == 9,  while gethostbyname()
 * can return a fully qualified name from DNS of up to 255 bytes.
 *
 * Fix suggested by Viktor Dukhovni (viktor@esm.com)
 */

#if defined(SYS_NMLN) && SYS_NMLEN >= 256
#define TCL_HOSTNAME_LEN SYS_NMLEN
#else
#define TCL_HOSTNAME_LEN 256
#endif


/*
 * The following variable holds the network name of this host.
 */

static char hostname[TCL_HOSTNAME_LEN + 1];
static int  hostnameInited = 0;

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetHostName --
 *
 *	Returns the name of the local host.
 *
 * Results:
 *	A string containing the network name for this machine, or
 *	an empty string if we can't figure out the name.  The caller 
 *	must not modify or free this string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetHostName()
{
#ifndef NO_UNAME
    struct utsname u;
    struct hostent *hp;
#endif

    if (hostnameInited) {
        return hostname;
    }

#ifndef NO_UNAME
    (VOID *) memset((VOID *) &u, (int) 0, sizeof(struct utsname));
    if (uname(&u) > -1) {
        hp = gethostbyname(u.nodename);
        if (hp != NULL) {
            strcpy(hostname, hp->h_name);
        } else {
            strcpy(hostname, u.nodename);
        }
        hostnameInited = 1;
        return hostname;
    }
#else
    /*
     * Uname doesn't exist; try gethostname instead.
     */

    if (gethostname(hostname, sizeof(hostname)) > -1) {
	hostnameInited = 1;
        return hostname;
    }
#endif

    hostname[0] = 0;
    return hostname;
}
