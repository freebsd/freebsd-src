# $FreeBSD: src/secure/libexec/Makefile,v 1.16.14.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
