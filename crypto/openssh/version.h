/* $OpenBSD: version.h,v 1.47 2006/08/30 00:14:37 djm Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_4.4p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20060930"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
