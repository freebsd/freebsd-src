/* $OpenBSD: version.h,v 1.80 2017/09/30 22:26:33 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_7.6"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20180507"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION	"without OpenSSL"
#endif
