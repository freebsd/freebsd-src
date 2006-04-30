# $FreeBSD$

SUBDIR=
.if !defined(NO_OPENSSL)
SUBDIR+=bdes openssl
.if !defined(NO_OPENSSH)
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan
.endif
.endif

.include <bsd.subdir.mk>
