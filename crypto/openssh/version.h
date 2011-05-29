/* $OpenBSD: version.h,v 1.61 2011/02/04 00:44:43 djm Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.8p2"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20110503"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *);
#endif /* SSH_VERSION */
