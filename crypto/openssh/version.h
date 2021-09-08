/* $OpenBSD: version.h,v 1.91 2021/08/20 03:22:55 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_8.7"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20210907"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION_STRING	OpenSSL_version(OPENSSL_VERSION)
#else
#define OPENSSL_VERSION_STRING	"without OpenSSL"
#endif
