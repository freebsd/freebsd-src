# $FreeBSD$

SUBDIR= libcipher
.if !defined(NOCRYPT) && !defined(MAKE_KERBEROS4)
SUBDIR+=libtelnet
.endif
.if !defined(NO_OPENSSL)
SUBDIR+=libcrypto librsaglue libssl
.if exists(${.CURDIR}/../../crypto/openssl/rsaref)
SUBDIR+=librsausa
.endif
.if exists(${.CURDIR}/../../crypto/openssl/crypto/rsa/rsa_eay.c) && \
    !defined(NO_RSAINTL)
SUBDIR+=librsaintl
.endif
.endif
.if !defined(NO_OPENSSH)
SUBDIR+=libssh
.endif

.include <bsd.subdir.mk>
