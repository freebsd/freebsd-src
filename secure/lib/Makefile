# $FreeBSD$

SUBDIR= 
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto libssl
.if !defined(NO_OPENSSH)
SUBDIR+=libssh
.endif
.if defined(NO_KERBEROS)
SUBDIR+=libtelnet
.endif
.endif

.include <bsd.subdir.mk>
