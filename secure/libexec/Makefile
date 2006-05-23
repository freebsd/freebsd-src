# $FreeBSD: src/secure/libexec/Makefile,v 1.15 2003/07/16 20:59:15 markm Exp $

SUBDIR=
.if !defined(NO_OPENSSL)
.if !defined(NO_OPENSSH)
SUBDIR+=sftp-server ssh-keysign
.endif
.endif

.include <bsd.subdir.mk>
