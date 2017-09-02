/* $OpenBSD: version.h,v 1.78 2016/12/19 04:55:51 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_7.4"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20170902"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION	"without OpenSSL"
#endif
