# $FreeBSD$

SUBDIR= libcipher libdes
.if !defined(NOCRYPT) && !defined(MAKE_KERBEROS4)
SUBDIR+=libtelnet
.endif
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto librsaglue libssl
.endif

.include <bsd.subdir.mk>
