/* $OpenBSD: version.h,v 1.61 2011/02/04 00:44:43 djm Exp $ */
/* $FreeBSD$ */

#ifndef _VERSION_H_
#define _VERSION_H_


#define SSH_VERSION_BASE        "OpenSSH_5.8p2"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20110503"
#define SSH_VERSION_HPN		"_hpn13v11"

const char *ssh_version_get(int hpn_disabled);
void ssh_version_set_addendum(const char *);
#endif /* _VERSION_H_ */
