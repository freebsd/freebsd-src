# $FreeBSD$

SUBDIR= bdes
.if !defined(NO_OPENSSL)
SUBDIR+=openssl
.if !defined(NO_OPENSSH)
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan
.endif
.if defined(NO_KERBEROS)
SUBDIR+=telnet
.endif
.endif

.include <bsd.subdir.mk>
