# $FreeBSD$

.if !defined(NOCRYPT) && !defined(MAKE_KERBEROS4) && !defined(MAKE_KERBEROS5) && !defined(NO_OPENSSL)
SUBDIR= telnetd
.endif
.if !defined(NO_OPENSSL) && !defined(NO_OPENSSH)
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
