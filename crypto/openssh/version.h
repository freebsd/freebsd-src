/* $OpenBSD: version.h,v 1.93 2022/02/23 11:07:09 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_9.0"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20220415"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION_STRING	OpenSSL_version(OPENSSL_VERSION)
#else
#define OPENSSL_VERSION_STRING	"without OpenSSL"
#endif
