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
 * SCCS: @(#) tclUnixSock.c 1.6 96/08/08 08:48:51
 */

#include "tcl.h"
#include "tclPort.h"

/*
 * The following variable holds the network name of this host.
 */

#ifndef SYS_NMLN
#   define SYS_NMLN 100
#endif

static char hostname[SYS_NMLN + 1];
static int  hostnameInited = 0;

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetHostName --
 *
 *	Get the network name for this machine, in a system dependent way.
 *
 * Results:
 *	A string containing the network name for this machine, or
 *	an empty string if we can't figure out the name.
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
