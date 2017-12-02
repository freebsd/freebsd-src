# $FreeBSD$
# libgeliboot

MAN=
DO32=1

.include <bsd.init.mk>
MK_SSP=		no

LIB=		geliboot
INTERNALLIB=
MK_PROFILE=	no
NO_PIC=

WARNS?=		0

# Our password input method
SRCS+=  pwgets.c

# sha256 and sha512 from sys/crypto
.PATH: ${SYSDIR}/crypto/sha2
CFLAGS+=	-DWEAK_REFS
SRCS+=		sha256c.c sha512c.c

# md5 from libmd
.PATH: ${SRCTOP}/lib/libmd
SRCS+=		md5c.c

# AES implementation from sys/crypto
.PATH: ${SYSDIR}/crypto/rijndael
CFLAGS+=	-I${LDRSRC}
# Remove asserts
CFLAGS+=	-DNDEBUG
SRCS+=		rijndael-alg-fst.c rijndael-api-fst.c rijndael-api.c

# local GELI Implementation
.PATH: ${SYSDIR}/geom/eli
SRCS+=		geliboot_crypto.c g_eli_hmac.c g_eli_key.c g_eli_key_cache.c pkcs5v2.c

# aes
.PATH:		${SYSDIR}/opencrypto
SRCS+=		xform_aes_xts.c

.include <bsd.stand.mk>
.include <bsd.lib.mk>
