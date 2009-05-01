# $FreeBSD: src/secure/libexec/Makefile,v 1.16.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
