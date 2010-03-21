# $FreeBSD: src/secure/lib/Makefile,v 1.34.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSL} != "no"
SUBDIR+=libcrypto libssl
.if ${MK_OPENSSH} != "no"
SUBDIR+=libssh
.endif
.endif

.include <bsd.subdir.mk>
