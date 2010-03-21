# $FreeBSD: src/secure/libexec/Makefile,v 1.16.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
