# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSH} != "no"
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
