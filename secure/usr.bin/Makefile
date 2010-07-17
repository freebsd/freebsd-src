# $FreeBSD: src/secure/usr.bin/Makefile,v 1.25.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSL} != "no"
SUBDIR+=bdes openssl
.if ${MK_OPENSSH} != "no"
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan
.endif
.endif

.include <bsd.subdir.mk>
