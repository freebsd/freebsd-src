# $FreeBSD$

SUBDIR=
.if !defined(NO_OPENSSL)
.if !defined(NO_OPENSSH)
SUBDIR+=sftp-server ssh-keysign
.endif
.endif

.include <bsd.subdir.mk>
