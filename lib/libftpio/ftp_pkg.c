/*
 * Copyright (c)1995, 1996 Jordan Hubbard
 *
 * All rights reserved.
 *
 * This source code may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of the software nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 * $Id: ftp_pkg.c,v 1.1.1.1 1996/06/17 12:26:06 jkh Exp $
 *
 * TCL Interface code for functions provided by the ftp library.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <ftpio.h>
#include "ftp_pkg.h"

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

#define CHECK_ARGS(cnt, myname, str) \
if (argc <= (cnt)) { sprintf(interp->result, "usage: %s %s", myname, str); return TCL_ERROR; }

#define USAGE(myname, msg) \
{ fprintf(stderr, "%s: %s\n", myname, msg); return TCL_ERROR; }


/* Registration function */
int
Ftp_Init(Tcl_Interp *interp)
{
    Tcl_CreateCommand (interp, "ftp_login",	Ftp_login,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_chdir",	Ftp_chdir,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_getsize",	Ftp_getsize,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_get",	Ftp_get,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_put",	Ftp_put,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_binary",	Ftp_binary,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_passive",	Ftp_passive,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_get_url",	Ftp_get_url,		NULL, NULL);
    Tcl_CreateCommand (interp, "ftp_put_url",	Ftp_put_url,		NULL, NULL);
    return TCL_OK;
}

/*
 * ftp_login  host user passwd port
 *  -- returns new fileId
 * 
 */
int
Ftp_login(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;
    char *user, *pass;
    int port;

    CHECK_ARGS(1, argv[0], "host [user] [passwd] [port]");

    user = (argc > 2) ? argv[2] : "ftp";
    pass = (argc > 3) ? argv[3] : "setup@";
    port = (argc > 4) ? atoi(argv[4]) : 21;
    /* Debug("ftp_pkg: attempt login to host %s using %s/%s (port %d)", argv[1], user, pass, port); */
    fp = ftpLogin(argv[1], user, pass, port);
    if (fp) {
	/* Debug("ftp_pkg: logged successfully into host %s", argv[1]); */
	Tcl_EnterFile(interp, fp, TCL_FILE_READABLE | TCL_FILE_WRITABLE);
	return TCL_OK;
    }
    /* Debug("ftp_pkg: login operation failed for host %s", argv[1]); */
    return TCL_ERROR;
}

/*
 * ftp_chdir  file-handle newdir
 *  -- returns status
 */
int
Ftp_chdir(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;

    CHECK_ARGS(2, argv[0], "fileId directory");
    if (Tcl_GetOpenFile(interp, argv[1], TRUE, TRUE, &fp) != TCL_OK)
	return TCL_ERROR;
    /* Debug("ftp_pkg: attempt chdir to dir %s", argv[2]); */
    if (!ftpChdir(fp, argv[2])) {
	/* Debug("ftp_pkg: chdir successful"); */
	return TCL_OK;
    }
    /* Debug("ftp_pkg: chdir failed"); */
    return TCL_ERROR;
}

/*
 * ftp_getsize  file-handle filename
 *  -- returns size
 */
int
Ftp_getsize(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;
    int sz;

    CHECK_ARGS(2, argv[0], "fileId filename");
    if (Tcl_GetOpenFile(interp, argv[1], TRUE, TRUE, &fp) != TCL_OK)
	return TCL_ERROR;
    /* Debug("ftp_pkg: attempt to get size of %s", argv[2]); */
    if ((sz = ftpGetSize(fp, argv[2])) >= 0) {
	/* Debug("ftp_pkg: getsize successful (%d)", sz); */
	sprintf(interp->result, "%d", sz);
	return TCL_OK;
    }
    /* Debug("ftp_pkg: chdir failed"); */
    return TCL_ERROR;
}

/*
 * ftp_get  fileId filename
 *  -- returns new fileId for filename
 * 
 */
int
Ftp_get(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp, *fp2;

    CHECK_ARGS(2, argv[0], "fileId filename");
    if (Tcl_GetOpenFile(interp, argv[1], TRUE, TRUE, &fp) != TCL_OK)
	return TCL_ERROR;
    /* Debug("ftp_pkg: attempt to get file %s", argv[2]); */
    fp2 = ftpGet(fp, argv[2]);
    if (fp2) {
	/* Debug("ftp_pkg: get operation successful for: %s", argv[2]); */
	Tcl_EnterFile(interp, fp2, TCL_FILE_READABLE);
	return TCL_OK;
    }
    /* Debug("ftp_pkg: get operation failed for file %s", argv[2]); */
    return TCL_ERROR;
}

/*
 * ftp_put  fileId filename
 *  -- returns new fileId for filename
 * 
 */
int
Ftp_put(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp, *fp2;

    CHECK_ARGS(2, argv[0], "fileId filename");
    if (Tcl_GetOpenFile(interp, argv[1], TRUE, TRUE, &fp) != TCL_OK)
	return TCL_ERROR;
    /* Debug("ftp_pkg: attempt to put file %s", argv[2]); */
    fp2 = ftpPut(fp, argv[2]);
    if (fp2) {
	/* Debug("ftp_pkg: put operation successful for: %s", argv[2]); */
	Tcl_EnterFile(interp, fp2, TCL_FILE_READABLE);
	return TCL_OK;
    }
    /* Debug("ftp_pkg: put operation failed for file %s", argv[2]); */
    return TCL_ERROR;
}

/*
 * ftp_binary  fileId value
 *  -- Set binary mode to truth value for FTP session represented by fileId
 * 
 */
int
Ftp_binary(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;

    CHECK_ARGS(1, argv[0], "fileId");
    if (Tcl_GetOpenFile(interp, argv[1], TRUE, TRUE, &fp) != TCL_OK)
	return TCL_ERROR;
    /* Debug("ftp_pkg: set binary mode"); */
    ftpBinary(fp);
    return TCL_OK;
}

/*
 * ftp_passive  fileId value
 *  -- Set passive mode to truth value for FTP session represented by fileId
 * 
 */
int
Ftp_passive(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;

    CHECK_ARGS(2, argv[0], "fileId bool");
    if (Tcl_GetOpenFile(interp, argv[1], TRUE, TRUE, &fp) != TCL_OK)
	return TCL_ERROR;
    /* Debug("ftp_pkg: set passive mode to %d", atoi(argv[2])); */
    ftpPassive(fp, atoi(argv[2]));
    return TCL_OK;
}

/*
 * ftp_get_url  URL user pass
 *  -- Return new fileId for open URL (using user and pass to log in)
 * 
 */
int
Ftp_get_url(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;
    char *user, *pass;

    CHECK_ARGS(1, argv[0], "URL [username] [password]");
    user = (argc > 2) ? argv[2] : "ftp";
    pass = (argc > 3) ? argv[3] : "setup@";
    /* Debug("ftp_pkg: attempt to get URL %s as %s/%s", argv[1], user, pass); */
    fp = ftpGetURL(argv[1], user, pass);
    if (fp) {
	/* Debug("ftp_pkg: get URL successful"); */
	Tcl_EnterFile(interp, fp, TCL_FILE_READABLE);
	return TCL_OK;
    }
    /* Debug("ftp_pkg: get URL failed"); */
    return TCL_ERROR;
}

/*
 * ftp_put_url  URL user pass
 *  -- Return new fileId for open url (using user and pass to log in)
 * 
 */
int
Ftp_put_url(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;
    char *user, *pass;

    CHECK_ARGS(1, argv[0], "URL [username] [password]");
    user = (argc > 2) ? argv[2] : "ftp";
    pass = (argc > 3) ? argv[3] : "setup@";
    /* Debug("ftp_pkg: attempt to put URL %s as %s/%s", argv[1], user, pass); */
    fp = ftpPutURL(argv[1], user, pass);
    if (fp) {
	/* Debug("ftp_pkg: put URL successful"); */
	Tcl_EnterFile(interp, fp, TCL_FILE_READABLE);
	return TCL_OK;
    }
    /* Debug("ftp_pkg: put URL failed"); */
    return TCL_ERROR;
}
