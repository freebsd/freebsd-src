/* $OpenBSD: version.h,v 1.39 2003/09/16 21:02:40 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.7.1p2"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20040106"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
