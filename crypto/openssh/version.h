/* $FreeBSD$ */
/* $OpenBSD: version.h,v 1.40 2004/02/23 15:16:46 markus Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.8.1p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20040419"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
