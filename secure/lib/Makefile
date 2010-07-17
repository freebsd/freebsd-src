# $FreeBSD: src/secure/lib/Makefile,v 1.34.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSL} != "no"
SUBDIR+=libcrypto libssl
.if ${MK_OPENSSH} != "no"
SUBDIR+=libssh
.endif
.endif

.include <bsd.subdir.mk>
