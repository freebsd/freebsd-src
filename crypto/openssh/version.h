/* $OpenBSD: version.h,v 1.57 2010/03/07 22:01:32 djm Exp $ */
/* $FreeBSD$ */

#ifndef _VERSION_H_
#define _VERSION_H_

#define SSH_VERSION_BASE        "OpenSSH_5.4p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20100308"
#define SSH_VERSION_HPN		"_hpn13v11"

const char *ssh_version_get(int hpn_disabled);
void ssh_version_set_addendum(const char *);
#endif /* _VERSION_H_ */
