# $FreeBSD: src/secure/libexec/Makefile,v 1.16 2006/03/17 18:54:32 ru Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
