/* $OpenBSD: version.h,v 1.54 2008/07/21 08:19:07 djm Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.1p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20080801"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
