/* $OpenBSD: version.h,v 1.42 2004/08/16 08:17:01 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.9p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20041028"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
