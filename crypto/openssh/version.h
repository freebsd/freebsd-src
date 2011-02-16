/* $OpenBSD: version.h,v 1.57 2010/03/07 22:01:32 djm Exp $ */
/* $FreeBSD: src/crypto/openssh/version.h,v 1.37.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.4p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20100308"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *);
#endif /* SSH_VERSION */
