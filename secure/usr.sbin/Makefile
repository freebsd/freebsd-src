# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSH} != "no"
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
