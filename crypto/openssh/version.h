/* $OpenBSD: version.h,v 1.81 2018/03/24 19:29:03 markus Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_7.7"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20180510"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION	"without OpenSSL"
#endif
