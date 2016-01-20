/* $OpenBSD: version.h,v 1.74 2015/08/02 09:56:42 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_7.0"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20160119"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION	"without OpenSSL"
#endif
