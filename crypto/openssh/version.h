/* $OpenBSD: version.h,v 1.58 2010/03/16 16:36:49 djm Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.5p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20100428"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *);
#endif /* SSH_VERSION */
