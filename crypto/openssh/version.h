/* $OpenBSD: version.h,v 1.33 2002/06/21 15:41:20 markus Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_3.3"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20020623"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
