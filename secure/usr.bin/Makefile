# $FreeBSD$

SUBDIR= bdes
.if !defined(NO_OPENSSL)
SUBDIR+=openssl
.if !defined(NO_OPENSSH)
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan
.endif
.if !defined(MAKE_KERBEROS5)
SUBDIR+=telnet
.endif
.endif

.include <bsd.subdir.mk>
