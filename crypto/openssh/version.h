/* $OpenBSD: version.h,v 1.77 2016/07/24 11:45:36 djm Exp $ */
/* $FreeBSD$ */

#define SSH_VERSION	"OpenSSH_7.3"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_FREEBSD	"FreeBSD-20170902"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION	SSLeay_version(SSLEAY_VERSION)
#else
#define OPENSSL_VERSION	"without OpenSSL"
#endif
