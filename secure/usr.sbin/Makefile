# $FreeBSD$

SUBDIR= 
.if !defined(NO_OPENSSH) && !defined(NO_OPENSSL)
SUBDIR+=sshd
.endif

.if !defined(NO_SENDMAIL) && !defined(NO_OPENSSL)
SUBDIR+=sendmail
.endif

.include <bsd.subdir.mk>
