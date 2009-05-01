# $FreeBSD: src/secure/lib/Makefile,v 1.34.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSL} != "no"
SUBDIR+=libcrypto libssl
.if ${MK_OPENSSH} != "no"
SUBDIR+=libssh
.endif
.endif

.include <bsd.subdir.mk>
