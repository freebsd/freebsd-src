/* $OpenBSD: version.h,v 1.34 2002/06/26 13:56:27 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.4p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20030924"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */

