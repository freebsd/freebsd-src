# $FreeBSD: src/secure/libexec/Makefile,v 1.15.22.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR=
.if !defined(NO_OPENSSL)
.if !defined(NO_OPENSSH)
SUBDIR+=sftp-server ssh-keysign
.endif
.endif

.include <bsd.subdir.mk>
