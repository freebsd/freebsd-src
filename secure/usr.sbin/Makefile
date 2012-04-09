# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.6.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSH} != "no"
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
