/* $OpenBSD: version.h,v 1.28 2002/03/06 00:25:55 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.1"
#define SSH_VERSION_ADDENDUM    "FreeBSD localisations 20020318"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
