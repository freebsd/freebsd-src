/* 
 * tclIOSock.c --
 *
 *	Common routines used by all socket based channel types.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclIOSock.c 1.20 97/04/25 16:36:40
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 *----------------------------------------------------------------------
 *
 * TclSockGetPort --
 *
 *	Maps from a string, which could be a service name, to a port.
 *	Used by socket creation code to get port numbers and resolve
 *	registered service names to port numbers.
 *
 * Results:
 *	A standard Tcl result.  On success, the port number is
 *	returned in portPtr. On failure, an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclSockGetPort(interp, string, proto, portPtr)
    Tcl_Interp *interp;
    char *string;		/* Integer or service name */
    char *proto;		/* "tcp" or "udp", typically */
    int *portPtr;		/* Return port number */
{
    struct servent *sp;		/* Protocol info for named services */
    if (Tcl_GetInt(interp, string, portPtr) != TCL_OK) {
	sp = getservbyname(string, proto);    
	if (sp != NULL) {
	    *portPtr = ntohs((unsigned short) sp->s_port);
	    Tcl_ResetResult(interp);	/* clear error message */
	    return TCL_OK;
	}
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, string, portPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (*portPtr > 0xFFFF) {
        Tcl_AppendResult(interp, "couldn't open socket: port number too high",
                (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSockMinimumBuffers --
 *
 *	Ensure minimum buffer sizes (non zero).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Sets SO_SNDBUF and SO_RCVBUF sizes.
 *
 *----------------------------------------------------------------------
 */

int
TclSockMinimumBuffers(sock, size)
    int sock;			/* Socket file descriptor */
    int size;			/* Minimum buffer size */
{
    int current;
    int len;
    
    len = sizeof(int);
    getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&current, &len);
    if (current < size) {
	len = sizeof(int);
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&size, len);
    }
    len = sizeof(int);
    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&current, &len);
    if (current < size) {
	len = sizeof(int);
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&size, len);
    }
    return TCL_OK;
}
