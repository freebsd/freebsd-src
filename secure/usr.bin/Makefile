# $FreeBSD: src/secure/usr.bin/Makefile,v 1.24.22.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR=
.if !defined(NO_OPENSSL)
SUBDIR+=bdes openssl
.if !defined(NO_OPENSSH)
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan
.endif
.endif

.include <bsd.subdir.mk>
