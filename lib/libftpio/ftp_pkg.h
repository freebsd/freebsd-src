#ifndef _FTP_PKG_H
#define _FTP_PKG_H

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
 * $Id$
 *
 * TCL Interface code for functions provided by the ftp library.
 */

#include <tcl.h>
#include <ftpio.h>

extern int	Ftp_login (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_chdir (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_getsize (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_get (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_put (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_binary (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_passive (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_get_url (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int	Ftp_put_url (ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

#endif	/* _FTP_PKG_H */
