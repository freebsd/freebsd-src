# $FreeBSD$

SUBDIR=
.if !defined(NO_OPENSSL)
.if !defined(NO_OPENSSH)
SUBDIR+=sftp-server ssh-keysign
.endif
.if defined(NO_KERBEROS)
SUBDIR+=telnetd
.endif
.endif

.include <bsd.subdir.mk>
