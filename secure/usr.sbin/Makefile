# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.5.26.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR= 
.if !defined(NO_OPENSSH) && !defined(NO_OPENSSL)
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
