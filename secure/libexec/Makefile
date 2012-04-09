# $FreeBSD: src/secure/libexec/Makefile,v 1.16.10.2.6.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign ssh-pkcs11-helper
.endif

.include <bsd.subdir.mk>
