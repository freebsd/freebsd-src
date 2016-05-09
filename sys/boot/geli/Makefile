# $FreeBSD$
# libgeliboot

MAN=

.include <src.opts.mk>
MK_SSP=		no

LIB=		geliboot
INTERNALLIB=
MK_PROFILE=	no
NO_PIC=

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=        -march=i386
.endif
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=        -m32
.endif

WARNS?=		0

# string functions from libc
.PATH: ${.CURDIR}/../../../lib/libc/string
SRCS+=  bcmp.c bcopy.c bzero.c

# Our password input method
SRCS+=  pwgets.c

# sha256 and sha512 from sys/crypto
.PATH: ${.CURDIR}/../../crypto/sha2
CFLAGS+=	-DWEAK_REFS
SRCS+=		sha256c.c sha512c.c

# md5 from libmd
.PATH: ${.CURDIR}/../../../lib/libmd
SRCS+=		md5c.c

# AES implementation from sys/crypto
.PATH: ${.CURDIR}/../../crypto/rijndael
CFLAGS+=	-I${.CURDIR}/../../
# Remove asserts
CFLAGS+=	-DNDEBUG
SRCS+=		rijndael-alg-fst.c rijndael-api-fst.c rijndael-api.c

# local GELI Implementation
.PATH: ${.CURDIR}/../../geom/eli
CFLAGS+=	-D_STAND
SRCS+=		geliboot_crypto.c g_eli_hmac.c g_eli_key.c g_eli_key_cache.c pkcs5v2.c

.include <bsd.stand.mk>
.include <bsd.lib.mk>
