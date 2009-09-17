/* $OpenBSD: version.h,v 1.55 2009/02/23 00:06:15 djm Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.2p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20090522"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *);
#endif /* SSH_VERSION */
