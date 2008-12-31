# $FreeBSD: src/secure/libexec/Makefile,v 1.16.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
