# $FreeBSD$

SUBDIR= libcipher
.if !defined(NOCRYPT) && !defined(MAKE_KERBEROS4)
SUBDIR+=libtelnet
.endif
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto libssl
.endif
.if !defined(NO_OPENSSH)
SUBDIR+=libssh
.endif

.include <bsd.subdir.mk>
