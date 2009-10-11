/* $OpenBSD: version.h,v 1.56 2009/06/30 14:54:40 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.3p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20091001"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *);
#endif /* SSH_VERSION */
