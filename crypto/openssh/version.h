/* $OpenBSD: version.h,v 1.35 2002/10/01 13:24:50 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.5p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20030201"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */

