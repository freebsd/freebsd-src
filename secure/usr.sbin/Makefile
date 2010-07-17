# $FreeBSD: src/secure/usr.sbin/Makefile,v 1.6.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSH} != "no"
SUBDIR+=sshd
.endif

.include <bsd.subdir.mk>
