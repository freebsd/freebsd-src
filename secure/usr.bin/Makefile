# $FreeBSD$

SUBDIR= bdes
.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(MAKE_KERBEROS4) && !defined(MAKE_KERBEROS5)
SUBDIR+=telnet
.endif
.if !defined(NO_OPENSSL)
SUBDIR+=openssl
.if !defined(NO_OPENSSH)
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan
.endif
.endif

.include <bsd.subdir.mk>
