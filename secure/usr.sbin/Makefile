# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.5 2000/10/24 16:04:51 gshapiro Exp $

SUBDIR= 
.if !defined(NO_OPENSSH) && !defined(NO_OPENSSL)
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
