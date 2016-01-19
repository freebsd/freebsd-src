/* $OpenBSD: version.h,v 1.72 2015/03/04 18:53:53 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_6.8"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20160119"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION	"without OpenSSL"
#endif
