/* $FreeBSD$ */
/* $OpenBSD: version.h,v 1.13 2000/10/16 09:38:45 djm Exp $ */

#ifndef	SSH_VERSION

#define SSH_VERSION		(ssh_version_get())
#define SSH_VERSION_BASE	"OpenSSH_2.3.0"
#define	SSH_VERSION_ADDENDUM	"green@FreeBSD.org 20010319"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
