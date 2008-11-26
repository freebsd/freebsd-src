# $FreeBSD: src/secure/lib/Makefile,v 1.33.22.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR= 
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto libssl
.if !defined(NO_OPENSSH)
SUBDIR+=libssh
.endif
.endif

.include <bsd.subdir.mk>
