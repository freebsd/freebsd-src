/* $OpenBSD: version.h,v 1.44 2005/03/16 21:17:39 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_4.1p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20041028"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
