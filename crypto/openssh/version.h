/* $OpenBSD: version.h,v 1.79 2017/03/20 01:18:59 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_7.5"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20170903"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION_STRING	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION_STRING	"without OpenSSL"
#endif
