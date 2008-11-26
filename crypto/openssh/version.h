/* $OpenBSD: version.h,v 1.48 2006/11/07 10:31:31 markus Exp $ */
/* $FreeBSD: src/crypto/openssh/version.h,v 1.30.2.4.6.1 2008/10/02 02:57:24 kensmith Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_4.5p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20061110"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
