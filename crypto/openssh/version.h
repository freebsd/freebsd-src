/* $OpenBSD: version.h,v 1.37 2003/04/01 10:56:46 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.6.1p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20030917"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */

