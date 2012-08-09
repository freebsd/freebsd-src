/* $OpenBSD: version.h,v 1.62 2011/08/02 23:13:01 djm Exp $ */
/* $FreeBSD$ */

#ifndef _VERSION_H_
#define _VERSION_H_

#define SSH_VERSION_BASE	"OpenSSH_5.9p1"
#define SSH_VERSION_ADDENDUM	"FreeBSD-20111001"
#define SSH_VERSION_HPN		"_hpn13v11"

const char *ssh_version_get(int hpn_disabled);
void ssh_version_set_addendum(const char *);
#endif /* _VERSION_H_ */
