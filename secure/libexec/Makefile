# $FreeBSD$

.if !defined(NO_OPENSSL) && !defined(MAKE_KERBEROS5)
SUBDIR= telnetd
.endif
.if !defined(NO_OPENSSL) && !defined(NO_OPENSSH)
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
