/* $OpenBSD: version.h,v 1.57 2010/03/07 22:01:32 djm Exp $ */
/* $FreeBSD$ */

#ifndef SSH_VERSION

#define SSH_VERSION_BASE        "OpenSSH_5.4p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20100308"
#define SSH_VERSION_HPN		"_hpn13v11"
#define SSH_VERSION		SSH_VERSION_BASE SSH_VERSION_HPN " " SSH_VERSION_ADDENDUM
#define SSH_RELEASE             (ssh_version_get())

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *);
#endif /* SSH_VERSION */
