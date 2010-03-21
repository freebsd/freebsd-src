# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.6.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSH} != "no"
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
