# $FreeBSD$

SUBDIR= libcipher
.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(MAKE_KERBEROS4) && !defined(MAKE_KERBEROS5)
SUBDIR+=libtelnet
.endif
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto libssl
.if !defined(NO_OPENSSH)
SUBDIR+=libssh
.endif
.endif

.include <bsd.subdir.mk>
