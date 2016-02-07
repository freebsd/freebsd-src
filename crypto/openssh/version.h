/* $OpenBSD: version.h,v 1.75 2015/08/21 03:45:26 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_7.1"

#define SSH_PORTABLE	"p2"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20160125"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION	"without OpenSSL"
#endif
