# $FreeBSD: src/secure/lib/Makefile,v 1.33 2003/07/16 20:59:14 markm Exp $

SUBDIR= 
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto libssl
.if !defined(NO_OPENSSH)
SUBDIR+=libssh
.endif
.endif

.include <bsd.subdir.mk>
