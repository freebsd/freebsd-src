/* $OpenBSD: version.h,v 1.92 2021/09/26 14:01:11 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_8.8"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20211221"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION_STRING	OpenSSL_version(OPENSSL_VERSION)
#else
#define OPENSSL_VERSION_STRING	"without OpenSSL"
#endif
