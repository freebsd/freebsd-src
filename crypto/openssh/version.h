/* $OpenBSD: version.h,v 1.59 2010/08/08 16:26:42 djm Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.6p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20101111"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *);
#endif /* SSH_VERSION */
