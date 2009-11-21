# $FreeBSD: src/secure/libexec/Makefile,v 1.16.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
