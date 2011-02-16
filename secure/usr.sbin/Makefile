# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.6.14.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSH} != "no"
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
