/* $FreeBSD$ */
/* $OpenBSD: version.h,v 1.23 2001/04/24 16:43:16 markus Exp $ */

#ifndef	SSH_VERSION

#define SSH_VERSION		(ssh_version_get())
#define SSH_VERSION_BASE	"OpenSSH_2.9"
#define	SSH_VERSION_ADDENDUM	"FreeBSD localisations 20020307"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
