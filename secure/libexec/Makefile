# $FreeBSD: src/secure/libexec/Makefile,v 1.16.10.2.2.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign ssh-pkcs11-helper
.endif

.include <bsd.subdir.mk>
