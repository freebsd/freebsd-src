# $FreeBSD$

SUBDIR= libcipher
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto libssl
.if !defined(NO_OPENSSH)
SUBDIR+=libssh
.endif
.if !defined(MAKE_KERBEROS5)
SUBDIR+=libtelnet
.endif
.endif

.include <bsd.subdir.mk>
